# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# This source file is part of the Cangjie project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

# This script needs to be placed in the output directory of Cangjie compiler.
# ** NOTE: Please use `source' command to execute this script. **
if ! (return 0 2>/dev/null); then
    echo "[ERROR] This script must be sourced, not executed directly."
    echo "        Please run: 'source $0' or '. $0'"
    exit 1
fi

# Detect the current shell type
_cj_detect_shell_name() {
    # Prefer shell-provided version variables when sourced
    if [ -n "${ZSH_VERSION:-}" ]; then
        printf '%s\n' "zsh"
        return 0
    fi
    if [ -n "${BASH_VERSION:-}" ]; then
        printf '%s\n' "bash"
        return 0
    fi

    # Fallback: detect by process name. In some container setups /proc/$$/exe can be misleading
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

    # If ps is not available, try reading from /proc filesystem
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

    # Unrecognized shell
    printf '%s\n' "unknown"
    return 0
}

# Resolve relative path to absolute path
_cj_resolve_abs_path() {
    input="$1"
    [ -n "$input" ] || return 1

    # Handle relative and absolute paths
    case "${input}" in
        /*) p="$input" ;;
        *) p="$PWD/$input" ;;
    esac

    # Try multiple tools to resolve absolute path, in order of preference:
    # 1. realpath
    # 2. readlink -f
    # 3. python3
    # 4. python
    # 5. pure shell implementation
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

    # Pure shell implementation: cd to directory then pwd
    d="${p%/*}"
    b="${p##*/}"
    (cd -P "$d" 2>/dev/null && printf '%s/%s\n' "$(pwd -P)" "$b") || return 1
}

# Get absolute path of the script directory from the script path
_cj_script_dir_from_path() {
    p="$1"
    [ -n "$p" ] || return 1

    # First resolve to absolute path
    abs="$(_cj_resolve_abs_path "$p" 2>/dev/null || true)"
    [ -n "$abs" ] || abs="$p"

    # Get directory part
    d="${abs%/*}"
    [ -n "$d" ] || d="."
    # Change to directory and output absolute path
    (cd -P "$d" 2>/dev/null && pwd -P) || return 1
}

# ==================== MAIN LOGIC ====================

# Detect current shell type
shell_name="$(_cj_detect_shell_name)"

# Get script directory based on different shell types
case "$shell_name" in
    zsh)
        # zsh completion setup (zsh-only syntax stays in eval for POSIX sh compatibility)
        eval '
            if (( ${+_comps} )); then
                compdef -d cjc cjc-frontend
            else
                autoload -Uz compinit
                compinit
            fi
            compdef _gnu_generic cjc cjc-frontend
        ' 2>/dev/null || true

        # ${(%):-%N} is zsh-specific syntax to get current script path
        # Note: This is intentionally not wrapped in eval to get the correct script path
        # This relies on the fact that most modern shells won't fail at parse time for
        # unknown parameter expansions when they're in an unexecuted code branch
        script_dir="$(_cj_script_dir_from_path "${(%):-%N}" 2>/dev/null || true)"
        if [ -z "$script_dir" ]; then
            echo "[ERROR] Failed to locate script directory in zsh." >&2
            return 1
        fi
        ;;
    bash)
        # shellcheck disable=SC3054
        # ${BASH_SOURCE[0]} is bash-specific syntax to get current script path
        script_dir="$(_cj_script_dir_from_path "${BASH_SOURCE[0]}" 2>/dev/null || true)"
        if [ -z "$script_dir" ]; then
            echo "[ERROR] Failed to locate script directory in bash." >&2
            return 1
        fi
        ;;
    sh)
        # Check if script is executed directly (not sourced)
        case "$0" in
            *"envsetup.sh")
                echo "[ERROR] This script must be sourced, not executed directly."
                echo "        Please run: 'source $0' or '. $0'"
                return 1
                ;;
        esac
        # POSIX sh (dash/ash/busybox sh) cannot reliably know the sourced filename.
        # Require running from the directory containing envsetup.sh.
        if [ -f "envsetup.sh" ]; then
            script_dir="$(pwd -P 2>/dev/null || pwd)"
        else
            echo "[ERROR] Switch to the directory containing envsetup.sh and run it." >&2
            return 1
        fi
        ;;
    *)
        echo "[ERROR] Unsupported shell: ${shell_name}, please switch to bash, sh or zsh." >&2
        return 1
        ;;
esac

# ==================== ENVIRONMENT VARIABLE CONFIGURATION ====================

# Detect system name
sys_name=$(uname -s)
if [ "$sys_name" = "HarmonyOS" ]; then
    sys_name="_ohos"
else
    sys_name=""
fi

# Detect hardware architecture
hw_arch=$(uname -m)
if [ "$hw_arch" = "" ]; then
    hw_arch="x86_64"
fi
hw_arch="${sys_name}_${hw_arch}"

# Remove Cangjie-related paths from an environment variable
_remove_cangjie_paths() {
    env_var_name="$1"
    env_var_value=""

    # Read environment variable by name in a way that works in both bash and zsh.
    eval "env_var_value=\${$env_var_name}"

    if [ -n "${env_var_value}" ]; then
        # IMPORTANT: use explicit ':' splitting with parameter expansion.
        # Do not rely on shell word-splitting behavior, which differs across sh/bash/zsh
        item=""
        new_env_var_value=""
        remaining="${env_var_value}"
        while :; do
            case "${remaining}" in
                *:*)
                    item="${remaining%%:*}"
                    remaining="${remaining#*:}"
                    ;;
                *)
                    item="${remaining}"
                    remaining=""
                    ;;
            esac
            case "${item}" in
                "${CANGJIE_HOME}/"*) ;;
                "${HOME}/.cjpm/bin") ;;
                *)
                    new_env_var_value="${new_env_var_value}${new_env_var_value:+:}${item}"
                    ;;
            esac
            [ -n "${remaining}" ] || break
        done
        eval "export ${env_var_name}=\"\${new_env_var_value}\""
    fi
}
if [ -n "${CANGJIE_HOME-}" ]; then
    _remove_cangjie_paths "PATH"
    _remove_cangjie_paths "LD_LIBRARY_PATH"
fi

# Set Cangjie related environment variables
export CANGJIE_HOME="${script_dir}"
export PATH="${CANGJIE_HOME}/bin:${CANGJIE_HOME}/tools/bin${PATH:+:${PATH}}:${HOME}/.cjpm/bin"
export LD_LIBRARY_PATH="${CANGJIE_HOME}/runtime/lib/linux${hw_arch}_cjnative:${CANGJIE_HOME}/tools/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"

# ==================== HARMONYOS CODE SIGNING ====================
# HarmonyOS (PC) refuses to execute or dlopen an ELF that lives in a
# user-writable directory unless it carries a valid `.codesign` section: execve
# fails with EACCES ("Permission denied") even though the file mode is 0755 and
# its interpreter exists. Both the SDK binaries and everything produced by
# cjc/cjpm therefore have to be self-signed on such a host.
# This section signs the SDK binaries once and exports `cjsign` for build
# outputs; it is a no-op on every other platform.
if [ "$sys_name" = "_ohos" ]; then

    # `binary-sign-tool` ships with the OpenHarmony / HarmonyOS SDK rather than
    # with the Cangjie SDK, so look for it in the usual places.
    _cj_find_sign_tool() {
        for _p in \
            "${OHOS_BINARY_SIGN_TOOL:-}" \
            "${DEVECO_SDK_HOME:-}/default/openharmony/toolchains/lib/binary-sign-tool" \
            "${OHOS_SDK_HOME:-}/default/openharmony/toolchains/lib/binary-sign-tool" \
            "${HMOS_CLT_HOME:-}/sdk/default/openharmony/toolchains/lib/binary-sign-tool"
        do
            if [ -n "$_p" ] && [ -x "$_p" ]; then
                printf '%s\n' "$_p"
                return 0
            fi
        done
        _p="$(command -v binary-sign-tool 2>/dev/null)"
        if [ -n "$_p" ]; then
            printf '%s\n' "$_p"
            return 0
        fi
        return 1
    }

    # True when the file already carries a `.codesign` section. The search runs
    # on a NUL-stripped stream: matching a binary directly with grep is not
    # portable, and returns no match even for a signed file on HarmonyOS.
    # The section header table ends an ELF, so the tail covers the common case;
    # the whole file is only read when the tail is inconclusive.
    _cj_is_signed() {
        if tail -c 65536 "$1" 2>/dev/null | tr -d '\000' | grep -q '\.codesign' 2>/dev/null; then
            return 0
        fi
        tr -d '\000' < "$1" 2>/dev/null | grep -q '\.codesign' 2>/dev/null
    }

    # Self-sign one file if it needs it. Relocatable objects (.o) are skipped:
    # only executables (e_type 2) and shared objects (e_type 3) are verified by
    # the kernel. Signing is idempotent: binary-sign-tool replaces an existing
    # `.codesign` section instead of appending a second one.
    _cj_sign_file() {
        _f="$1"
        [ -f "$_f" ] || return 0
        [ "$(od -An -tx1 -N4 "$_f" 2>/dev/null | tr -d ' \n')" = "7f454c46" ] || return 0
        case "$(od -An -tu2 -j16 -N2 "$_f" 2>/dev/null | tr -d ' \n')" in
            2|3) ;;
            *) return 0 ;;
        esac
        if _cj_is_signed "$_f"; then
            return 0
        fi
        # Files that have been executed are sealed by HMFS and cannot be written
        # in place, so fall back to signing a temporary file and renaming it.
        if "$_cj_sign_tool" sign -selfSign 1 -inFile "$_f" -outFile "$_f" >/dev/null 2>&1; then
            [ -n "${_cj_sign_quiet:-}" ] || printf '%s\n' "  signed $_f"
            return 0
        fi
        _t="$_f.codesign.tmp"
        if "$_cj_sign_tool" sign -selfSign 1 -inFile "$_f" -outFile "$_t" >/dev/null 2>&1 && mv -f "$_t" "$_f"; then
            [ -n "${_cj_sign_quiet:-}" ] || printf '%s\n' "  signed $_f"
            return 0
        fi
        rm -f "$_t" 2>/dev/null
        printf '%s\n' "[cangjie] failed to sign $_f" >&2
        return 1
    }

    # Sign executables and shared libraries so that HarmonyOS will load them.
    #   cjsign <file-or-dir>...        e.g. `cjsign target` in a cjpm project
    #   cjsign -q <file-or-dir>...     same, without listing every file
    cjsign() {
        _cj_sign_quiet=""
        if [ "${1:-}" = "-q" ]; then
            _cj_sign_quiet=1
            shift
        fi
        if [ -z "$_cj_sign_tool" ]; then
            _cj_sign_tool="$(_cj_find_sign_tool)"
        fi
        if [ -z "$_cj_sign_tool" ]; then
            printf '%s\n' "[cangjie] cannot sign: binary-sign-tool was not found." >&2
            printf '%s\n' "[cangjie] install the HarmonyOS/OpenHarmony SDK, or set OHOS_BINARY_SIGN_TOOL to" >&2
            printf '%s\n' "[cangjie] <sdk>/default/openharmony/toolchains/lib/binary-sign-tool." >&2
            unset _cj_sign_quiet
            return 1
        fi
        for _arg in "$@"; do
            if [ -d "$_arg" ]; then
                find "$_arg" -type f 2>/dev/null | while IFS= read -r _f; do
                    _cj_sign_file "$_f"
                done
            else
                _cj_sign_file "$_arg"
            fi
        done
        unset _cj_sign_quiet _arg
        return 0
    }

    # Sign the SDK binaries once per installation. cjc is re-installed (newer
    # mtime) on every SDK upgrade, which invalidates the stamp and re-runs this.
    _cj_sign_tool="$(_cj_find_sign_tool)"
    _cj_sign_stamp="${CANGJIE_HOME}/.ohos-codesign.stamp"
    if [ -z "$_cj_sign_tool" ]; then
        printf '%s\n' "[cangjie] HarmonyOS verifies ELF code signatures, but binary-sign-tool was not found;" >&2
        printf '%s\n' "[cangjie] set OHOS_BINARY_SIGN_TOOL to <ohos-sdk>/default/openharmony/toolchains/lib/binary-sign-tool" >&2
    elif [ -f "${CANGJIE_HOME}/bin/cjc" ] && [ -f "$_cj_sign_stamp" ] && [ ! "${CANGJIE_HOME}/bin/cjc" -nt "$_cj_sign_stamp" ]; then
        : # the SDK of this installation is already signed
    else
        cjsign -q "${CANGJIE_HOME}/bin" "${CANGJIE_HOME}/tools" "${CANGJIE_HOME}/lib" \
            "${CANGJIE_HOME}/runtime" "${CANGJIE_HOME}/third_party"
        if : > "$_cj_sign_stamp" 2>/dev/null; then
            printf '%s\n' "[cangjie] SDK binaries signed for HarmonyOS (ELF code signatures are verified on exec)"
        else
            printf '%s\n' "[cangjie] SDK binaries signed for HarmonyOS, but the result could not be" >&2
            printf '%s\n' "[cangjie] cached in ${CANGJIE_HOME}; run 'cjsign ${CANGJIE_HOME}' to re-check." >&2
        fi
    fi
    printf '%s\n' "[cangjie] build outputs need signing too: cjpm build && cjsign target && cjpm run"

    unset _cj_sign_stamp
fi

# Clean up temporary variables
unset hw_arch
unset sys_name
