# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# This source file is part of the Cangjie project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

if(NOT CANGJIE_ENABLE_CCACHE)
    return()
endif()

if(CANGJIE_CACHE_TOOL STREQUAL "auto" OR NOT DEFINED CANGJIE_CACHE_TOOL)
    find_program(CCACHE_PROGRAM ccache)
    find_program(SCCACHE_PROGRAM sccache)
    if(CCACHE_PROGRAM)
        set(CANGJIE_CACHE_PROGRAM "${CCACHE_PROGRAM}")
        set(CANGJIE_CACHE_TOOL_NAME "ccache")
    elseif(SCCACHE_PROGRAM)
        set(CANGJIE_CACHE_PROGRAM "${SCCACHE_PROGRAM}")
        set(CANGJIE_CACHE_TOOL_NAME "sccache")
    endif()
elseif(CANGJIE_CACHE_TOOL STREQUAL "ccache")
    find_program(CCACHE_PROGRAM ccache)
    if(CCACHE_PROGRAM)
        set(CANGJIE_CACHE_PROGRAM "${CCACHE_PROGRAM}")
        set(CANGJIE_CACHE_TOOL_NAME "ccache")
    endif()
elseif(CANGJIE_CACHE_TOOL STREQUAL "sccache")
    find_program(SCCACHE_PROGRAM sccache)
    if(SCCACHE_PROGRAM)
        set(CANGJIE_CACHE_PROGRAM "${SCCACHE_PROGRAM}")
        set(CANGJIE_CACHE_TOOL_NAME "sccache")
    endif()
endif()

if(CANGJIE_CACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CANGJIE_CACHE_PROGRAM}")

    message(STATUS "${CANGJIE_CACHE_TOOL_NAME} found at ${CANGJIE_CACHE_PROGRAM}. Build with ${CANGJIE_CACHE_TOOL_NAME}.")
else()
    message(STATUS "No compiler cache tool found. Build without cache.")
endif()
