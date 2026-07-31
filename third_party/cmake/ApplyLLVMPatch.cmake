# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# This source file is part of the Cangjie project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

# cmake -P
# ApplyLLVMPatch.cmake (arg 2)
# ${LLVM_SOURCE_DIR} (arg 3)
# ${LLVM_PATCH} (arg 4)
set(LLVM_SOURCE_DIR ${CMAKE_ARGV3})
set(LLVM_PATCH ${CMAKE_ARGV4})

# Calculate the hash of the patch file to detect changes
file(MD5 ${LLVM_PATCH} PATCH_HASH)
set(APPLIED_MARKER ${LLVM_SOURCE_DIR}/.patch_applied_${PATCH_HASH})

# Check if the patch has already been applied
if(EXISTS ${APPLIED_MARKER})
    message(STATUS "LLVM patch ${PATCH_HASH} already applied, skipping")
    return()
endif()

# Check if there are uncommitted changes in the LLVM source directory
execute_process(COMMAND git diff --quiet
    WORKING_DIRECTORY ${LLVM_SOURCE_DIR}
    RESULT_VARIABLE CJNATIVE_SOURCE_DIR_IS_MODIFIED)

if(CJNATIVE_SOURCE_DIR_IS_MODIFIED EQUAL 0)
    # Reset to the base commit
    execute_process(
        COMMAND git reset --hard 5c68a1cb123161b54b72ce90e7975d95a8eaf2a4
        WORKING_DIRECTORY ${LLVM_SOURCE_DIR}
        RESULT_VARIABLE RESET_RESULT
    )

    if(NOT RESET_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to reset LLVM source to base commit")
    endif()

    # Apply the patch
    execute_process(
        COMMAND git apply --reject --whitespace=fix ${LLVM_PATCH}
        WORKING_DIRECTORY ${LLVM_SOURCE_DIR}
        RESULT_VARIABLE APPLY_RESULT
    )

    if(NOT APPLY_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to apply LLVM patch ${LLVM_PATCH}")
    endif()

    # Mark the patch as applied
    file(WRITE ${APPLIED_MARKER} "Patch ${PATCH_HASH} applied successfully")
    message(STATUS "Successfully applied LLVM patch ${PATCH_HASH}")
else()
    message(WARNING "LLVM source directory has uncommitted changes, skipping patch application")
endif()