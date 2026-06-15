// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <iostream>
#include <string>

#include "gtest/gtest.h"
#include "TestCompilerInstance.h"
#include "cangjie/ConditionalCompilation/ConditionalCompilation.h"

using namespace Cangjie;

class ConditionalCompilationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
#ifdef _WIN32
        srcPath = projectPath + "\\unittests\\ConditionalCompilation\\srcFiles\\";
#else
        srcPath = projectPath + "/unittests/ConditionalCompilation/srcFiles/";
#endif
    }

#ifdef PROJECT_SOURCE_DIR
    // Gets the absolute path of the project from the compile parameter.
    std::string projectPath = PROJECT_SOURCE_DIR;
#else
    // Just in case, give it a default value.
    // Assume the initial is in the build directory.
    std::string projectPath = "..";
#endif
    std::string srcPath;
    DiagnosticEngine diag;
    CompilerInvocation invocation;
    std::unique_ptr<TestCompilerInstance> instance;
};

/**
 * @brief 测试条件编译的基本功能
 * 
 * 测试目的：
 *   验证 PerformConditionCompile 接口能够正确处理条件编译指令
 * 
 * 测试步骤：
 *   1. 创建编译器实例，设置源文件为 os.cj
 *   2. 调用 Compile 编译到 CONDITION_COMPILE 阶段
 *   3. 验证编译成功且无错误
 *   4. 检查 AST 结构完整性
 * 
 * 预期结果：
 *   - 编译成功，无诊断错误
 *   - AST 包含包和文件节点
 *   - 文件中包含声明节点
 * 
 * 关键验证点：
 *   - 条件编译阶段正确执行（parseResult == true）
 *   - AST 结构完整（packages 非空，files 非空，decls 非空）
 */
TEST_F(ConditionalCompilationTest, PerformConditionCompile_Basic)
{
    auto src = srcPath + "os.cj";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};

    bool parseResult = instance->Compile(CompileStage::CONDITION_COMPILE);
    EXPECT_TRUE(parseResult);
    EXPECT_EQ(diag.GetErrorCount(), 0);

    auto packages = instance->GetSourcePackages();
    ASSERT_FALSE(packages.empty());
    ASSERT_FALSE(packages[0]->files.empty());
    auto file = packages[0]->files[0].get();
    ASSERT_FALSE(file->decls.empty());
}

/**
 * @brief 测试解析后执行条件编译
 * 
 * 测试目的：
 *   验证在 PARSE 阶段后显式调用 PerformConditionCompile 的行为
 * 
 * 测试步骤：
 *   1. 创建编译器实例，设置源文件为 os.cj
 *   2. 先执行 PARSE 阶段
 *   3. 记录解析后的声明数量
 *   4. 显式调用 PerformConditionCompile
 *   5. 验证条件编译后声明数量变化
 * 
 * 预期结果：
 *   - 解析后有 3 个声明
 *   - 条件编译成功执行
 *   - 条件编译后剩余 2 个声明（根据条件过滤掉 1 个）
 * 
 * 关键验证点：
 *   - 解析阶段正确执行
 *   - 条件编译能够过滤掉不符合条件的声明
 *   - 声明数量变化符合预期
 */
TEST_F(ConditionalCompilationTest, PerformConditionCompile_AfterParse)
{
    auto src = srcPath + "os.cj";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};

    bool parseResult = instance->Compile(CompileStage::PARSE);
    ASSERT_TRUE(parseResult);

    auto packages = instance->GetSourcePackages();
    auto declCountBefore = packages[0]->files[0]->decls.size();
    EXPECT_EQ(declCountBefore, 3);

    bool ccResult = instance->PerformConditionCompile();
    EXPECT_TRUE(ccResult);

    auto declCountAfter = packages[0]->files[0]->decls.size();
    EXPECT_EQ(declCountAfter, 2);
}

/**
 * @brief 测试不包含条件编译指令的文件
 * 
 * 测试目的：
 *   验证当源文件不包含条件编译指令时，PerformConditionCompile 不会过滤任何声明
 * 
 * 测试步骤：
 *   1. 创建编译器实例，使用不包含条件编译指令的 func_plain.cj
 *   2. 执行 PARSE 阶段
 *   3. 记录解析后的声明数量
 *   4. 执行条件编译
 *   5. 验证声明数量不变
 * 
 * 预期结果：
 *   - 条件编译成功执行
 *   - 声明数量保持不变
 * 
 * 关键验证点：
 *   - 无条件编译指令的文件不会受影响
 *   - 所有声明都被保留
 */
TEST_F(ConditionalCompilationTest, PerformConditionCompile_AfterParse_NoFilter)
{
    auto src = srcPath + "func_plain.cj";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};

    bool parseResult = instance->Compile(CompileStage::PARSE);
    ASSERT_TRUE(parseResult);

    auto packages = instance->GetSourcePackages();
    auto declCountBefore = packages[0]->files[0]->decls.size();

    bool ccResult = instance->PerformConditionCompile();
    EXPECT_TRUE(ccResult);

    auto declCountAfter = packages[0]->files[0]->decls.size();
    EXPECT_EQ(declCountAfter, declCountBefore);
}

TEST_F(ConditionalCompilationTest, for_lsp)
{
    auto src = srcPath + "os.cj";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    invocation.globalOptions.enableMacroInLSP = true;
    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::PARSE);
    instance->PerformConditionCompile();
}

TEST_F(ConditionalCompilationTest, passedCondition_for_lsp)
{
    auto src = srcPath + "os.cj";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    invocation.globalOptions.enableMacroInLSP = true;
    invocation.globalOptions.passedWhenKeyValue.insert({"test1", "abc"});
    invocation.globalOptions.passedWhenKeyValue.insert({"test2", "aaa"});
    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::PARSE);
    instance->PerformConditionCompile();
}

TEST_F(ConditionalCompilationTest, passedCondition_cfgFile_for_lsp)
{
    auto src = srcPath + "os.cj";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    invocation.globalOptions.enableMacroInLSP = true;
    invocation.globalOptions.passedWhenCfgPaths.emplace_back(srcPath);
    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::PARSE);
    instance->PerformConditionCompile();
}

TEST_F(ConditionalCompilationTest, packagePaths_for_lsp)
{
    auto src = srcPath + "os.cj";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    invocation.globalOptions.enableMacroInLSP = true;
    invocation.globalOptions.packagePaths.emplace_back(srcPath);

    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::PARSE);
    instance->PerformConditionCompile();
}

#ifndef _WIN32
TEST_F(ConditionalCompilationTest, cfgPaths_no_file_for_lsp)
{
    auto src = srcPath + "os.cj";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    invocation.globalOptions.enableMacroInLSP = true;
    invocation.globalOptions.passedWhenCfgPaths.emplace_back("srcPath");

    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::PARSE);
    instance->PerformConditionCompile();

    EXPECT_EQ(diag.GetWarningCount(), 1);
}

TEST_F(ConditionalCompilationTest, same_with_builtin_for_lsp)
{
    auto src = srcPath + "os.cj";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    invocation.globalOptions.enableMacroInLSP = true;
    invocation.globalOptions.passedWhenCfgPaths.emplace_back("srcPath");
    invocation.globalOptions.passedWhenKeyValue.insert({"os", "aaa"});
    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::PARSE);
    instance->PerformConditionCompile();

    EXPECT_EQ(diag.GetErrorCount(), 1);
}

TEST_F(ConditionalCompilationTest, cfg_path_ignored_for_lsp)
{
    auto src = srcPath + "os.cj";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    invocation.globalOptions.enableMacroInLSP = true;
    invocation.globalOptions.passedWhenCfgPaths.emplace_back("srcPath");
    invocation.globalOptions.passedWhenKeyValue.insert({"test1", "abc"});
    invocation.globalOptions.passedWhenKeyValue.insert({"test2", "aaa"});
    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::PARSE);
    instance->PerformConditionCompile();

    EXPECT_EQ(diag.GetWarningCount(), 1);
}
#endif
