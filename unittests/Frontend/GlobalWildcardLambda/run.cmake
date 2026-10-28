# Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
# This source file is part of the Cangjie project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(program "${OUTPUT_DIR}/global-wildcard-lambda${EXE_SUFFIX}")
execute_process(
    COMMAND "${CJC}" -p "${SOURCE_DIR}" -O2 --static -o "${program}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Wildcard lambda compilation failed (${result}):\n${out}${err}")
endif()
execute_process(COMMAND "${program}" RESULT_VARIABLE result)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Wildcard lambda callbacks failed (${result})")
endif()
