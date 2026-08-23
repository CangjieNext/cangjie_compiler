#!/bin/bash

# Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
# This source file is part of the Cangjie project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

set -euo pipefail

runtime_dir=$(cd "$1"; pwd)

check_envsetup() {
    script_name="$1"
    library_dir="$2"
    script_path="${runtime_dir}/${script_name}"
    expected_path="${runtime_dir}/lib/${library_dir}"

    actual_path=$("${BASH}" -c \
        'set -u; unset LD_LIBRARY_PATH; source "$1"; printf "%s" "$LD_LIBRARY_PATH"' _ "${script_path}")
    if [ "${actual_path}" != "${expected_path}" ]; then
        echo "${script_name}: unexpected LD_LIBRARY_PATH when the original value is unset: ${actual_path}" >&2
        return 1
    fi

    actual_path=$(LD_LIBRARY_PATH="" "${BASH}" -c \
        'source "$1"; printf "%s" "$LD_LIBRARY_PATH"' _ "${script_path}")
    if [ "${actual_path}" != "${expected_path}" ]; then
        echo "${script_name}: unexpected LD_LIBRARY_PATH when the original value is empty: ${actual_path}" >&2
        return 1
    fi

    inherited_path="/existing/library/path"
    actual_path=$(LD_LIBRARY_PATH="${inherited_path}" "${BASH}" -c \
        'source "$1"; printf "%s" "$LD_LIBRARY_PATH"' _ "${script_path}")
    if [ "${actual_path}" != "${expected_path}:${inherited_path}" ]; then
        echo "${script_name}: unexpected LD_LIBRARY_PATH when the original value is set: ${actual_path}" >&2
        return 1
    fi
}

check_envsetup llvm_linux_x86_64.sh linux_x86_64_cjnative
check_envsetup llvm_linux_aarch64.sh linux_aarch64_cjnative
