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
    local _comm
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
        _comm="$(ps -o comm= -p "$$" 2>/dev/null | tr -d '\n')"
        _comm="${_comm##*/}"
        case "$_comm" in
            -*) _comm="${_comm#-}" ;;
        esac
        case "$_comm" in
            zsh|bash) printf '%s\n' "$_comm"; return 0 ;;
            sh|dash|ash|busybox) printf '%s\n' "sh"; return 0 ;;
        esac
    fi

    # If ps is not available, try reading from /proc filesystem
    if [ -r "/proc/$$/comm" ]; then
        _comm="$(tr -d '\n' < "/proc/$$/comm" 2>/dev/null)"
        _comm="${_comm##*/}"
        case "$_comm" in
            -*) _comm="${_comm#-}" ;;
        esac
        case "$_comm" in
            zsh|bash) printf '%s\n' "$_comm"; return 0 ;;
            sh|dash|ash|busybox) printf '%s\n' "sh"; return 0 ;;
        esac
    fi

    # Unrecognized shell
    printf '%s\n' "unknown"
    return 0
}

# Resolve relative path to absolute path
_cj_resolve_abs_path() {
    # NOTE: avoid the name "path" — in zsh it is tied to PATH (array form),
    # and declaring `local path` corrupts the global PATH on return. Use _rpath instead.
    local _input _rpath _dirname _basename
    _input="$1"
    [ -n "$_input" ] || return 1

    # Handle relative and absolute paths
    case "${_input}" in
        /*) _rpath="$_input" ;;
        *) _rpath="$PWD/$_input" ;;
    esac

    # Try multiple tools to resolve absolute path, in order of preference:
    # 1. realpath
    # 2. readlink -f
    # 3. python3
    # 4. python
    # 5. pure shell implementation
    if command -v realpath >/dev/null 2>&1; then
        realpath "$_rpath" 2>/dev/null && return 0
    fi
    if command -v readlink >/dev/null 2>&1; then
        readlink -f "$_rpath" 2>/dev/null && return 0
    fi
    if command -v python3 >/dev/null 2>&1; then
        python3 -c 'import os,sys; print(os.path.realpath(sys.argv[1]))' "$_rpath" 2>/dev/null && return 0
    fi
    if command -v python >/dev/null 2>&1; then
        python -c 'import os,sys; print(os.path.realpath(sys.argv[1]))' "$_rpath" 2>/dev/null && return 0
    fi

    # Pure shell implementation: cd to directory then pwd
    _dirname="${_rpath%/*}"
    _basename="${_rpath##*/}"
    (cd -P "$_dirname" 2>/dev/null && printf '%s/%s\n' "$(pwd -P)" "$_basename") || return 1
}

# Get absolute path of the script directory from the script path
_cj_script_dir_from_path() {
    local _input_path _absolute_path _directory_part
    _input_path="$1"
    [ -n "$_input_path" ] || return 1

    # First resolve to absolute path
    _absolute_path="$(_cj_resolve_abs_path "$_input_path" 2>/dev/null || true)"
    [ -n "$_absolute_path" ] || _absolute_path="$_input_path"

    # Get directory part
    _directory_part="${_absolute_path%/*}"
    [ -n "$_directory_part" ] || _directory_part="."
    # Change to directory and output absolute path
    (cd -P "$_directory_part" 2>/dev/null && pwd -P) || return 1
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
    local _env_var_name="$1"
    local _env_var_value=""
    local _item=""
    local _new_env_var_value=""
    local _remaining=""

    # Read environment variable by name in a way that works in both bash and zsh.
    eval "_env_var_value=\${$_env_var_name}"

    if [ -n "${_env_var_value}" ]; then
        # IMPORTANT: use explicit ':' splitting with parameter expansion.
        # Do not rely on shell word-splitting behavior, which differs across sh/bash/zsh
        _remaining="${_env_var_value}"
        while :; do
            case "${_remaining}" in
                *:*)
                    _item="${_remaining%%:*}"
                    _remaining="${_remaining#*:}"
                    ;;
                *)
                    _item="${_remaining}"
                    _remaining=""
                    ;;
            esac
            case "${_item}" in
                "${CANGJIE_HOME}/"*) ;;
                "${HOME}/.cjpm/bin") ;;
                *)
                    _new_env_var_value="${_new_env_var_value}${_new_env_var_value:+:}${_item}"
                    ;;
            esac
            [ -n "${_remaining}" ] || break
        done
        eval "export ${_env_var_name}=\"\${_new_env_var_value}\""
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

# Clean up temporary variables
unset hw_arch
unset sys_name
unset script_dir
unset shell_name