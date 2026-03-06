# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# This source file is part of the Cangjie project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

# =============================================================================
# Configuration Variables
# =============================================================================

set(MIMALLOC_SOURCE_DIR ${CMAKE_SOURCE_DIR}/third_party/mimalloc)
set(MIMALLOC_GIT_REPO "https://gitcode.com/GitHub_Trending/mi/mimalloc.git")
set(MIMALLOC_GIT_TAG "v2.1.7")
set(MIMALLOC_MIN_VERSION "2.0.0")

# =============================================================================
# Sanitizer Conflict Detection
# =============================================================================

# Check all known Sanitizer CMake variables
# mimalloc is incompatible with most sanitizers because they both need to
# intercept memory allocation functions
if(CANGJIE_SANITIZER_SUPPORT_ENABLED)
    set(MIMALLOC_CONFLICT_REASON "sanitizer runtime support")
elseif(CANGJIE_ENABLE_ASAN)
    set(MIMALLOC_CONFLICT_REASON "compiler ASan build")
elseif(CANGJIE_ENABLE_ASAN_COV)
    set(MIMALLOC_CONFLICT_REASON "ASan coverage")
elseif(CANGJIE_ENABLE_COMPILER_TSAN)
    set(MIMALLOC_CONFLICT_REASON "compiler TSan")
elseif(CANGJIE_ENABLE_HWASAN)
    set(MIMALLOC_CONFLICT_REASON "HWASan (ARM MTE conflict)")
elseif(CANGJIE_ASAN_SUPPORT)
    set(MIMALLOC_CONFLICT_REASON "ASan support")
elseif(CANGJIE_TSAN_SUPPORT)
    set(MIMALLOC_CONFLICT_REASON "TSan support")
elseif(CANGJIE_HWASAN_SUPPORT)
    set(MIMALLOC_CONFLICT_REASON "HWASan support")
endif()

# Check for LeakSanitizer in compiler flags
string(FIND "${CMAKE_CXX_FLAGS}" "-fsanitize=leak" _LSAN_POS)
if(NOT _LSAN_POS EQUAL -1)
    set(MIMALLOC_CONFLICT_REASON "LeakSanitizer")
endif()

# =============================================================================
# Platform Support Detection
# =============================================================================

# OHOS platform is not tested with mimalloc
if(OHOS AND CANGJIE_USE_MIMALLOC)
    message(WARNING
        "mimalloc is not tested on OHOS platform. "
        "CANGJIE_USE_MIMALLOC disabled.")
    set(CANGJIE_USE_MIMALLOC OFF CACHE BOOL "" FORCE)
endif()

# Android platform is experimental
if(ANDROID AND CANGJIE_USE_MIMALLOC)
    message(WARNING
        "mimalloc on Android is experimental. "
        "CANGJIE_USE_MIMALLOC disabled.")
    set(CANGJIE_USE_MIMALLOC OFF CACHE BOOL "" FORCE)
endif()

# =============================================================================
# Conflict Resolution
# =============================================================================

if(MIMALLOC_CONFLICT_REASON AND CANGJIE_USE_MIMALLOC)
    message(WARNING
        "CANGJIE_USE_MIMALLOC is disabled due to ${MIMALLOC_CONFLICT_REASON}. "
        "mimalloc is not compatible with Sanitizers. "
        "Using system allocator instead.")
    set(CANGJIE_USE_MIMALLOC OFF CACHE BOOL "" FORCE)
    return()
endif()

# =============================================================================
# mimalloc Source Acquisition
# =============================================================================

if(CANGJIE_USE_MIMALLOC)
    # Auto-detect if source directory exists
    if(NOT EXISTS ${MIMALLOC_SOURCE_DIR}/CMakeLists.txt)
        message(STATUS "mimalloc source not found at ${MIMALLOC_SOURCE_DIR}")
        message(STATUS "Attempting to clone from ${MIMALLOC_GIT_REPO}...")

        find_package(Git REQUIRED)

        execute_process(
            COMMAND ${GIT_EXECUTABLE} clone --depth 1 --branch ${MIMALLOC_GIT_TAG}
                    ${MIMALLOC_GIT_REPO} ${MIMALLOC_SOURCE_DIR}
            RESULT_VARIABLE MIMALLOC_CLONE_RESULT
            OUTPUT_VARIABLE MIMALLOC_CLONE_OUTPUT
            ERROR_VARIABLE MIMALLOC_CLONE_ERROR
            TIMEOUT 300
        )

        if(NOT MIMALLOC_CLONE_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Failed to clone mimalloc:\n${MIMALLOC_CLONE_ERROR}\n"
                "Please manually clone the repository:\n"
                "  git clone --depth 1 --branch ${MIMALLOC_GIT_TAG} \\\n"
                "    ${MIMALLOC_GIT_REPO} ${MIMALLOC_SOURCE_DIR}\n"
                "For offline builds, clone on a networked machine and copy the directory."
            )
        endif()

        message(STATUS "mimalloc cloned successfully to ${MIMALLOC_SOURCE_DIR}")
    else()
        message(STATUS "Using existing mimalloc source at ${MIMALLOC_SOURCE_DIR}")
    endif()

    # =========================================================================
    # Platform-Specific Configuration
    # =========================================================================

    # Windows: Use Strategy B (new/delete override only)
    # - Avoids DLL boundary issues with LLVM
    # - More conservative but stable
    # - Lower performance gain (5-8%) but safer
    if(WIN32)
        set(MI_OVERRIDE OFF CACHE BOOL "Do not override malloc/free on Windows" FORCE)
        set(CANGJIE_MIMALLOC_STRATEGY "new-delete" CACHE STRING "mimalloc strategy" FORCE)
        message(STATUS "mimalloc: Using Strategy B (new/delete override) for Windows")
    else()
        # Linux/macOS: Use Strategy A (global malloc/free override)
        # - Maximum performance gain (10-15%)
        # - Covers all allocations including LLVM
        set(MI_OVERRIDE ON CACHE BOOL "Override malloc/free globally" FORCE)
        set(CANGJIE_MIMALLOC_STRATEGY "global" CACHE STRING "mimalloc strategy" FORCE)
        message(STATUS "mimalloc: Using Strategy A (global override) for ${CMAKE_SYSTEM_NAME}")
    endif()

    # =========================================================================
    # Build Configuration
    # =========================================================================

    # Build mimalloc as static library for better integration
    set(MI_BUILD_SHARED OFF CACHE BOOL "Build mimalloc as static library" FORCE)
    set(MI_BUILD_TESTS OFF CACHE BOOL "Disable mimalloc tests" FORCE)
    set(MI_DEBUG_FULL OFF CACHE BOOL "Disable full debug mode" FORCE)

    # Add mimalloc subdirectory
    add_subdirectory(${MIMALLOC_SOURCE_DIR} ${CMAKE_BINARY_DIR}/mimalloc EXCLUDE_FROM_ALL)

    # Version check
    if(MI_VERSION)
        if(MI_VERSION VERSION_LESS ${MIMALLOC_MIN_VERSION})
            message(FATAL_ERROR
                "mimalloc version ${MI_VERSION} is too old. "
                "Minimum required: ${MIMALLOC_MIN_VERSION}")
        endif()
        message(STATUS "mimalloc version: ${MI_VERSION}")
    endif()

    # =========================================================================
    # Create Interface Library for Easy Linking
    # =========================================================================

    add_library(cangjie_mimalloc INTERFACE)

    target_include_directories(cangjie_mimalloc INTERFACE
        ${MIMALLOC_SOURCE_DIR}/include
    )

    target_link_libraries(cangjie_mimalloc INTERFACE mimalloc-static)

    target_compile_definitions(cangjie_mimalloc INTERFACE CANGJIE_USE_MIMALLOC)

    # Linux-specific: Use whole-archive to ensure all symbols are linked
    # This is necessary for malloc/free override to work correctly
    if(UNIX AND NOT APPLE)
        target_link_options(cangjie_mimalloc INTERFACE
            "-Wl,--whole-archive"
            "$<TARGET_FILE:mimalloc-static>"
            "-Wl,--no-whole-archive"
        )
    endif()

    message(STATUS "mimalloc integration enabled successfully")
endif()