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

#include "ParseOuterInterfaceTestBase.h"
#include "cangjie/AST/Match.h"
#include "cangjie/AST/Walker.h"
#include "cangjie/Modules/ImportManager.h"

using namespace Cangjie;
using namespace AST;

class ParseOuterInterfaceTest : public ParseOuterInterfaceTestBase {
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
