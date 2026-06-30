// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_UNITTESTS_PARSE_PARSE_TEST_BASE_H
#define CANGJIE_UNITTESTS_PARSE_PARSE_TEST_BASE_H

#include <string>

#include "gtest/gtest.h"
#include "TestCompilerInstance.h"

using namespace Cangjie;
using namespace AST;

class ParseTestBase : public testing::Test {
protected:
    void SetUp() override
    {
#ifdef _WIN32
        srcPath = projectPath + "\\unittests\\Parse\\ParseCangjieFiles\\";
#else
        srcPath = projectPath + "/unittests/Parse/ParseCangjieFiles/";
#endif
#ifdef __x86_64__
        invocation.globalOptions.target.arch = Cangjie::Triple::ArchType::X86_64;
#else
        invocation.globalOptions.target.arch = Cangjie::Triple::ArchType::AARCH64;
#endif
#ifdef _WIN32
        invocation.globalOptions.target.os = Cangjie::Triple::OSType::WINDOWS;
        invocation.globalOptions.executablePath = projectPath + "\\output\\bin\\";
#elif defined(__unix__)
        invocation.globalOptions.target.os = Cangjie::Triple::OSType::LINUX;
        invocation.globalOptions.executablePath = projectPath + "/output/bin/";
#endif
        invocation.globalOptions.importPaths = {definePath};
    }

#ifdef PROJECT_SOURCE_DIR
    std::string projectPath = PROJECT_SOURCE_DIR;
#else
    std::string projectPath = "..";
#endif
    std::string srcPath;
    std::string definePath;
    DiagnosticEngine diag;
    SourceManager sm;
    CompilerInvocation invocation;
    std::unique_ptr<TestCompilerInstance> instance;
};

#endif // CANGJIE_UNITTESTS_PARSE_PARSE_TEST_BASE_H
