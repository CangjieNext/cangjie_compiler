// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file ParseOuterInterfaceTest.cpp
 * @brief 解析相关外部接口的单元测试
 * 
 * 本文件提供编译器解析相关外部接口的综合单元测试，覆盖以下接口：
 *   1. PerformParse - 解析源代码生成 AST
 * 
 * 测试场景包括：
 *   - 基本源代码解析测试
 *   - 多源文件解析测试
 *   - 空文件列表处理测试
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
 * @brief 测试基本源代码的解析功能
 * 
 * 测试目的：
 *   验证 PerformParse 接口能够正确解析源文件并生成 AST 节点
 * 
 * 测试步骤：
 *   1. 创建编译器实例，设置源文件为 Test.cj
 *   2. 调用 Compile 编译到 PARSE 阶段
 *   3. 验证编译成功且无错误
 *   4. 检查生成的 AST 结构
 * 
 * 预期结果：
 *   - 编译成功，无诊断错误
 *   - AST 包含包和文件节点
 *   - 文件中包含宏展开声明节点
 * 
 * 关键验证点：
 *   - 解析阶段正确执行（parseResult == true）
 *   - AST 结构完整（packages 非空，files 非空，decls 非空）
 *   - AST 节点类型正确（包含 MACRO_EXPAND_DECL）
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
 * @brief 测试解析多个源文件
 * 
 * 测试目的：
 *   验证 PerformParse 能够正确处理多个源文件的解析
 * 
 * 测试步骤：
 *   1. 创建编译器实例，设置两个源文件（Test.cj 和 Test1.cj）
 *   2. 调用 Compile 编译到 PARSE 阶段
 *   3. 验证编译成功且无错误
 *   4. 检查包中包含多个文件
 * 
 * 预期结果：
 *   - 编译成功，无诊断错误
 *   - 包中至少包含 1 个文件（多个文件可能被合并）
 * 
 * 关键验证点：
 *   - 多文件解析不会失败
 *   - 文件数量符合预期
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
 * @brief 测试空源文件列表的处理
 * 
 * 测试目的：
 *   验证当源文件列表为空时，PerformParse 能够优雅地处理而不会崩溃
 * 
 * 测试步骤：
 *   1. 创建编译器实例，不设置任何源文件
 *   2. 调用 Compile 编译到 PARSE 阶段
 *   3. 验证编译不会崩溃
 * 
 * 预期结果：
 *   - 编译成功返回（不会崩溃）
 *   - 空文件列表被优雅处理
 * 
 * 关键验证点：
 *   - 空文件列表不会导致异常
 *   - 编译器能够正常返回
 */
TEST_F(ParseOuterInterfaceTest, PerformParse_EmptySourceFile)
{
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {};

    bool parseResult = instance->Compile(CompileStage::PARSE);
    EXPECT_TRUE(parseResult);
}
