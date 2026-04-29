# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# This source file is part of the Cangjie project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

set(BASIC_SRC
    StringConvertor.cpp
    DiagnosticEngine.cpp
    DiagnosticEngineImpl.cpp
    InteropCJPackageConfigReader.cpp
    MacroCallDiagInfo.cpp
    Position.cpp
    SourceManager.cpp
    Display.cpp
    DiagnosticEmitter.cpp
    DiagnosticJsonFormatter.cpp
    Utils.cpp
    Print.cpp)

set(CANGJIE_GIT_VERSION_SRC "${CMAKE_CURRENT_BINARY_DIR}/generated/CangjieGitVersion.cpp")
set_source_files_properties("${CANGJIE_GIT_VERSION_SRC}" PROPERTIES GENERATED TRUE)
set(BASIC_SRC ${BASIC_SRC} Version.cpp "${CANGJIE_GIT_VERSION_SRC}")

include_directories(${CMAKE_SOURCE_DIR}/third_party/tinytoml)

add_custom_target(CangjieGitVersionSource
    COMMAND ${CMAKE_COMMAND}
        -D SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -D OUTPUT_FILE=${CANGJIE_GIT_VERSION_SRC}
        -P "${CMAKE_SOURCE_DIR}/cmake/modules/GenerateGitVersion.cmake"
    BYPRODUCTS ${CANGJIE_GIT_VERSION_SRC}
    COMMENT "Generating Cangjie git version source"
    VERBATIM)

add_library(CangjieBasic OBJECT ${BASIC_SRC})
add_dependencies(CangjieBasic CangjieGitVersionSource)
target_compile_options(CangjieBasic PRIVATE -Wno-deprecated-declarations)
target_compile_options(CangjieBasic PRIVATE ${CJC_EXTRA_WARNINGS})
set_source_files_properties(InteropCJPackageConfigReader.cpp PROPERTIES COMPILE_OPTIONS "-fexceptions")
