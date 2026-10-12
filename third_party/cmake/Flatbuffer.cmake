# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# This source file is part of the Cangjie project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

set(FLATBUFFERS_SRC ${CMAKE_CURRENT_SOURCE_DIR}/flatbuffers)

# Build flatc for the target platform (the installed / shipped binary).
# Pass CMAKE_BUILD_TYPE so release deliveries get an optimized Release flatc.
set(FLATBUFFERS_COMPILE_OPTIONS
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER})
if(CMAKE_CROSSCOMPILING)
    # Ensure the ExternalProject uses the target sysroot/toolchain rather than the host.
    list(APPEND FLATBUFFERS_COMPILE_OPTIONS
        -DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}
        -DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR})
    if(CMAKE_TOOLCHAIN_FILE)
        # Prefer the target toolchain file; do not override its CMAKE_C/CXX_FLAGS.
        list(APPEND FLATBUFFERS_COMPILE_OPTIONS
            -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
            -DCANGJIE_TARGET_TOOLCHAIN=${CANGJIE_TARGET_TOOLCHAIN}
            -DCANGJIE_TARGET_SYSROOT=${CANGJIE_TARGET_SYSROOT})
    else()
        list(APPEND FLATBUFFERS_COMPILE_OPTIONS
            -DCMAKE_C_FLAGS=${GCC_TOOLCHAIN_FLAG}
            -DCMAKE_CXX_FLAGS=${GCC_TOOLCHAIN_FLAG})
    endif()
    if(CMAKE_AR)
        list(APPEND FLATBUFFERS_COMPILE_OPTIONS -DCMAKE_AR=${CMAKE_AR})
    endif()
    if(CMAKE_RANLIB)
        list(APPEND FLATBUFFERS_COMPILE_OPTIONS -DCMAKE_RANLIB=${CMAKE_RANLIB})
    endif()
else()
    list(APPEND FLATBUFFERS_COMPILE_OPTIONS
        -DCMAKE_C_FLAGS=${GCC_TOOLCHAIN_FLAG}
        -DCMAKE_CXX_FLAGS=${GCC_TOOLCHAIN_FLAG})
endif()

# Cross libs-only builds do not install or run target flatc; skip building it.
set(FLATBUFFERS_BUILD_FLATC ON)
set(FLATBUFFERS_BYPRODUCTS)
if(CMAKE_CROSSCOMPILING AND NOT CANGJIE_BUILD_CJC)
    set(FLATBUFFERS_BUILD_FLATC OFF)
else()
    list(APPEND FLATBUFFERS_BYPRODUCTS
        ${CMAKE_BINARY_DIR}/bin/flatc${CMAKE_EXECUTABLE_SUFFIX})
endif()
list(APPEND FLATBUFFERS_COMPILE_OPTIONS
    -DFLATBUFFERS_BUILD_FLATC=${FLATBUFFERS_BUILD_FLATC})

# Statically link flatc only for Windows (MinGW) deliveries.
# - Windows + llvm-mingw: the C++ runtime lives in DLLs such as libc++.dll /
#   libunwind.dll, which are not part of the OS. The SDK places flatc.exe under
#   third_party/flatbuffers/bin without those DLLs next to it, and envsetup PATH
#   does not cover that directory either, so a dynamically linked flatc.exe fails
#   to start when users run it directly. Use FLATBUFFERS_STATIC_FLATC (adds
#   -static) the same way Release cjc does.
# - Linux / macOS: libstdc++ / libc++ and the C runtime are provided by the
#   system and resolved via the dynamic loader, so a dynamically linked flatc
#   is fine and matches the default FLATBUFFERS_STATIC_FLATC=OFF.
if(WIN32 OR MINGW)
    list(APPEND FLATBUFFERS_COMPILE_OPTIONS -DFLATBUFFERS_STATIC_FLATC=ON)
endif()

# for CloudDragon, download in Prebuild
if(EXISTS ${FLATBUFFERS_SRC}/CMakeLists.txt)
    set(FLATBUFFERS_DOWNLOAD_ARGS
        SOURCE_DIR ${FLATBUFFERS_SRC})
else()
    set(REPOSITORY_PATH https://gitcode.com/openharmony/third_party_flatbuffers.git)
    message(STATUS "Set flatbuffers REPOSITORY_PATH: ${REPOSITORY_PATH}")
    set(FLATBUFFERS_DOWNLOAD_ARGS
        GIT_REPOSITORY ${REPOSITORY_PATH}
        GIT_TAG c3e4d69cbd5950e43f775ba76eadb30750d6e0b7
        GIT_PROGRESS ON
        GIT_CONFIG ${GIT_ARGS}
        GIT_SHALLOW OFF)
endif()

ExternalProject_Add(
    flatbuffers
    ${FLATBUFFERS_DOWNLOAD_ARGS}
    PATCH_COMMAND ${CMAKE_COMMAND}
        -DFLATBUFFERS_SRC=<SOURCE_DIR>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ApplyCangjieIdlPackagePatch.cmake
    CMAKE_ARGS
        # no need to Build tests and install.
        -DFLATBUFFERS_BUILD_TESTS=OFF
        -DFLATBUFFERS_INSTALL=ON
        # Build only necessary targets.
        -DFLATBUFFERS_BUILD_FLATHASH=OFF
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}
        ${FLATBUFFERS_COMPILE_OPTIONS}
    STEP_TARGETS patch
    BUILD_BYPRODUCTS ${FLATBUFFERS_BYPRODUCTS})
externalproject_get_property(flatbuffers SOURCE_DIR)
set(FLATBUFFERS_SRC ${SOURCE_DIR})
set(FLATBUFFERS_SOURCE_DIR ${SOURCE_DIR} PARENT_SCOPE)

set(FLATC_TARGET_EXECUTABLE "${CMAKE_BINARY_DIR}/bin/flatc${CMAKE_EXECUTABLE_SUFFIX}")

# flatc is also invoked during the build to generate headers. When cross-compiling,
# the target flatc cannot run on the host, so build a host-native flatc as well.
if(CMAKE_CROSSCOMPILING)
    # CMAKE_HOST_EXECUTABLE_SUFFIX is only available since CMake 3.31; keep a
    # fallback for older versions allowed by cmake_minimum_required (3.16.5).
    if(NOT DEFINED CMAKE_HOST_EXECUTABLE_SUFFIX)
        if(CMAKE_HOST_WIN32)
            set(CMAKE_HOST_EXECUTABLE_SUFFIX ".exe")
        else()
            set(CMAKE_HOST_EXECUTABLE_SUFFIX "")
        endif()
    endif()
    if(CMAKE_HOST_WIN32)
        set(FLATBUFFERS_HOST_C_COMPILER gcc)
        set(FLATBUFFERS_HOST_CXX_COMPILER g++)
    else()
        set(FLATBUFFERS_HOST_C_COMPILER clang)
        set(FLATBUFFERS_HOST_CXX_COMPILER clang++)
    endif()
    set(FLATC_HOST_EXECUTABLE
        "${CMAKE_BINARY_DIR}/host-tools/bin/flatc${CMAKE_HOST_EXECUTABLE_SUFFIX}")
    ExternalProject_Add(
        flatbuffers-host
        DEPENDS flatbuffers-patch
        DOWNLOAD_COMMAND ""
        UPDATE_COMMAND ""
        PATCH_COMMAND ""
        SOURCE_DIR ${SOURCE_DIR}
        CMAKE_ARGS
            -DFLATBUFFERS_BUILD_TESTS=OFF
            -DFLATBUFFERS_INSTALL=ON
            -DFLATBUFFERS_BUILD_FLATHASH=OFF
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/host-tools
            -DCMAKE_C_COMPILER=${FLATBUFFERS_HOST_C_COMPILER}
            -DCMAKE_CXX_COMPILER=${FLATBUFFERS_HOST_CXX_COMPILER}
        BUILD_BYPRODUCTS ${FLATC_HOST_EXECUTABLE})
else()
    set(FLATC_HOST_EXECUTABLE "${FLATC_TARGET_EXECUTABLE}")
endif()

set(FLATC_TARGET_EXECUTABLE ${FLATC_TARGET_EXECUTABLE} PARENT_SCOPE)
set(FLATC_HOST_EXECUTABLE ${FLATC_HOST_EXECUTABLE} PARENT_SCOPE)
