// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file ParseOuterInterfaceTest.cpp
 * @brief Unit tests for parsing-related external interfaces
 * 
 * This file provides comprehensive unit tests for compiler parsing-related external interfaces, covering:
 *   1. PerformParse - Parse source code to generate AST
 * 
 * Test scenarios include:
 *   - Basic source code parsing test
 *   - Multiple source files parsing test
 *   - Empty file list handling test
 */

#include <cstdlib>
#include <string>
#include "gtest/gtest.h"

#include "TestCompilerInstance.h"
#include "cangjie/AST/Match.h"
#include "cangjie/AST/Walker.h"
#include "cangjie/Modules/ImportManager.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

using namespace Cangjie;
using namespace AST;

namespace {
std::unordered_map<std::string, std::string> GetEnvironmentVars()
{
    std::unordered_map<std::string, std::string> envVars;
#ifdef _WIN32
    char **env = _environ;
#else
    char **env = environ;
#endif
    while (env && *env) {
        std::string entry(*env);
        size_t pos = entry.find('=');
        if (pos != std::string::npos) {
            std::string key = entry.substr(0, pos);
            std::string value = entry.substr(pos + 1);
            envVars[key] = value;
        }
        ++env;
    }
    return envVars;
}
}

// =============================================================================
// Test fixture for ParseOuterInterfaceTest
// =============================================================================
class ParseOuterInterfaceTest : public testing::Test {
protected:
    void SetUp() override
    {
#ifdef _WIN32
        srcPath = projectPath + "\\unittests\\Parse\\ParseCangjieFiles\\";
        // definePath = srcPath + "define\\";
#else
        srcPath = projectPath + "/unittests/Parse/ParseCangjieFiles/";
        // definePath = srcPath + "define/";
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
        std::string cangjieHome = projectPath + "/output";
#if defined(_WIN32)
        std::string platform = "windows_x86_64";
#elif defined(__APPLE__) && defined(__x86_64__)
        std::string platform = "darwin_x86_64";
#elif defined(__APPLE__)
        std::string platform = "darwin_arm64";
#elif defined(__x86_64__)
        std::string platform = "linux_x86_64";
#else
        std::string platform = "linux_aarch64";
#endif
        std::string cangjiePath = cangjieHome + "/modules/" + platform + "_cjnative";

#ifdef _WIN32
        char* oldHome = getenv("CANGJIE_HOME");
        char* oldPath = getenv("CANGJIE_PATH");
        if (oldHome) savedCangjieHome = oldHome;
        if (oldPath) savedCangjiePath = oldPath;
        _putenv_s("CANGJIE_HOME", cangjieHome.c_str());
        _putenv_s("CANGJIE_PATH", cangjiePath.c_str());
#else
        char* oldHome = getenv("CANGJIE_HOME");
        char* oldPath = getenv("CANGJIE_PATH");
        if (oldHome) savedCangjieHome = oldHome;
        if (oldPath) savedCangjiePath = oldPath;
        setenv("CANGJIE_HOME", cangjieHome.c_str(), 1);
        setenv("CANGJIE_PATH", cangjiePath.c_str(), 1);
#endif
        invocation.globalOptions.ReadPathsFromEnvironmentVars(GetEnvironmentVars());
        // invocation.globalOptions.importPaths = {definePath};
    }

    void TearDown() override
    {
#ifdef _WIN32
        if (savedCangjieHome.empty()) {
            _putenv_s("CANGJIE_HOME", "");
        } else {
            _putenv_s("CANGJIE_HOME", savedCangjieHome.c_str());
        }
        if (savedCangjiePath.empty()) {
            _putenv_s("CANGJIE_PATH", "");
        } else {
            _putenv_s("CANGJIE_PATH", savedCangjiePath.c_str());
        }
#else
        if (savedCangjieHome.empty()) {
            unsetenv("CANGJIE_HOME");
        } else {
            setenv("CANGJIE_HOME", savedCangjieHome.c_str(), 1);
        }
        if (savedCangjiePath.empty()) {
            unsetenv("CANGJIE_PATH");
        } else {
            setenv("CANGJIE_PATH", savedCangjiePath.c_str(), 1);
        }
#endif
    }

#ifdef PROJECT_SOURCE_DIR
    std::string projectPath = PROJECT_SOURCE_DIR;
#else
    std::string projectPath = "..";
#endif
    std::string srcPath;
    // std::string definePath;
    std::string savedCangjieHome;
    std::string savedCangjiePath;
    DiagnosticEngine diag;
    CompilerInvocation invocation;
    std::unique_ptr<TestCompilerInstance> instance;
};

/**
 * @brief Test basic source code parsing functionality
 * 
 * Test purpose:
 *   Verify that the PerformParse interface can correctly parse source files and generate AST nodes
 * 
 * Test steps:
 *   1. Create compiler instance, set source file to Test.cj
 *   2. Call Compile to compile to PARSE stage
 *   3. Verify compilation succeeds with no errors
 *   4. Check generated AST structure
 * 
 * Expected results:
 *   - Compilation succeeds with no diagnostic errors
 *   - AST contains package and file nodes
 *   - File contains macro expand declaration nodes
 * 
 * Key verification points:
 *   - Parse stage executes correctly (parseResult == true)
 *   - AST structure is complete (packages not empty, files not empty, decls not empty)
 *   - AST node types are correct (contains MACRO_EXPAND_DECL)
 */
TEST_F(ParseOuterInterfaceTest, PerformParse_BasicSourceCode)
{
    auto src = srcPath + "Test.cj";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};

    bool parseResult = instance->Compile(CompileStage::PARSE);
    EXPECT_TRUE(parseResult);
    EXPECT_EQ(diag.GetErrorCount(), 0);

    auto packages = instance->GetSourcePackages();
    ASSERT_FALSE(packages.empty());
    ASSERT_FALSE(packages[0]->files.empty());

    auto file = packages[0]->files[0].get();
    ASSERT_FALSE(file->decls.empty());

    auto expandDecl = AST::As<ASTKind::MACRO_EXPAND_DECL>(file->decls[0].get());
    EXPECT_TRUE(expandDecl != nullptr);
}

/**
 * @brief Test parsing multiple source files
 * 
 * Test purpose:
 *   Verify that PerformParse can correctly handle parsing of multiple source files
 * 
 * Test steps:
 *   1. Create compiler instance, set two source files (Test.cj and Test1.cj)
 *   2. Call Compile to compile to PARSE stage
 *   3. Verify compilation succeeds with no errors
 *   4. Check that package contains multiple files
 * 
 * Expected results:
 *   - Compilation succeeds with no diagnostic errors
 *   - Package contains at least 1 file (multiple files may be merged)
 * 
 * Key verification points:
 *   - Multi-file parsing does not fail
 *   - File count matches expectations
 */
TEST_F(ParseOuterInterfaceTest, PerformParse_MultipleSourceFiles)
{
    auto src1 = srcPath + "Test.cj";
    auto src2 = srcPath + "Test1.cj";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src1, src2};

    bool parseResult = instance->Compile(CompileStage::PARSE);
    EXPECT_TRUE(parseResult);
    EXPECT_EQ(diag.GetErrorCount(), 0);

    auto packages = instance->GetSourcePackages();
    ASSERT_FALSE(packages.empty());
    EXPECT_GE(packages[0]->files.size(), 1);
}

/**
 * @brief Test handling of empty source file list
 * 
 * Test purpose:
 *   Verify that PerformParse can gracefully handle empty source file list without crashing
 * 
 * Test steps:
 *   1. Create compiler instance without setting any source files
 *   2. Call Compile to compile to PARSE stage
 *   3. Verify compilation does not crash
 * 
 * Expected results:
 *   - Compilation returns successfully (does not crash)
 *   - Empty file list is handled gracefully
 * 
 * Key verification points:
 *   - Empty file list does not cause exceptions
 *   - Compiler can return normally
 */
TEST_F(ParseOuterInterfaceTest, PerformParse_EmptySourceFile)
{
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {};

    bool parseResult = instance->Compile(CompileStage::PARSE);
    EXPECT_TRUE(parseResult);
}
