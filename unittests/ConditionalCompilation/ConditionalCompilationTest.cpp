// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <cstdio>
#include <iostream>
#include <string>

#include "gtest/gtest.h"
#include "TestCompilerInstance.h"
#include "cangjie/Basic/Version.h"
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
 * @brief Test basic conditional compilation functionality
 * 
 * Test purpose:
 *   Verify that the PerformConditionCompile interface can correctly handle conditional compilation directives
 * 
 * Test steps:
 *   1. Create compiler instance, set source file to os.cj
 *   2. Call Compile to compile to CONDITION_COMPILE stage
 *   3. Verify compilation succeeds with no errors
 *   4. Check AST structure integrity
 * 
 * Expected results:
 *   - Compilation succeeds with no diagnostic errors
 *   - AST contains package and file nodes
 *   - File contains declaration nodes
 * 
 * Key verification points:
 *   - Conditional compilation stage executes correctly (parseResult == true)
 *   - AST structure is complete (packages not empty, files not empty, decls not empty)
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
 * @brief Test conditional compilation after parsing
 * 
 * Test purpose:
 *   Verify behavior of explicitly calling PerformConditionCompile after PARSE stage
 * 
 * Test steps:
 *   1. Create compiler instance, set source file to os.cj
 *   2. First execute PARSE stage
 *   3. Record declaration count after parsing
 *   4. Explicitly call PerformConditionCompile
 *   5. Verify declaration count changes after conditional compilation
 * 
 * Expected results:
 *   - 3 declarations after parsing
 *   - Conditional compilation executes successfully
 *   - 2 declarations remain after conditional compilation (1 filtered out based on conditions)
 * 
 * Key verification points:
 *   - Parse stage executes correctly
 *   - Conditional compilation can filter out declarations that don't meet conditions
 *   - Declaration count changes as expected
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
    // os.cj guards the two `foo` declarations with `@When[os == "Linux"]` and
    // `@When[os == "Windows"]`. On Linux exactly one matches (2 decls remain);
    // on macOS the host os is "macOS", so neither matches and only `main`
    // remains (1 decl). The filtered-out count differs by platform, so the
    // expected remainder is platform-dependent.
#ifdef __APPLE__
    EXPECT_EQ(declCountAfter, 1);
#else
    EXPECT_EQ(declCountAfter, 2);
#endif
}

/**
 * @brief Test files without conditional compilation directives
 * 
 * Test purpose:
 *   Verify that PerformConditionCompile does not filter any declarations when source file contains no conditional compilation directives
 * 
 * Test steps:
 *   1. Create compiler instance using func_plain.cj without conditional compilation directives
 *   2. Execute PARSE stage
 *   3. Record declaration count after parsing
 *   4. Execute conditional compilation
 *   5. Verify declaration count remains unchanged
 * 
 * Expected results:
 *   - Conditional compilation executes successfully
 *   - Declaration count remains unchanged
 * 
 * Key verification points:
 *   - Files without conditional compilation directives are not affected
 *   - All declarations are preserved
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

// Mirror the way ConditionalCompilationImpl parses CANGJIE_VERSION: extract the
// leading major.minor.patch and compare to the literal version.cj anchors its ==
// guard against. CANGJIE_VERSION may carry a suffix (e.g. a coverage build passes
// -v <sdk-version>, possibly "0.0.1-git..."), while ParseVersion only keeps the
// first three numeric groups, so use sscanf instead of a raw string compare.
static bool VersionNormalizedEquals(const std::string& version, const char* expected)
{
    unsigned major = 0, minor = 0, patch = 0;
    unsigned eMajor = 0, eMinor = 0, ePatch = 0;
    if (sscanf(version.c_str(), "%u.%u.%u", &major, &minor, &patch) != 3 ||
        sscanf(expected, "%u.%u.%u", &eMajor, &eMinor, &ePatch) != 3) {
        return false;
    }
    return major == eMajor && minor == eMinor && patch == ePatch;
}

TEST_F(ConditionalCompilationTest, version_comparisons)
{
    // Exercises all six cjc_version comparison operators against the compile
    // time CANGJIE_VERSION. The five non-== guards ( > "0.0.0", >= "0.0.0",
    // < "99.0.0", <= "99.0.0", != "99.0.0") hold for any real SDK version and
    // every build keeps them. The == guard is anchored to the "0.0.1" literal in
    // version.cj: coverage/prebuilt builds may pass -v <sdk-version> instead of
    // the CMake default "0.0.1", so derive the expected count from the same
    // Cangjie::CANGJIE_VERSION global the evaluator reads instead of hard-coding
    // a version that only holds for the plain CMake default build.
    auto src = srcPath + "version.cj";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};

    bool parseResult = instance->Compile(CompileStage::PARSE);
    ASSERT_TRUE(parseResult);

    bool ccResult = instance->PerformConditionCompile();
    EXPECT_TRUE(ccResult);
    // 5 always-kept guarded funcs + main, plus the == guard iff the normalized
    // CANGJIE_VERSION is exactly what version.cj compares against.
    size_t expectedDecls = 6;
    if (VersionNormalizedEquals(Cangjie::CANGJIE_VERSION, "0.0.1")) {
        expectedDecls += 1;
    }
    EXPECT_EQ(instance->GetSourcePackages()[0]->files[0]->decls.size(), expectedDecls);
}

TEST_F(ConditionalCompilationTest, container_kinds_filtered)
{
    // Exercises the Walker dispatch over class/struct/interface/enum/extend/
    // prop/func-params bodies. The `@When[!test]` guards keep every node,
    // so all child containers are walked.
    auto src = srcPath + "container_kinds.cj";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};
    ASSERT_TRUE(instance->Compile(CompileStage::PARSE));

    auto& file = instance->GetSourcePackages()[0]->files[0];
    ASSERT_GT(file->decls.size(), 1);

    bool ccResult = instance->PerformConditionCompile();
    EXPECT_TRUE(ccResult);
    // 10 decls: 7 guarded by @When[!test] (kept) + 3 unguarded.
    EXPECT_EQ(file->decls.size(), 10);
}
#endif
