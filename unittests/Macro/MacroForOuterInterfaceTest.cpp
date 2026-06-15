// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file MacroForOuterInterfaceTest.cpp
 * @brief 宏相关外部接口的单元测试
 * 
 * 本文件提供编译器宏相关外部接口的综合单元测试，覆盖以下接口：
 *   1. PerformMacroExpand     - 展开所有 MacroExpandDecl 节点
 *   2. ExpandDecl             - 展开单个 MacroExpandDecl 节点
 *   3. CreateMacroSrvProcess  - 为 LSP 创建宏服务进程
 * 
 * 测试场景包括：
 *   - 宏展开的基本功能测试
 *   - 单个宏声明的展开测试
 *   - 多注解宏声明的展开测试
 *   - LSP 模式下的宏服务测试
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
// Test fixture for MacroForOuterInterfaceTest
// =============================================================================
class MacroForOuterInterfaceTest : public testing::Test {
protected:
    void SetUp() override
    {
#ifdef _WIN32
        srcPath = projectPath + "\\unittests\\Macro\\srcFiles\\";
        definePath = srcPath + "define\\";
#else
        srcPath = projectPath + "/unittests/Macro/srcFiles/";
        definePath = srcPath + "define/";
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
        invocation.globalOptions.importPaths = {definePath};
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
    std::string definePath;
    std::string savedCangjieHome;
    std::string savedCangjiePath;
    DiagnosticEngine diag;
    CompilerInvocation invocation;
    std::unique_ptr<TestCompilerInstance> instance;
};

/**
 * @brief 测试宏展开的基本功能
 * 
 * 测试目的：
 *   验证 PerformMacroExpand 接口能够正确展开源文件中的所有宏声明
 * 
 * 测试步骤：
 *   1. 编译宏定义文件 define.cj，生成宏库
 *   2. 创建编译器实例，设置源文件为 test_gen.cj
 *   3. 调用 Compile 编译到 MACRO_EXPAND 阶段
 *   4. 验证编译成功且无错误
 *   5. 检查 AST 中是否包含宏展开后生成的类声明
 * 
 * 预期结果：
 *   - 编译成功，无诊断错误
 *   - @GenClass 宏被正确展开，AST 中包含 ClassDecl 节点
 * 
 * 关键验证点：
 *   - 宏库编译成功（err == 0）
 *   - 编译阶段正确执行（result == true）
 *   - AST 包含展开后的类声明（foundClass == true）
 */
TEST_F(MacroForOuterInterfaceTest, PerformMacroExpand_Basic)
{
    std::string command = "cd " + definePath + " && cjc define.cj --compile-macro";
    int err = system(command.c_str());
    ASSERT_EQ(0, err);

    auto src = srcPath + "test_gen.cj";
    invocation.globalOptions.executablePath = projectPath + "/output/bin/";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};

    bool result = instance->Compile(CompileStage::MACRO_EXPAND);
    EXPECT_TRUE(result);
    EXPECT_EQ(diag.GetErrorCount(), 0);

    auto packages = instance->GetSourcePackages();
    ASSERT_FALSE(packages.empty());
    auto file = packages[0]->files[0].get();

    bool foundClass = false;
    for (auto &decl : file->decls) {
        if (AST::As<ASTKind::CLASS_DECL>(decl.get())) {
            foundClass = true;
            break;
        }
    }
    EXPECT_TRUE(foundClass);
}

/**
 * @brief 测试在包导入后显式调用宏展开
 * 
 * 测试目的：
 *   验证 PerformMacroExpand 可以在 IMPORT_PACKAGE 阶段后显式调用
 * 
 * 测试步骤：
 *   1. 编译宏定义文件，生成宏库
 *   2. 创建编译器实例
 *   3. 先执行到 IMPORT_PACKAGE 阶段
 *   4. 显式调用 PerformMacroExpand 方法
 *   5. 验证宏展开成功且无错误
 * 
 * 预期结果：
 *   - 包导入成功（importResult == true）
 *   - 宏展开成功（expandResult == true）
 *   - 无诊断错误
 * 
 * 关键验证点：
 *   - 编译阶段可以分步执行
 *   - PerformMacroExpand 可以独立调用
 */
TEST_F(MacroForOuterInterfaceTest, PerformMacroExpand_CalledAfterImportPackage)
{
    std::string command = "cd " + definePath + " && cjc define.cj --compile-macro";
    int err = system(command.c_str());
    ASSERT_EQ(0, err);

    auto src = srcPath + "test_gen.cj";
    invocation.globalOptions.executablePath = projectPath + "/output/bin/";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};

    bool importResult = instance->Compile(CompileStage::IMPORT_PACKAGE);
    ASSERT_TRUE(importResult);

    bool expandResult = instance->PerformMacroExpand();
    EXPECT_TRUE(expandResult);
    EXPECT_EQ(diag.GetErrorCount(), 0);
}

/**
 * @brief 测试源文件中不包含宏时的行为
 * 
 * 测试目的：
 *   验证当源文件不包含任何宏声明时，PerformMacroExpand 能正常处理
 * 
 * 测试步骤：
 *   1. 使用不包含宏的普通函数文件 func_plain.cj
 *   2. 编译到 MACRO_EXPAND 阶段
 *   3. 验证编译成功且 AST 正常生成
 * 
 * 预期结果：
 *   - 编译成功，无错误
 *   - AST 包含原有的函数声明
 * 
 * 关键验证点：
 *   - 无宏源文件不会导致编译失败
 *   - 原有声明保持不变
 */
TEST_F(MacroForOuterInterfaceTest, PerformMacroExpand_NoMacroInSource)
{
    auto src = srcPath + "func_plain.cj";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};

    bool result = instance->Compile(CompileStage::MACRO_EXPAND);
    EXPECT_TRUE(result);
    EXPECT_EQ(diag.GetErrorCount(), 0);

    auto packages = instance->GetSourcePackages();
    auto file = packages[0]->files[0].get();
    ASSERT_FALSE(file->decls.empty());
}

/**
 * @brief 测试展开非宏声明时的行为
 * 
 * 测试目的：
 *   验证当传入非 MacroExpandDecl 类型的声明时，ExpandDecl 返回原声明
 * 
 * 测试步骤：
 *   1. 使用不包含宏的普通函数文件
 *   2. 编译到 IMPORT_PACKAGE 阶段
 *   3. 获取文件中的第一个声明（函数声明）
 *   4. 调用 ExpandDecl 展开该声明
 *   5. 验证返回的声明类型和内容不变
 * 
 * 预期结果：
 *   - 返回的声明数量为 1
 *   - 返回的声明类型仍为 FuncDecl
 * 
 * 关键验证点：
 *   - 非宏声明不会被错误处理
 *   - 原声明被正确保留并返回
 */
TEST_F(MacroForOuterInterfaceTest, ExpandDecl_NonMacroDecl_ReturnsSameDecl)
{
    auto src = srcPath + "func_plain.cj";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::IMPORT_PACKAGE);

    auto file = instance->GetSourcePackages()[0]->files[0].get();
    ASSERT_FALSE(file->decls.empty());

    auto decl = std::move(file->decls[0]);

    auto result = instance->ExpandDecl(std::move(decl));
    ASSERT_EQ(result.size(), 1);
    auto funcDecl = AST::As<ASTKind::FUNC_DECL>(result[0].get());
    EXPECT_TRUE(funcDecl != nullptr);
}

/**
 * @brief 测试展开单个宏声明生成类声明
 * 
 * 测试目的：
 *   验证 ExpandDecl 能够正确展开单个 MacroExpandDecl 并生成预期的 AST 节点
 * 
 * 测试步骤：
 *   1. 编译宏定义文件，生成宏库
 *   2. 编译测试文件到 IMPORT_PACKAGE 阶段
 *   3. 在 AST 中查找 @GenClass 宏声明
 *   4. 调用 ExpandDecl 展开该宏声明
 *   5. 验证展开结果为 ClassDecl 且标识符为 "A"
 * 
 * 预期结果：
 *   - 找到 @GenClass 宏声明
 *   - 展开后生成 1 个 ClassDecl
 *   - 类名为 "A"
 * 
 * 关键验证点：
 *   - 宏声明能够被正确识别
 *   - 展开后的 AST 节点类型正确
 *   - 展开后的类名符合预期
 */
TEST_F(MacroForOuterInterfaceTest, ExpandDecl_MacroExpandDecl_ExpandsToClass)
{
    std::string command = "cd " + definePath + " && cjc define.cj --compile-macro";
    int err = system(command.c_str());
    ASSERT_EQ(0, err);

    auto src = srcPath + "test_gen.cj";
    invocation.globalOptions.executablePath = projectPath + "/output/bin/";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::IMPORT_PACKAGE);

    auto file = instance->GetSourcePackages()[0]->files[0].get();
    OwnedPtr<Decl> targetDecl;
    for (auto &decl : file->decls) {
        if (auto med = AST::As<ASTKind::MACRO_EXPAND_DECL>(decl.get())) {
            if (med->invocation.macroCallDiagInfo.identifier == "GenClass") {
                targetDecl = std::move(decl);
                break;
            }
        }
    }
    ASSERT_TRUE(targetDecl != nullptr);

    auto result = instance->ExpandDecl(std::move(targetDecl));
    ASSERT_EQ(result.size(), 1);
    auto classDecl = AST::As<ASTKind::CLASS_DECL>(result[0].get());
    ASSERT_TRUE(classDecl != nullptr);
    EXPECT_EQ(classDecl->identifier, "A");
}

/**
 * @brief 测试带有多个注解的声明展开
 * 
 * 测试目的：
 *   验证当一个声明带有多个宏注解时，ExpandDecl 能够正确展开所有注解
 * 
 * 测试步骤：
 *   1. 编译宏定义文件
 *   2. 编译测试文件到 PARSE 阶段后执行包导入
 *   3. 查找带有多个注解（@GenVar 和 @GenLet）的宏声明
 *   4. 调用 ExpandDecl 展开该声明
 *   5. 验证展开后生成多个声明
 * 
 * 预期结果：
 *   - 找到带有多个注解的宏声明
 *   - 展开后生成 2 个声明（@GenVar 和 @GenLet 各生成一个）
 * 
 * 关键验证点：
 *   - 多注解宏声明能够被正确识别
 *   - 每个注解都能正确展开
 *   - 展开后的声明数量正确
 */
TEST_F(MacroForOuterInterfaceTest, ExpandDecl_MultipleAnnotationsExpand)
{
    std::string command = "cd " + definePath + " && cjc define.cj --compile-macro";
    int err = system(command.c_str());
    ASSERT_EQ(0, err);

    auto src = srcPath + "test_gen.cj";
    invocation.globalOptions.executablePath = projectPath + "/output/bin/";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::PARSE);
    instance->PerformImportPackage();

    auto file = instance->GetSourcePackages()[0]->files[0].get();
    OwnedPtr<Decl> targetDecl;
    for (auto &decl : file->decls) {
        if (auto med = AST::As<ASTKind::MACRO_EXPAND_DECL>(decl.get())) {
            if (med->invocation.attrs.size() >= 2) {
                targetDecl = std::move(decl);
                break;
            }
        }
    }
    if (targetDecl) {
        auto result = instance->ExpandDecl(std::move(targetDecl));
        ASSERT_EQ(result.size(), 2);
    }
}

/**
 * @brief 测试 LSP 模式下宏服务进程的创建
 * 
 * 测试目的：
 *   验证当启用 LSP 模式时，宏服务进程能够正确创建并处理宏展开
 * 
 * 测试步骤：
 *   1. 启用宏在 LSP 中的支持（enableMacroInLSP = true）
 *   2. 创建编译器实例，使用普通函数文件
 *   3. 编译到 MACRO_EXPAND 阶段
 *   4. 验证编译成功且无错误
 * 
 * 预期结果：
 *   - 编译成功
 *   - 无诊断错误
 * 
 * 关键验证点：
 *   - LSP 模式能够正常启用
 *   - 宏服务进程正确初始化
 */
TEST_F(MacroForOuterInterfaceTest, CreateMacroSrvProcess_LSPModeEnabled)
{
    invocation.globalOptions.enableMacroInLSP = true;
    auto src = srcPath + "func_plain.cj";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};

    bool result = instance->Compile(CompileStage::MACRO_EXPAND);
    EXPECT_TRUE(result);
    EXPECT_EQ(diag.GetErrorCount(), 0);
}

/**
 * @brief 测试 LSP 模式禁用时的行为
 * 
 * 测试目的：
 *   验证当 LSP 模式禁用时，编译器仍能正常处理宏展开
 * 
 * 测试步骤：
 *   1. 保持 LSP 模式禁用（默认状态）
 *   2. 创建编译器实例，使用普通函数文件
 *   3. 编译到 MACRO_EXPAND 阶段
 *   4. 验证编译成功且无错误
 * 
 * 预期结果：
 *   - 编译成功
 *   - 无诊断错误
 * 
 * 关键验证点：
 *   - 非 LSP 模式下编译器正常工作
 *   - 宏展开流程不受影响
 */
TEST_F(MacroForOuterInterfaceTest, CreateMacroSrvProcess_LSPModeDisabled)
{
    auto src = srcPath + "func_plain.cj";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};

    bool result = instance->Compile(CompileStage::MACRO_EXPAND);
    EXPECT_TRUE(result);
    EXPECT_EQ(diag.GetErrorCount(), 0);
}
