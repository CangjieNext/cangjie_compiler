# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# This source file is part of the Cangjie project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

# cmake -P
# MergeBitcodeArchive.cmake (arg 2)
# <llvm-ar path, optional> (arg 3)
# <bitcode file> (arg 4)
# <ffi archive> (arg 5)
# <output archive> (arg 6)

set(LLVM_AR_PATH "${CMAKE_ARGV3}")
set(BITCODE_FILE "${CMAKE_ARGV4}")
set(FFI_ARCHIVE "${CMAKE_ARGV5}")
set(OUTPUT_ARCHIVE "${CMAKE_ARGV6}")

if("${LLVM_AR_PATH}" STREQUAL "")
    find_program(LLVM_AR_PATH NAMES llvm-ar)
endif()

if("${LLVM_AR_PATH}" STREQUAL "")
    message(FATAL_ERROR "llvm-ar is required to merge bitcode and ffi archive.")
endif()

if(NOT EXISTS "${BITCODE_FILE}")
    message(FATAL_ERROR "Bitcode file does not exist: ${BITCODE_FILE}")
endif()

if(NOT EXISTS "${FFI_ARCHIVE}")
    message(FATAL_ERROR "FFI archive does not exist: ${FFI_ARCHIVE}")
endif()

get_filename_component(OUTPUT_DIR "${OUTPUT_ARCHIVE}" DIRECTORY)
if(NOT "${OUTPUT_DIR}" STREQUAL "")
    file(MAKE_DIRECTORY "${OUTPUT_DIR}")
endif()

set(TMP_DIR "${OUTPUT_ARCHIVE}.merge_tmp")
file(REMOVE_RECURSE "${TMP_DIR}")
file(MAKE_DIRECTORY "${TMP_DIR}")

execute_process(
    COMMAND "${LLVM_AR_PATH}" x "${FFI_ARCHIVE}"
    WORKING_DIRECTORY "${TMP_DIR}"
    RESULT_VARIABLE EXTRACT_RESULT
    OUTPUT_VARIABLE EXTRACT_STDOUT
    ERROR_VARIABLE EXTRACT_STDERR)

if(NOT EXTRACT_RESULT EQUAL 0)
    file(REMOVE_RECURSE "${TMP_DIR}")
    message(FATAL_ERROR "Failed to extract ffi archive '${FFI_ARCHIVE}': ${EXTRACT_STDERR}${EXTRACT_STDOUT}")
endif()

file(GLOB EXTRACTED_MEMBERS LIST_DIRECTORIES false "${TMP_DIR}/*")

file(REMOVE "${OUTPUT_ARCHIVE}")

set(ARCHIVE_INPUTS "${BITCODE_FILE}")
list(APPEND ARCHIVE_INPUTS ${EXTRACTED_MEMBERS})

execute_process(
    COMMAND "${LLVM_AR_PATH}" crsD "${OUTPUT_ARCHIVE}" ${ARCHIVE_INPUTS}
    RESULT_VARIABLE CREATE_RESULT
    OUTPUT_VARIABLE CREATE_STDOUT
    ERROR_VARIABLE CREATE_STDERR)

file(REMOVE_RECURSE "${TMP_DIR}")

if(NOT CREATE_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to create merged archive '${OUTPUT_ARCHIVE}': ${CREATE_STDERR}${CREATE_STDOUT}")
endif()
