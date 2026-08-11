// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * Covers the frontend entry points that the compiler only reaches through a driver flag:
 * the `--dump-*` actions, the frontend-only option table and the compiler instances that
 * override the default pipeline stages.
 */

#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "TestCompilerInstance.h"
#include "cangjie/Frontend/CompilerInstance.h"
#include "cangjie/Frontend/FrontendOptions.h"
#include "cangjie/FrontendTool/CjdCompilerInstance.h"
#include "cangjie/Option/Option.h"
#include "cangjie/Utils/FileUtil.h"

using namespace Cangjie;

namespace {
std::string ProjectPath()
{
#ifdef PROJECT_SOURCE_DIR
    return PROJECT_SOURCE_DIR;
#else
    return "..";
#endif
}
} // namespace

class FrontendEntryTest : public ::testing::Test {
protected:
    void SetUp() override
    {
#ifdef __x86_64__
        invocation.globalOptions.target.arch = Cangjie::Triple::ArchType::X86_64;
#else
        invocation.globalOptions.target.arch = Cangjie::Triple::ArchType::AARCH64;
#endif
#ifdef _WIN32
        invocation.globalOptions.target.os = Cangjie::Triple::OSType::WINDOWS;
#elif defined(__unix__)
        invocation.globalOptions.target.os = Cangjie::Triple::OSType::LINUX;
#endif
        invocation.globalOptions.compilePackage = true;
        invocation.globalOptions.compilationCachedPath = ".";
    }

    CompilerInvocation invocation;
    DiagnosticEngine diag;
};

// `CompilerInstance` ships no-op defaults for the stages a concrete instance is expected to
// override. Every shipped instance overrides them, so the defaults are only reachable from here.
TEST_F(FrontendEntryTest, CompilerInstanceDefaultStagesAreNoOps)
{
    TestCompilerInstance ci(invocation, diag);

    EXPECT_EQ(ci.GetFileByPath("no/such/file.cj"), nullptr);
    EXPECT_TRUE(ci.PerformCodeGen());
    EXPECT_TRUE(ci.PerformCjoSaving());
    EXPECT_TRUE(ci.PerformResultsSaving());

#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
    EXPECT_TRUE(ci.GetFileNameMap().empty());
    ci.GetFileNameMap().emplace(1U, "a.cj");
    EXPECT_EQ(ci.GetFileNameMap().size(), 1U);
    // The accessor hands back the live map, not a copy.
    EXPECT_EQ(ci.GetFileNameMap().at(1U), "a.cj");
#endif
}

/*
 * Compiling a .cj.d declaration file must stop after sema, which `CjdCompilerInstance` implements
 * by overriding every post-sema stage with a no-op. "Returns true" alone does not pin that down -
 * the inherited DefaultCompilerInstance implementations would also return true on an empty input.
 *
 * What distinguishes them is that each real implementation opens its own ProfileRecorder scope
 * (src/FrontendTool/DefaultCompilerInstance.cpp: "Desugar after Sema", "Generic Instantiation",
 * "Overflow Strategy", "CHIR", "CodeGen") while the .cj.d overrides return before doing anything.
 * So the test enables the timer and asserts none of those scopes appear: drop any one override and
 * its stage name shows up, and the test fails.
 */
TEST_F(FrontendEntryTest, CjdCompilerInstanceSkipsBackendStages)
{
    CjdCompilerInstance ci(invocation, diag);

    // Building the identifier trie is pointless for a declaration-only compile.
    EXPECT_FALSE(ci.IsBuildTrie());

    // The timer is a process-wide singleton with no reset, so compare against a baseline rather
    // than against the empty string - other tests in this binary may have recorded entries.
    Utils::ProfileRecorder::Enable(true, Utils::ProfileRecorder::Type::TIMER);
    const std::string before = Utils::ProfileRecorder::GetResult(Utils::ProfileRecorder::Type::TIMER);

    EXPECT_TRUE(ci.PerformDesugarAfterSema());
    EXPECT_TRUE(ci.PerformGenericInstantiation());
    EXPECT_TRUE(ci.PerformOverflowStrategy());
    EXPECT_TRUE(ci.PerformCHIRCompilation());
    EXPECT_TRUE(ci.PerformCodeGen());

    const std::string after = Utils::ProfileRecorder::GetResult(Utils::ProfileRecorder::Type::TIMER);
    Utils::ProfileRecorder::Enable(false, Utils::ProfileRecorder::Type::ALL);

    for (const char* stage : {"Desugar after Sema", "Generic Instantiation", "Overflow Strategy",
             "CHIR", "CodeGen"}) {
        EXPECT_EQ(before.find(stage), std::string::npos) << "stale entry for " << stage;
        EXPECT_EQ(after.find(stage), std::string::npos) << stage << " ran, so the stage was not skipped";
    }

    // Nothing was produced or consumed, and no stage reported a diagnostic.
    EXPECT_TRUE(ci.GetSourcePackages().empty());
    EXPECT_EQ(diag.GetErrorCount(), 0U);
}

TEST_F(FrontendEntryTest, CjdCompilerInstanceSavesResultsOnlyForChirOutput)
{
    // Both calls report success, so the observable difference is the profile entry: the saving
    // stage only opens its ProfileRecorder scope once it gets past the output-mode check.
    Utils::ProfileRecorder::Enable(true, Utils::ProfileRecorder::Type::TIMER);

    invocation.globalOptions.outputMode = GlobalOptions::OutputMode::STATIC_LIB;
    CjdCompilerInstance ci(invocation, diag);
    // Anything other than a CHIR output returns before touching the packages.
    EXPECT_TRUE(ci.PerformResultsSaving());
    EXPECT_EQ(Utils::ProfileRecorder::GetResult(Utils::ProfileRecorder::Type::TIMER).find("Save results"),
        std::string::npos);

    invocation.globalOptions.outputMode = GlobalOptions::OutputMode::CHIR;
    // Nothing has been parsed, so the cjo loop runs zero times and still reports success.
    EXPECT_TRUE(ci.PerformResultsSaving());
    EXPECT_NE(Utils::ProfileRecorder::GetResult(Utils::ProfileRecorder::Type::TIMER).find("Save results"),
        std::string::npos);

    Utils::ProfileRecorder::Enable(false, Utils::ProfileRecorder::Type::ALL);
}

TEST_F(FrontendEntryTest, DumpTokensPrintsEveryTokenOfEverySourceFile)
{
    TestCompilerInstance ci(invocation, diag);
    const std::string src = FileUtil::JoinPath(ProjectPath(), "unittests/Frontend/FullCompile/src/test001.cj");
    ASSERT_TRUE(FileUtil::FileExist(src));
    ci.srcFilePaths = {src};

    testing::internal::CaptureStdout();
    const bool ret = ci.DumpTokens();
    const std::string out = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(ret);
    EXPECT_FALSE(out.empty());
    // Every line carries a `<file:line:column>` position marker.
    EXPECT_NE(out.find(src + ":"), std::string::npos);
}

TEST_F(FrontendEntryTest, DumpTokensReportsUnreadableSourceFiles)
{
    TestCompilerInstance ci(invocation, diag);
    ci.srcFilePaths = {FileUtil::JoinPath(ProjectPath(), "unittests/Frontend/no_such_source.cj")};

    testing::internal::CaptureStdout();
    const bool ret = ci.DumpTokens();
    (void)testing::internal::GetCapturedStdout();

    EXPECT_FALSE(ret);
    EXPECT_GT(diag.GetErrorCount(), 0U);
}

TEST_F(FrontendEntryTest, DumpMacroPrintsTheRecordedTokenStreams)
{
    TestCompilerInstance ci(invocation, diag);
    ci.tokensEvalInMacro = {"@M expanded tokens"};

    testing::internal::CaptureStdout();
    ci.DumpMacro();
    const std::string out = testing::internal::GetCapturedStdout();

    EXPECT_NE(out.find("==== Start Dumping ===="), std::string::npos);
    EXPECT_NE(out.find("@M expanded tokens"), std::string::npos);
}

TEST_F(FrontendEntryTest, GetASTContextByPackageRejectsANullPackage)
{
    TestCompilerInstance ci(invocation, diag);
    EXPECT_EQ(ci.GetASTContextByPackage(nullptr), nullptr);
    // Nothing has been parsed, so no package has a context either.
    EXPECT_TRUE(ci.GetSourcePackages().empty());
}

// The incremental cache is skipped for every compilation that cannot be resumed incrementally.
TEST_F(FrontendEntryTest, CacheFileIsNotWrittenForNonIncrementalCompilations)
{
    {
        TestCompilerInstance ci(invocation, diag);
        ci.invocation.globalOptions.enIncrementalCompilation = false;
        EXPECT_TRUE(ci.UpdateAndWriteCachedInfoToDisk());
    }
    {
        // A `--dump-xxx` run produces no object code, so there is nothing worth caching.
        TestCompilerInstance ci(invocation, diag);
        ci.invocation.globalOptions.enIncrementalCompilation = true;
        ci.invocation.frontendOptions.dumpAction = FrontendOptions::DumpAction::DUMP_TOKENS;
        EXPECT_TRUE(ci.UpdateAndWriteCachedInfoToDisk());
    }
    {
        // Neither does compiling a declaration file.
        TestCompilerInstance ci(invocation, diag);
        ci.invocation.globalOptions.enIncrementalCompilation = true;
        ci.invocation.frontendOptions.dumpAction = FrontendOptions::DumpAction::NO_ACTION;
        ci.invocation.globalOptions.compileCjd = true;
        EXPECT_TRUE(ci.UpdateAndWriteCachedInfoToDisk());
    }
}

TEST_F(FrontendEntryTest, DumpSymbolsPrintsAWellFormedSymbolTable)
{
    TestCompilerInstance ci(invocation, diag);

    testing::internal::CaptureStdout();
    ci.DumpSymbols();
    const std::string out = testing::internal::GetCapturedStdout();

    // With nothing parsed the table is still emitted, with both sections empty.
    EXPECT_NE(out.find("\"packages\""), std::string::npos);
    EXPECT_NE(out.find("\"files\""), std::string::npos);
}

namespace {
/// `FrontendOptions::ParseOption` is protected; this exposes it to the test.
class ExposedFrontendOptions : public FrontendOptions {
public:
    using FrontendOptions::ParseOption;
};

/**
 * Builds the metadata an OptionArgInstance needs.
 *
 * The frontend-only flags (--dump-tokens, --dump-symbols, --typecheck, -d,
 * --deserialize-chir-and-dump) are declared in the FRONTEND group without GROUP(VISIBLE), so the
 * default build (CANGJIE_VISIBLE_OPTIONS_ONLY=ON) drops them from the recognised option list
 * entirely and no command line can reach them. Driving ParseOption with a hand-built OptionInfo
 * keeps this test meaningful in both build configurations - only the ID is consulted by the
 * action table under test.
 */
OptionTable::OptionInfo MakeOptionInfo(Options::ID id, const std::string& name)
{
    return OptionTable::OptionInfo{name, static_cast<uint16_t>(id), Options::Kind::FLAG,
        {Options::Backend::ALL}, {Options::Group::FRONTEND}, nullptr, {},
        Options::Occurrence::MULTIPLE_OCCURRENCE, "", Options::Visibility::VISIBLE};
}

bool ParseAction(ExposedFrontendOptions& opts, Options::ID id, const std::string& name,
    const std::string& value = "")
{
    const OptionTable::OptionInfo info = MakeOptionInfo(id, name);
    OptionArgInstance arg(info, name, value);
    auto res = opts.ParseOption(arg);
    return res.has_value() && res.value();
}
} // namespace

class FrontendOptionsTest : public ::testing::Test {
};

TEST_F(FrontendOptionsTest, DumpActionsAreRecognised)
{
    ExposedFrontendOptions tokens;
    ASSERT_TRUE(ParseAction(tokens, Options::ID::DUMP_TOKENS, "--dump-tokens"));
    EXPECT_EQ(tokens.dumpAction, FrontendOptions::DumpAction::DUMP_TOKENS);

    ExposedFrontendOptions symbols;
    ASSERT_TRUE(ParseAction(symbols, Options::ID::DUMP_SYMBOLS, "--dump-symbols"));
    EXPECT_EQ(symbols.dumpAction, FrontendOptions::DumpAction::DUMP_SYMBOLS);

    ExposedFrontendOptions typeCheck;
    ASSERT_TRUE(ParseAction(typeCheck, Options::ID::TYPE_CHECK, "--typecheck"));
    EXPECT_EQ(typeCheck.dumpAction, FrontendOptions::DumpAction::TYPE_CHECK);
}

TEST_F(FrontendOptionsTest, ScanDependencyAlsoSetsTheGlobalFlag)
{
    ExposedFrontendOptions opts;
    ASSERT_TRUE(ParseAction(opts, Options::ID::DUMP_DEPENDENT_PACKAGE, "--scan-dependency"));
    EXPECT_EQ(opts.dumpAction, FrontendOptions::DumpAction::DUMP_DEP_PKG);
    // GlobalOptions handles --scan-dependency too; the frontend override has to set it again.
    EXPECT_TRUE(opts.scanDepPkg);
}

TEST_F(FrontendOptionsTest, DeserializeChirAndDumpCarriesItsPathArgument)
{
    ExposedFrontendOptions opts;
    ASSERT_TRUE(ParseAction(
        opts, Options::ID::DESERIALIZE_CHIR_AND_DUMP, "--deserialize-chir-and-dump", "some/pkg.chir"));
    EXPECT_EQ(opts.dumpAction, FrontendOptions::DumpAction::DESERIALIZE_CHIR);
    EXPECT_TRUE(opts.chirDeserialize);
    EXPECT_TRUE(opts.dumpCHIR);
    EXPECT_EQ(opts.chirDeserializePath, "some/pkg.chir");
}

TEST_F(FrontendOptionsTest, CompileCjdIsAFrontendOnlyFlag)
{
    ExposedFrontendOptions opts;
    ASSERT_TRUE(ParseAction(opts, Options::ID::COMPILE_CJD, "-d"));
    EXPECT_TRUE(opts.compileCjd);
    // -d does not select a dump action.
    EXPECT_EQ(opts.dumpAction, FrontendOptions::DumpAction::NO_ACTION);
}

TEST_F(FrontendOptionsTest, UnknownFrontendOptionsFallBackToGlobalOptions)
{
    // --output-type is a GlobalOptions flag, so FrontendOptions::ParseOption must delegate.
    auto optTbl = CreateOptionTable(true);
    const std::string srcFile = FileUtil::JoinPath(ProjectPath(), "unittests/Option/main.cj");
    std::vector<std::string> argStrs{"cjc", "--output-type", "staticlib", srcFile};
    ArgList argList;
    ASSERT_TRUE(optTbl->ParseArgs(argStrs, argList));

    FrontendOptions opts;
    ASSERT_TRUE(opts.ParseFromArgs(argList));
    EXPECT_EQ(opts.outputMode, GlobalOptions::OutputMode::STATIC_LIB);
    EXPECT_EQ(opts.dumpAction, FrontendOptions::DumpAction::NO_ACTION);
}
