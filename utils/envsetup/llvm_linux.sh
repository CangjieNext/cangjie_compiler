#!/usr/bin/env sh

# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# This source file is part of the Cangjie project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

# This script needs to be placed in the output directory of Cangjie compiler.
# ** NOTE: Please use `source' command to execute this script. **

_cj_return_or_exit() {
    code="$1"
    (return 0 2>/dev/null) && return "$code"
    exit "$code"
}

_cj_detect_shell_name() {
    # Prefer shell-provided variables when sourced.
    if [ -n "${ZSH_VERSION:-}" ]; then
        printf '%s\n' "zsh"
        return 0
    fi
    if [ -n "${BASH_VERSION:-}" ]; then
        printf '%s\n' "bash"
        return 0
    fi

    # Fallback: process name. In some container setups /proc/$$/exe can be misleading
    # (e.g. rosetta), so we avoid it.
    if command -v ps >/dev/null 2>&1; then
        comm="$(ps -o comm= -p "$$" 2>/dev/null | tr -d '\n')"
        comm="${comm##*/}"
        case "$comm" in
            -*) comm="${comm#-}" ;;
        esac
        case "$comm" in
            zsh|bash) printf '%s\n' "$comm"; return 0 ;;
            sh|dash|ash|busybox) printf '%s\n' "sh"; return 0 ;;
        esac
    fi

    if [ -r "/proc/$$/comm" ]; then
        comm="$(tr -d '\n' < "/proc/$$/comm" 2>/dev/null)"
        comm="${comm##*/}"
        case "$comm" in
            -*) comm="${comm#-}" ;;
        esac
        case "$comm" in
            zsh|bash) printf '%s\n' "$comm"; return 0 ;;
            sh|dash|ash|busybox) printf '%s\n' "sh"; return 0 ;;
        esac
    fi

    printf '%s\n' "unknown"
    return 0
}

_cj_resolve_abs_path() {
    in="$1"
    [ -n "$in" ] || return 1

    case "$in" in
        /*) p="$in" ;;
        *) p="$PWD/$in" ;;
    esac

    if command -v realpath >/dev/null 2>&1; then
        realpath "$p" 2>/dev/null && return 0
    fi
    if command -v readlink >/dev/null 2>&1; then
        readlink -f "$p" 2>/dev/null && return 0
    fi
    if command -v python3 >/dev/null 2>&1; then
        python3 -c 'import os,sys; print(os.path.realpath(sys.argv[1]))' "$p" 2>/dev/null && return 0
    fi
    if command -v python >/dev/null 2>&1; then
        python -c 'import os,sys; print(os.path.realpath(sys.argv[1]))' "$p" 2>/dev/null && return 0
    fi

    d="${p%/*}"
    b="${p##*/}"
    (cd -P "$d" 2>/dev/null && printf '%s/%s\n' "$(pwd -P)" "$b") || return 1
}

_cj_script_dir_from_path() {
    p="$1"
    [ -n "$p" ] || return 1

    abs="$(_cj_resolve_abs_path "$p" 2>/dev/null || true)"
    [ -n "$abs" ] || abs="$p"

    d="${abs%/*}"
    [ -n "$d" ] || d="."
    (cd -P "$d" 2>/dev/null && pwd -P) || return 1
}

shell_name="$(_cj_detect_shell_name)"
case "$shell_name" in
    zsh)
        # ${(%):-%N} is zsh-only; keep it in eval so sh can parse this file.
        script_path="$(eval 'printf "%s\n" "${(%):-%N}"' 2>/dev/null)"
        script_dir="$(_cj_script_dir_from_path "$script_path" 2>/dev/null || true)"
        if [ -z "$script_dir" ]; then
            echo "[ERROR] Failed to locate script directory in zsh." >&2
            _cj_return_or_exit 1
        fi
        # zsh completion setup (run only in zsh; zsh-only syntax stays in eval).
        eval '
            if (( ${+_comps} )); then
                compdef -d cjc cjc-frontend
            else
                autoload -Uz compinit
                compinit
            fi
            compdef _gnu_generic cjc cjc-frontend
        ' 2>/dev/null || true
        ;;
    bash)
        # shellcheck disable=SC3054
        script_dir="$(_cj_script_dir_from_path "${BASH_SOURCE[0]}" 2>/dev/null || true)"
        if [ -z "$script_dir" ]; then
            echo "[ERROR] Failed to locate script directory in bash." >&2
            _cj_return_or_exit 1
        fi
        ;;
    sh)
        # POSIX sh (dash/ash/busybox sh) cannot reliably know the sourced filename.
        # Require running from the directory containing envsetup.sh.
        if [ -f "envsetup.sh" ]; then
            script_dir="$(pwd -P 2>/dev/null || pwd)"
        else
            echo "[ERROR] Switch to the directory containing envsetup.sh and run it." >&2
            _cj_return_or_exit 1
        fi
        ;;
    *)
        echo "[ERROR] Unsupported shell: ${shell_name}, please switch to bash, sh or zsh." >&2
        _cj_return_or_exit 1
        ;;
esac

sys_name=$(uname -s)
if [ "$sys_name" = "HarmonyOS" ]; then
    sys_name="_ohos"
else
    sys_name=""
fi
hw_arch=$(uname -m)
if [ "$hw_arch" = "" ]; then
    hw_arch="x86_64"
fi
hw_arch="${sys_name}_${hw_arch}"

export CANGJIE_HOME="${script_dir}"
export PATH="${CANGJIE_HOME}/bin:${CANGJIE_HOME}/tools/bin${PATH:+:${PATH}}:${HOME}/.cjpm/bin"
export LD_LIBRARY_PATH="${CANGJIE_HOME}/runtime/lib/linux${hw_arch}_cjnative:${CANGJIE_HOME}/tools/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
unset hw_arch
unset sys_name
