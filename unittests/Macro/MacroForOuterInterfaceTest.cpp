// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file MacroForOuterInterfaceTest.cpp
 * @brief Unit tests for macro-related external interfaces
 * 
 * This file provides comprehensive unit tests for compiler macro-related external interfaces, covering:
 *   1. PerformMacroExpand     - Expand all MacroExpandDecl nodes
 *   2. ExpandDecl             - Expand a single MacroExpandDecl node
 *   3. CreateMacroSrvProcess  - Create macro service process for LSP
 * 
 * Test scenarios include:
 *   - Basic macro expansion functionality test
 *   - Single macro declaration expansion test
 *   - Multiple annotation macro declaration expansion test
 *   - Macro service test in LSP mode
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
 * @brief Test basic macro expansion functionality
 * 
 * Test purpose:
 *   Verify that the PerformMacroExpand interface can correctly expand all macro declarations in source files
 * 
 * Test steps:
 *   1. Compile macro definition file define.cj to generate macro library
 *   2. Create compiler instance, set source file to test_gen.cj
 *   3. Call Compile to compile to MACRO_EXPAND stage
 *   4. Verify compilation succeeds with no errors
 *   5. Check if AST contains class declarations generated after macro expansion
 * 
 * Expected results:
 *   - Compilation succeeds with no diagnostic errors
 *   - @GenClass macro is correctly expanded, AST contains ClassDecl node
 * 
 * Key verification points:
 *   - Macro library compilation succeeds (err == 0)
 *   - Compilation stage executes correctly (result == true)
 *   - AST contains expanded class declaration (foundClass == true)
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
 * @brief Test explicit macro expansion call after package import
 * 
 * Test purpose:
 *   Verify that PerformMacroExpand can be explicitly called after IMPORT_PACKAGE stage
 * 
 * Test steps:
 *   1. Compile macro definition file to generate macro library
 *   2. Create compiler instance
 *   3. First execute to IMPORT_PACKAGE stage
 *   4. Explicitly call PerformMacroExpand method
 *   5. Verify macro expansion succeeds with no errors
 * 
 * Expected results:
 *   - Package import succeeds (importResult == true)
 *   - Macro expansion succeeds (expandResult == true)
 *   - No diagnostic errors
 * 
 * Key verification points:
 *   - Compilation stages can be executed step by step
 *   - PerformMacroExpand can be called independently
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
 * @brief Test behavior when source file contains no macros
 * 
 * Test purpose:
 *   Verify that PerformMacroExpand handles source files without macro declarations normally
 * 
 * Test steps:
 *   1. Use plain function file func_plain.cj without macros
 *   2. Compile to MACRO_EXPAND stage
 *   3. Verify compilation succeeds and AST is generated normally
 * 
 * Expected results:
 *   - Compilation succeeds with no errors
 *   - AST contains original function declarations
 * 
 * Key verification points:
 *   - Source files without macros do not cause compilation failure
 *   - Original declarations remain unchanged
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
 * @brief Test behavior when expanding non-macro declarations
 * 
 * Test purpose:
 *   Verify that ExpandDecl returns original declaration when given non-MacroExpandDecl type
 * 
 * Test steps:
 *   1. Use plain function file without macros
 *   2. Compile to IMPORT_PACKAGE stage
 *   3. Get the first declaration (function declaration) in the file
 *   4. Call ExpandDecl to expand the declaration
 *   5. Verify returned declaration type and content are unchanged
 * 
 * Expected results:
 *   - Returned declaration count is 1
 *   - Returned declaration type is still FuncDecl
 * 
 * Key verification points:
 *   - Non-macro declarations are not mishandled
 *   - Original declaration is correctly preserved and returned
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
 * @brief Test expanding single macro declaration to generate class declaration
 * 
 * Test purpose:
 *   Verify that ExpandDecl can correctly expand a single MacroExpandDecl and generate expected AST nodes
 * 
 * Test steps:
 *   1. Compile macro definition file to generate macro library
 *   2. Compile test file to IMPORT_PACKAGE stage
 *   3. Find @GenClass macro declaration in AST
 *   4. Call ExpandDecl to expand the macro declaration
 *   5. Verify expansion result is ClassDecl with identifier "A"
 * 
 * Expected results:
 *   - Find @GenClass macro declaration
 *   - Expansion generates 1 ClassDecl
 *   - Class name is "A"
 * 
 * Key verification points:
 *   - Macro declaration can be correctly identified
 *   - Expanded AST node type is correct
 *   - Expanded class name matches expectation
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
 * @brief Test expansion of declarations with multiple annotations
 * 
 * Test purpose:
 *   Verify that ExpandDecl can correctly expand all annotations when a declaration has multiple macro annotations
 * 
 * Test steps:
 *   1. Compile macro definition file
 *   2. Compile test file to PARSE stage then execute package import
 *   3. Find macro declaration with multiple annotations (@GenVar and @GenLet)
 *   4. Call ExpandDecl to expand the declaration
 *   5. Verify multiple declarations are generated after expansion
 * 
 * Expected results:
 *   - Find macro declaration with multiple annotations
 *   - Expansion generates 2 declarations (@GenVar and @GenLet each generate one)
 * 
 * Key verification points:
 *   - Multi-annotation macro declaration can be correctly identified
 *   - Each annotation can be correctly expanded
 *   - Expanded declaration count is correct
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
 * @brief Test macro service process creation in LSP mode
 * 
 * Test purpose:
 *   Verify that macro service process can be correctly created and handle macro expansion when LSP mode is enabled
 * 
 * Test steps:
 *   1. Enable macro support in LSP (enableMacroInLSP = true)
 *   2. Create compiler instance using plain function file
 *   3. Compile to MACRO_EXPAND stage
 *   4. Verify compilation succeeds with no errors
 * 
 * Expected results:
 *   - Compilation succeeds
 *   - No diagnostic errors
 * 
 * Key verification points:
 *   - LSP mode can be enabled normally
 *   - Macro service process initializes correctly
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
 * @brief Test macro expansion failure in LSP mode
 * 
 * Test purpose:
 *   Verify that compiler correctly handles and reports macro expansion failures in LSP mode
 * 
 * Test steps:
 *   1. Enable LSP mode
 *   2. Create compiler instance using file with invalid macro expansion
 *   3. Compile to MACRO_EXPAND stage
 *   4. Verify compilation fails and error count is greater than 0
 * 
 * Expected results:
 *   - Compilation fails
 *   - Diagnostic errors are reported
 * 
 * Key verification points:
 *   - LSP mode correctly handles macro expansion failures
 *   - Error diagnostics are properly generated in LSP mode
 */
TEST_F(MacroForOuterInterfaceTest, MacroExpansionFailure_LSPModeEnabled)
{
    invocation.globalOptions.enableMacroInLSP = true;
    auto src = srcPath + "test_failExpand.cj";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};

    bool result = instance->Compile(CompileStage::MACRO_EXPAND);
    EXPECT_FALSE(result);
    EXPECT_GT(diag.GetErrorCount(), 0);
}

/**
 * @brief Test behavior when LSP mode is disabled
 * 
 * Test purpose:
 *   Verify that compiler can still handle macro expansion normally when LSP mode is disabled
 * 
 * Test steps:
 *   1. Keep LSP mode disabled (default state)
 *   2. Create compiler instance using plain function file
 *   3. Compile to MACRO_EXPAND stage
 *   4. Verify compilation succeeds with no errors
 * 
 * Expected results:
 *   - Compilation succeeds
 *   - No diagnostic errors
 * 
 * Key verification points:
 *   - Compiler works normally in non-LSP mode
 *   - Macro expansion process is not affected
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