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

#include <cstdlib>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#define CJ_ENVIRON _environ
#elif defined(__APPLE__)
#include <crt_externs.h>
#define CJ_ENVIRON (*_NSGetEnviron())
#else
#include <unistd.h>
#define CJ_ENVIRON environ
#endif

#include "gtest/gtest.h"

#include "TestCompilerInstance.h"
#include "cangjie/AST/Match.h"
#include "cangjie/AST/Searcher.h"
#include "cangjie/Frontend/CompilerInstance.h"
#include "cangjie/Frontend/FrontendOptions.h"
#include "cangjie/FrontendTool/CjdCompilerInstance.h"
#include "cangjie/Macro/InvokeUtil.h"
#include "cangjie/Option/Option.h"
#include "cangjie/Utils/FileUtil.h"

using namespace Cangjie;
using namespace Cangjie::AST;

namespace {
std::string ProjectPath()
{
#ifdef PROJECT_SOURCE_DIR
    return PROJECT_SOURCE_DIR;
#else
    return "..";
#endif
}

std::unordered_map<std::string, std::string> GetEnvironmentVars()
{
    std::unordered_map<std::string, std::string> envVars;
    char** env = CJ_ENVIRON;
    while (env && *env) {
        std::string entry(*env);
        size_t pos = entry.find('=');
        if (pos != std::string::npos) {
            envVars[entry.substr(0, pos)] = entry.substr(pos + 1);
        }
        ++env;
    }
    return envVars;
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

// The .cj.d instance stops after sema: every backend stage is a no-op and only cjo saving runs.
TEST_F(FrontendEntryTest, CjdCompilerInstanceSkipsBackendStages)
{
    CjdCompilerInstance ci(invocation, diag);

    // Building the identifier trie is pointless for a declaration-only compile.
    EXPECT_FALSE(ci.IsBuildTrie());

    EXPECT_TRUE(ci.PerformDesugarAfterSema());
    EXPECT_TRUE(ci.PerformGenericInstantiation());
    EXPECT_TRUE(ci.PerformOverflowStrategy());
    EXPECT_TRUE(ci.PerformCHIRCompilation());
    EXPECT_TRUE(ci.PerformCodeGen());
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

/**
 * `GetExtendDecls` / `GetAllVisibleExtendMembers` are the LSP-facing entry points for extension
 * lookup. They need a semantically analysed package, so this fixture points the instance at the
 * built standard library the same way the Sema unit tests do.
 */
class ExtendMemberTest : public ::testing::Test {
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
#elif defined(__APPLE__)
        invocation.globalOptions.target.os = Cangjie::Triple::OSType::DARWIN;
#elif defined(__unix__)
        invocation.globalOptions.target.os = Cangjie::Triple::OSType::LINUX;
#endif
        const std::string cangjieHome = ProjectPath() + "/output";
        const std::string cangjiePath =
            cangjieHome + "/modules/" + invocation.globalOptions.GetCangjieLibTargetPathName();
        if (const char* oldHome = getenv("CANGJIE_HOME"); oldHome != nullptr) {
            savedHome = oldHome;
        }
        if (const char* oldPath = getenv("CANGJIE_PATH"); oldPath != nullptr) {
            savedPath = oldPath;
        }
#ifdef _WIN32
        _putenv_s("CANGJIE_HOME", cangjieHome.c_str());
        _putenv_s("CANGJIE_PATH", cangjiePath.c_str());
#else
        setenv("CANGJIE_HOME", cangjieHome.c_str(), 1);
        setenv("CANGJIE_PATH", cangjiePath.c_str(), 1);
#endif
        // A library build, so the package under test does not need a `main`.
        invocation.globalOptions.compilePackage = true;
        invocation.globalOptions.outputMode = GlobalOptions::OutputMode::STATIC_LIB;
        invocation.globalOptions.ReadPathsFromEnvironmentVars(GetEnvironmentVars());
        Cangjie::MacroProcMsger::GetInstance().CloseMacroSrv();
        instance = std::make_unique<TestCompilerInstance>(invocation, diag);
        // Extension lookup needs a semantically analysed package, which needs the core package.
        // Skip rather than fail where the standard library has not been built yet.
        stdlibAvailable = FileUtil::FileExist(FileUtil::JoinPath(cangjiePath, "std.cjo"));
    }

    void TearDown() override
    {
        instance.reset();
        // A variable that was not set before must be removed again, not set to "" - other tests
        // in this binary derive paths from it and the two are not the same thing.
#ifdef _WIN32
        _putenv_s("CANGJIE_HOME", savedHome.c_str());
        _putenv_s("CANGJIE_PATH", savedPath.c_str());
#else
        if (savedHome.empty()) {
            unsetenv("CANGJIE_HOME");
        } else {
            setenv("CANGJIE_HOME", savedHome.c_str(), 1);
        }
        if (savedPath.empty()) {
            unsetenv("CANGJIE_PATH");
        } else {
            setenv("CANGJIE_PATH", savedPath.c_str(), 1);
        }
#endif
    }

    CompilerInvocation invocation;
    DiagnosticEngine diag;
    std::unique_ptr<TestCompilerInstance> instance;
    std::string savedHome;
    std::string savedPath;
    bool stdlibAvailable = false;
};

TEST_F(ExtendMemberTest, VisibleExtendMembersComeFromExtensionsAndTheirInterfaces)
{
    if (!stdlibAvailable) {
        GTEST_SKIP() << "standard library not built under " << ProjectPath() << "/output/modules";
    }
    instance->code = R"(
        package pkg1
        interface Eqq {
            func g(): Unit
        }
        extend Int64 <: Eqq {
            public func g(): Unit {}
        }
        class A <: Eqq {
            public func g(): Unit {}
        }
        extend A {
            public func h(): Unit {}
        }
    )";
    instance->Compile(CompileStage::SEMA);
    auto pkgs = instance->GetSourcePackages();
    ASSERT_EQ(pkgs.size(), 1U);
    ASTContext* ctx = instance->GetASTContextByPackage(pkgs[0]);
    ASSERT_NE(ctx, nullptr);

    Searcher searcher;
    auto extendSyms = searcher.Search(*ctx, "ast_kind:extend_decl", Sort::posAsc);
    ASSERT_FALSE(extendSyms.empty());
    auto extendDecl = StaticAs<ASTKind::EXTEND_DECL>(extendSyms[0]->node);
    ASSERT_NE(extendDecl->extendedType, nullptr);

    // Looked up by Ty: `extend Int64 <: Eqq` contributes `g`.
    auto byTy = instance->GetAllVisibleExtendMembers(
        extendDecl->extendedType->GetTy(), *extendSyms[0]->node->curFile);
    bool foundG = false;
    for (auto member : byTy) {
        EXPECT_TRUE(member->astKind == ASTKind::FUNC_DECL || member->astKind == ASTKind::PROP_DECL);
        foundG = foundG || member->identifier.Val() == "g";
    }
    EXPECT_TRUE(foundG);
    EXPECT_FALSE(instance->GetExtendDecls(extendDecl->extendedType->GetTy()).empty());

    // Looked up by declaration: `extend A` contributes `h`.
    auto classSyms = searcher.Search(*ctx, "(ast_kind:class_decl && name:A)");
    ASSERT_FALSE(classSyms.empty());
    auto classDecl = RawStaticCast<InheritableDecl*>(classSyms[0]->node);
    auto byDecl = instance->GetAllVisibleExtendMembers(classDecl, *extendSyms[0]->node->curFile);
    bool foundH = false;
    for (auto member : byDecl) {
        EXPECT_TRUE(member->astKind == ASTKind::FUNC_DECL || member->astKind == ASTKind::PROP_DECL);
        foundH = foundH || member->identifier.Val() == "h";
    }
    EXPECT_TRUE(foundH);
    EXPECT_FALSE(instance->GetExtendDecls(classDecl).empty());
}
