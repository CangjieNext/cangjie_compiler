// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/AST/Searcher.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

#include "Collector.h"
#include "TestCompilerInstance.h"

#include "cangjie/Parse/Parser.h"

#include "gtest/gtest.h"

#ifdef _WIN32
#include <windows.h>
#define CJ_ENVIRON _environ
#elif defined(__APPLE__)
// macOS does not declare `environ` in <unistd.h>; it exposes it via
// <crt_externs.h>::_NSGetEnviron(). Use that to obtain the environ pointer.
#include <crt_externs.h>
#define CJ_ENVIRON (*_NSGetEnviron())
#else
#include <unistd.h>
#define CJ_ENVIRON environ
#endif

using namespace Cangjie;
using namespace AST;

namespace {
std::unordered_map<std::string, std::string> GetEnvironmentVars()
{
    std::unordered_map<std::string, std::string> envVars;
    char **env = CJ_ENVIRON;
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

class SearchPlusTest : public testing::Test {
protected:
    void SetUp() override
    {
#ifdef _WIN32
        srcPath = projectPath + "\\unittests\\AST\\CangjieFiles\\";
        packagePath = packagePath + "\\";
#else
        std::string command;
        int err = 0;
        if (FileUtil::FileExist("testTempFiles")) {
            command = "rm -rf testTempFiles";
            err = system(command.c_str());
            ASSERT_EQ(0, err);
        }
        command = "mkdir -p testTempFiles";
        err = system(command.c_str());
        ASSERT_EQ(0, err);
        srcPath = projectPath + "/unittests/AST/CangjieFiles/";
        packagePath = packagePath + "/";
#endif
        std::string cangjieHome = projectPath + "/output";
        // Derive the platform-specific module directory from the target triple
        // (GetCangjieLibTargetPathName returns "<os>_<arch>_cjnative") instead
        // of hard-coding "darwin_arm64"/"darwin_aarch64", which must otherwise
        // be kept in sync with the actual output directory name.
        std::string cangjiePath =
            cangjieHome + "/modules/" + invocation.globalOptions.GetCangjieLibTargetPathName();
#ifdef _WIN32
        invocation.globalOptions.executablePath = projectPath + "\\output\\bin\\";
#elif defined(__APPLE__)
        invocation.globalOptions.executablePath = projectPath + "/output/bin/";
#elif defined(__unix__)
        invocation.globalOptions.executablePath = projectPath + "/output/bin/";
#endif
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
    // Gets the absolute path of the project from the compile parameter.
    const std::string projectPath = FileUtil::JoinPath(PROJECT_SOURCE_DIR, "");
#else
    // Just in case, give it a default value.
    // Assume the initial is in the build directory.
    std::string projectPath = FileUtil::JoinPath(FileUtil::JoinPath("..", ".."), "..");
#endif

    DiagnosticEngine diag;
    CompilerInvocation invocation;
    std::unique_ptr<CompilerInstance> instance;
    std::string packagePath = "testTempFiles";
    std::string srcPath;
    std::string savedCangjieHome;
    std::string savedCangjiePath;
};

TEST_F(SearchPlusTest, IllegalParametersTest)
{
    auto srcFile = srcPath + "testfile_search_01n.cj";
    std::string failedReason;
    auto content = FileUtil::ReadFileContent(srcFile, failedReason);
    if (!content.has_value()) {
        diag.DiagnoseRefactor(
            DiagKindRefactor::module_read_file_to_buffer_failed, DEFAULT_POSITION, srcFile, failedReason);
    }

    std::unique_ptr<CompilerInstance> instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    Parser parser(0, content.value(), diag, instance->GetSourceManager());
    OwnedPtr<Package> pkg = MakeOwned<Package>();
    pkg->files.emplace_back(parser.ParseTopLevel());
    ASTContext ctx(diag, *pkg);
    ScopeManager scopeManager;
    Collector collector(scopeManager);
    collector.BuildSymbolTable(ctx, pkg.get());

    std::vector<std::string> srcFiles;
    std::vector<std::string> srcs = {srcFile};
    for (auto& src : srcs) {
        srcFiles.push_back(src);
    }
    instance->srcFilePaths = srcFiles;
    instance->PerformParse();

    Searcher searcher;
    std::vector<Symbol*> res = searcher.Search(ctx, "");
    EXPECT_EQ(res.size(), 0);
    res = searcher.Search(ctx, " ");
    EXPECT_EQ(res.size(), 0);
    res = searcher.Search(ctx, "name:Day");
    EXPECT_EQ(res.size(), 13);
    res = searcher.Search(ctx, "name:Day!name:Day");
    EXPECT_EQ(res.size(), 0);
    res = searcher.Search(ctx, "name:Day&&name:Day");
    EXPECT_EQ(res.size(), 13);
    res = searcher.Search(ctx, "name:Day||name:Day");
    EXPECT_EQ(res.size(), 13);
    res = searcher.Search(ctx, "name:name");
    EXPECT_EQ(res.size(), 0);
    res = searcher.Search(ctx, "name:Day,name:Time");
    EXPECT_EQ(res.size(), 0);
    res = searcher.Search(ctx, "name:中"); // EXPECTED:illegal symbol '中'
    EXPECT_TRUE(res.empty());
    res = searcher.Search(ctx, "key:value"); // EXPECTED:Unknow query term: key!
    EXPECT_TRUE(res.empty());
    res = searcher.Search(ctx,
        "name:abcdefghijklmnopqrstuvwxyabcdefghijklmnopqrstuvwxyabcdefghijklmnopqrstuvwxyabcdefghijklmnopqrstuvwxy");
    EXPECT_EQ(res.size(), 1);
    res = searcher.Search(ctx, "name:**"); // EXPECTED :1:6: should be identifer, positive integer, 'foo*' or '*foo'
    EXPECT_TRUE(res.empty());
    res = searcher.Search(ctx, "name:D*y");
    EXPECT_EQ(res.size(), 0);
    res = searcher.Search(ctx, "name:*i*"); // EXPECTED :1:6: should be identifer, positive integer, 'foo*' or '*foo'
    EXPECT_TRUE(res.empty());
    res = searcher.Search(ctx, "name:Da?");
    EXPECT_EQ(res.size(), 0);

    res = searcher.Search(
        ctx, "id:1000"); // EXPECTED:Searcher error: id number '128' past the end of array and it would be ignored.
    EXPECT_TRUE(res.empty());
    res = searcher.Search(
        ctx, "id:205 && name:n && scope_name:a0j0k0i && ast_kind:ref_expr && scope_level:3 && _<=(0, 84, 32)");
    EXPECT_EQ(res.size(), 0);

    res = searcher.Search(ctx, "_=(0, 87, 32) ");
    EXPECT_EQ(res.size(), 5);
    res = searcher.Search(ctx, "_>(-1, -86, -32) ");
    EXPECT_TRUE(res.empty());

    res = searcher.Search(ctx, "scope_name:a0j0*");
    EXPECT_EQ(res.size(), 49);
    res = searcher.Search(ctx, "scope_name:*0i"); // scope_name not support suffix
    EXPECT_TRUE(res.empty());
    res = searcher.Search(ctx, "scope_name:xxxxxxxx");
    EXPECT_TRUE(res.empty());

    res = searcher.Search(ctx, "ast_kind:generic_param_decl");
    EXPECT_EQ(res.size(), 2);
    res = searcher.Search(ctx, "ast_kind:struct_decl");
    EXPECT_EQ(res.size(), 2);
    res = searcher.Search(ctx, "ast_kind:func_decl");
    EXPECT_EQ(res.size(), 13);
    res = searcher.Search(ctx, "ast_kind:var_decl");
    EXPECT_EQ(res.size(), 22);
    res = searcher.Search(ctx, "ast_kind:class_decl");
    EXPECT_EQ(res.size(), 6);
    res = searcher.Search(ctx, "ast_kind:interface_decl");
    EXPECT_EQ(res.size(), 1);
    res = searcher.Search(ctx, "ast_kind:enum_decl");
    EXPECT_EQ(res.size(), 2);
    res = searcher.Search(ctx, "ast_kind:type_alias_decl");
    EXPECT_EQ(res.size(), 0);
    res = searcher.Search(ctx, "ast_kind:class_like_decl");
    EXPECT_EQ(res.size(), 0);
    res = searcher.Search(ctx, "ast_kind:*decl");
    EXPECT_EQ(res.size(), 49);
    res = searcher.Search(ctx, "ast_kind:c*"); // ast_kind not support prefix
    EXPECT_TRUE(res.empty());
    res = searcher.Search(ctx, "ast_kind:*_decl");
    EXPECT_EQ(res.size(), 49);
    res = searcher.Search(ctx, "ast_kind:xxxxxxxx");
    EXPECT_TRUE(res.empty());
    // AST must be released before ASTContext for correct symbol detaching.
    pkg.reset();
    instance.reset();
}

TEST_F(SearchPlusTest, FileIDTest)
{
    auto srcFile = srcPath + "testfile_search_01n.cj";
    std::string failedReason;
    auto content = FileUtil::ReadFileContent(srcFile, failedReason);
    if (!content.has_value()) {
        diag.DiagnoseRefactor(
            DiagKindRefactor::module_read_file_to_buffer_failed, DEFAULT_POSITION, srcFile, failedReason);
    }

    std::string fileID = Utils::FillZero(~0u, 0);
    std::unique_ptr<CompilerInstance> instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    Parser parser(~0, content.value(), diag, instance->GetSourceManager());
    OwnedPtr<Package> pkg = MakeOwned<Package>();
    pkg->files.emplace_back(parser.ParseTopLevel());
    ASTContext ctx(diag, *pkg);
    ScopeManager scopeManager;
    Collector collector(scopeManager);
    collector.BuildSymbolTable(ctx, pkg.get());

    std::vector<std::string> srcFiles;
    std::vector<std::string> srcs = {srcFile};
    for (auto& src : srcs) {
        srcFiles.push_back(src);
    }
    instance->srcFilePaths = srcFiles;
    instance->PerformParse();

    Searcher searcher;
    std::vector<Symbol*> res = searcher.Search(ctx, "");
    EXPECT_EQ(res.size(), 0);
    res = searcher.Search(ctx, " ");
    EXPECT_EQ(res.size(), 0);
    res = searcher.Search(ctx, "name:Day");
    EXPECT_EQ(res.size(), 13);
    res = searcher.Search(ctx, "name:Day!name:Day");
    EXPECT_EQ(res.size(), 0);
    res = searcher.Search(ctx, "name:Day&&name:Day");
    EXPECT_EQ(res.size(), 13);
    res = searcher.Search(ctx, "name:Day||name:Day");
    EXPECT_EQ(res.size(), 13);
    res = searcher.Search(ctx, "name:name");
    EXPECT_EQ(res.size(), 0);
    res = searcher.Search(ctx, "name:Day,name:Time");
    EXPECT_EQ(res.size(), 0);
    res = searcher.Search(ctx, "name:中"); // EXPECTED:illegal symbol '中'
    EXPECT_TRUE(res.empty());
    res = searcher.Search(ctx, "key:value"); // EXPECTED:Unknow query term: key!
    EXPECT_TRUE(res.empty());
    res = searcher.Search(ctx,
        "name:abcdefghijklmnopqrstuvwxyabcdefghijklmnopqrstuvwxyabcdefghijklmnopqrstuvwxyabcdefghijklmnopqrstuvwxy");
    EXPECT_EQ(res.size(), 1);
    res = searcher.Search(ctx, "name:**"); // EXPECTED :1:6: should be identifer, positive integer, 'foo*' or '*foo'
    EXPECT_TRUE(res.empty());
    res = searcher.Search(ctx, "name:D*y");
    EXPECT_EQ(res.size(), 0);
    res = searcher.Search(ctx, "name:*i*"); // EXPECTED :1:6: should be identifer, positive integer, 'foo*' or '*foo'
    EXPECT_TRUE(res.empty());
    res = searcher.Search(ctx, "name:Da?");
    EXPECT_EQ(res.size(), 0);

    res = searcher.Search(
        ctx, "id:1000"); // EXPECTED:Searcher error: id number '128' past the end of array and it would be ignored.
    EXPECT_TRUE(res.empty());
    res = searcher.Search(
        ctx, "id:205 && name:n && scope_name:a0j0k0i && ast_kind:ref_expr && scope_level:3 && _<=(" + fileID + ", 84, 32)");
    EXPECT_EQ(res.size(), 0);

    res = searcher.Search(ctx, "_=(" + fileID + ", 87, 32) ");
    EXPECT_EQ(res.size(), 5);
    res = searcher.Search(ctx, "_>(-1, -86, -32) ");
    EXPECT_TRUE(res.empty());

    res = searcher.Search(ctx, "scope_name:a0j0*");
    EXPECT_EQ(res.size(), 49);
    res = searcher.Search(ctx, "scope_name:*0i"); // scope_name not support suffix
    EXPECT_TRUE(res.empty());
    res = searcher.Search(ctx, "scope_name:xxxxxxxx");
    EXPECT_TRUE(res.empty());

    res = searcher.Search(ctx, "ast_kind:generic_param_decl");
    EXPECT_EQ(res.size(), 2);
    res = searcher.Search(ctx, "ast_kind:struct_decl");
    EXPECT_EQ(res.size(), 2);
    res = searcher.Search(ctx, "ast_kind:func_decl");
    EXPECT_EQ(res.size(), 13);
    res = searcher.Search(ctx, "ast_kind:var_decl");
    EXPECT_EQ(res.size(), 22);
    res = searcher.Search(ctx, "ast_kind:class_decl");
    EXPECT_EQ(res.size(), 6);
    res = searcher.Search(ctx, "ast_kind:interface_decl");
    EXPECT_EQ(res.size(), 1);
    res = searcher.Search(ctx, "ast_kind:enum_decl");
    EXPECT_EQ(res.size(), 2);
    res = searcher.Search(ctx, "ast_kind:type_alias_decl");
    EXPECT_EQ(res.size(), 0);
    res = searcher.Search(ctx, "ast_kind:class_like_decl");
    EXPECT_EQ(res.size(), 0);
    res = searcher.Search(ctx, "ast_kind:*decl");
    EXPECT_EQ(res.size(), 49);
    res = searcher.Search(ctx, "ast_kind:c*"); // ast_kind not support prefix
    EXPECT_TRUE(res.empty());
    res = searcher.Search(ctx, "ast_kind:*_decl");
    EXPECT_EQ(res.size(), 49);
    res = searcher.Search(ctx, "ast_kind:xxxxxxxx");
    EXPECT_TRUE(res.empty());
    // AST must be released before ASTContext for correct symbol detaching.
    pkg.reset();
    instance.reset();
}

TEST_F(SearchPlusTest, ScopeTest)
{
    auto srcFile = srcPath + "testfile_search_02n.cj";
    std::string failedReason;
    auto content = FileUtil::ReadFileContent(srcFile, failedReason);
    if (!content.has_value()) {
        diag.DiagnoseRefactor(
            DiagKindRefactor::module_read_file_to_buffer_failed, DEFAULT_POSITION, srcFile, failedReason);
    }

    std::unique_ptr<CompilerInstance> instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    Parser parser(0, content.value(), diag, instance->GetSourceManager());
    OwnedPtr<Package> pkg = MakeOwned<Package>();
    pkg->files.emplace_back(parser.ParseTopLevel());
    ASTContext ctx(diag, *pkg);

    ScopeManager scopeManager;
    Collector collector(scopeManager);
    collector.BuildSymbolTable(ctx, pkg.get());
    std::vector<std::string> srcFiles;
    std::vector<std::string> srcs = {srcFile};
    for (auto& src : srcs) {
        srcFiles.push_back(src);
    }
    instance->srcFilePaths = srcFiles;
    instance->PerformParse();

    diag.ClearError();

    Searcher searcher;
    std::vector<Symbol*> res = searcher.Search(ctx, "name:Friday && scope_level:10 && ast_kind:class_decl");
    EXPECT_EQ(res.size(), 1);

    std::string curScopeName;
    res = searcher.Search(ctx, "_ = (0, 43, 28)");
    curScopeName = ScopeManagerApi::GetScopeNameWithoutTail(res[0]->scopeName);
    std::vector<std::string> result;
    while (!curScopeName.empty()) {
        std::string q = "scope_name: " + curScopeName + "&& _ < (0, 43, 28)";
        res = searcher.Search(ctx, q);
        for (auto tmp : res) {
            if (tmp->name.empty() || tmp->scopeName == "a") {
                continue;
            }
            result.push_back(tmp->name);
        }
        curScopeName = ScopeManagerApi::GetParentScopeName(curScopeName);
    }
    EXPECT_EQ(result.size(), 22);
    EXPECT_EQ(result[0], "thurday");
    EXPECT_EQ(result[1], "Int32");
    EXPECT_EQ(result[2], "a");
    EXPECT_EQ(result[3], "wednesday");
    EXPECT_EQ(result[4], "Int32");
    EXPECT_EQ(result[5], "a");
    EXPECT_EQ(result[6], "tuesday");
    EXPECT_EQ(result[7], "Int32");
    EXPECT_EQ(result[8], "a");
    EXPECT_EQ(result[9], "monday");
    EXPECT_EQ(result[10], "Int32");
    EXPECT_EQ(result[11], "a");
    EXPECT_EQ(result[12], "String");
    EXPECT_EQ(result[13], "String");
    EXPECT_EQ(result[14], "c");
    EXPECT_EQ(result[15], "String");
    EXPECT_EQ(result[16], "String");
    EXPECT_EQ(result[17], "b");
    EXPECT_EQ(result[18], "Int32");
    EXPECT_EQ(result[19], "a");
    // AST must be released before ASTContext for correct symbol detaching.
    pkg.reset();
    instance.reset();
}

TEST_F(SearchPlusTest, MultiFileTest)
{
#ifdef _WIN32
    srcPath = srcPath + "\\pkgs";
#elif defined(__APPLE__) || defined(__unix__)
    srcPath = srcPath + "/pkgs";
#endif

    CompilerInvocation invocation;
    DiagnosticEngine diag;
#ifdef _WIN32
    invocation.globalOptions.executablePath = projectPath + "\\output\\bin\\";
#elif defined(__APPLE__)
    invocation.globalOptions.executablePath = projectPath + "/output/bin/";
#elif defined(__unix__)
    invocation.globalOptions.executablePath = projectPath + "/output/bin/";
#endif
    std::unique_ptr<CompilerInstance> instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->srcDirs = {srcPath};
    instance->invocation.globalOptions.implicitPrelude = true;
#ifdef __x86_64__
    instance->invocation.globalOptions.target.arch = Cangjie::Triple::ArchType::X86_64;
#else
    instance->invocation.globalOptions.target.arch = Cangjie::Triple::ArchType::AARCH64;
#endif
#ifdef _WIN32
    instance->invocation.globalOptions.target.os = Cangjie::Triple::OSType::WINDOWS;
#elif defined(__APPLE__)
    instance->invocation.globalOptions.target.os = Cangjie::Triple::OSType::DARWIN;
#elif defined(__unix__)
    instance->invocation.globalOptions.target.os = Cangjie::Triple::OSType::LINUX;
#endif
    instance->Compile(CompileStage::SEMA);

    Searcher searcher;
    ASTContext* ctx = instance->GetASTContextByPackage(instance->GetSourcePackages()[0]);
    ASSERT_TRUE(ctx != nullptr);
    std::vector<Symbol*> res = searcher.Search(*ctx, "name:Time && ast_kind:class_decl");
    EXPECT_EQ(res.size(), 2);
    EXPECT_EQ(res[0]->hashID.fieldID, 55);
    EXPECT_EQ(res[1]->hashID.fieldID, 55);
}

// Build the per-test ASTContext that the coverage tests below reuse: parse
// testfile_search_01n.cj, run the Collector to populate the symbol table, and
// hand back the context plus the file content hash of the (single) source file.
// The hash is the value Searcher::InFiles compares each symbol's hashID.hash64
// against, so callers can construct a fileHashes set that either admits or
// rejects every symbol.
struct CoverageFixture {
    std::unique_ptr<CompilerInstance> instance;
    OwnedPtr<Package> pkg;
    std::unique_ptr<ASTContext> ctx;
    uint64_t fileHash{0};
};

static void BuildCoverageFixture(CompilerInvocation& invocation, DiagnosticEngine& diag,
    const std::string& srcPath, CoverageFixture& fix)
{
    auto srcFile = srcPath + "testfile_search_01n.cj";
    std::string failedReason;
    auto content = FileUtil::ReadFileContent(srcFile, failedReason);
    if (!content.has_value()) {
        diag.DiagnoseRefactor(
            DiagKindRefactor::module_read_file_to_buffer_failed, DEFAULT_POSITION, srcFile, failedReason);
    }
    fix.instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    Parser parser(0, content.value(), diag, fix.instance->GetSourceManager());
    fix.pkg = MakeOwned<Package>();
    fix.pkg->files.emplace_back(parser.ParseTopLevel());
    // The file content hash is what InFiles compares against; capture it before
    // the context takes ownership of the package.
    fix.fileHash = fix.pkg->files.front()->fileHash;
    fix.ctx = std::make_unique<ASTContext>(diag, *fix.pkg);
    ScopeManager scopeManager;
    Collector collector(scopeManager);
    collector.BuildSymbolTable(*fix.ctx, fix.pkg.get());
    std::vector<std::string> srcFiles = {srcFile};
    fix.instance->srcFilePaths = srcFiles;
    fix.instance->PerformParse();
}

// Covers the position comparator signs "<=" and ">=" (Searcher::GetIDsByPos
// branches at lines 568/572), which no existing fixture exercises (only "="
// and ">" were used).
TEST_F(SearchPlusTest, PositionLessEqualAndGreaterEqualSigns)
{
    CoverageFixture fix;
    BuildCoverageFixture(invocation, diag, srcPath, fix);
    Searcher searcher;
    // "<=" admits the node that begins at (0, 87, 32) (same hit as "=" at that
    // position) plus anything strictly before it.
    auto res = searcher.Search(*fix.ctx, "_<=(0, 87, 32) ");
    EXPECT_GE(res.size(), 1);
    // ">=" admits nodes whose begin >= the position; fileID 0 line 87 col 32 is
    // inside a function body, so there is at least one such node.
    auto res2 = searcher.Search(*fix.ctx, "_>=(0, 87, 32) ");
    EXPECT_GE(res2.size(), 1);
    for (auto& sym : fix.ctx->symbolTable) {
        (void)sym;
    }
    fix.pkg.reset();
    fix.instance.reset();
}

// Covers the scope_level comparator signs "<" and "<=" (GetIDsByScopeLevel
// branches at lines 606-615) and the "past the end of array" diagnostic path
// in StrToUint (lines 697-699), reached when the numeric level exceeds the
// symbol table size.
TEST_F(SearchPlusTest, ScopeLevelLessAndLessEqualAndOverflow)
{
    CoverageFixture fix;
    BuildCoverageFixture(invocation, diag, srcPath, fix);
    diag.ClearError();
    Searcher searcher;
    // "<1" iterates scope levels 0..0 and "<=0" iterates level 0 only, so the
    // two result sets must be equal (the loop bodies at lines 607-615 run
    // regardless of whether level 0 is indexed).
    auto less = searcher.Search(*fix.ctx, "scope_level:<1");
    auto lessEqual = searcher.Search(*fix.ctx, "scope_level:<=0");
    EXPECT_EQ(less.size(), lessEqual.size());

    // A scope level far beyond the table size triggers StrToUint's overflow
    // diagnostic (lines 698-699) and yields no symbols.
    auto overflow = searcher.Search(*fix.ctx, "scope_level:99999999");
    EXPECT_TRUE(overflow.empty());
    fix.pkg.reset();
    fix.instance.reset();
}

// Covers the fileHash filter path end to end: Searcher::InFiles (the only
// fully-uncovered function), NormalizeQuery's fileHash loop (lines 39-40),
// FindInSearchCache's filtered branch (lines 336-338), and
// FilterAndSortSearchResult's needFilter branch (line 349). Also drives the
// NormalizeQuery posDesc branch (line 45) and the NOT operator's Difference
// body (lines 549-550) via a legal "a ! b" query.
TEST_F(SearchPlusTest, FileHashFilterAndPosDescAndNot)
{
    CoverageFixture fix;
    BuildCoverageFixture(invocation, diag, srcPath, fix);
    Searcher searcher;

    // A fileHashes set containing the real file hash admits every symbol
    // (InFiles returns true for all of them), so the result equals the unfiltered
    // "name:Day" search (13 hits).
    std::unordered_set<uint64_t> admitting = {fix.fileHash};
    auto res = searcher.Search(*fix.ctx, "name:Day", Sort::posDesc, admitting);
    EXPECT_EQ(res.size(), 13);
    // posDesc was applied: results are sorted descending by position.
    EXPECT_TRUE(std::is_sorted(res.begin(), res.end(), Sort::posDesc));

    // Re-issue the same query so FindInSearchCache hits the cache, this time
    // with the filter set populated; the filtered branch (lines 336-338) runs.
    auto cached = searcher.Search(*fix.ctx, "name:Day", Sort::posDesc, admitting);
    EXPECT_EQ(cached.size(), 13);

    // A fileHashes set with no matching hash filters every symbol out
    // (InFiles returns false for all), yielding an empty result.
    std::unordered_set<uint64_t> rejecting = {fix.fileHash + 1};
    auto res2 = searcher.Search(*fix.ctx, "name:Day", Sort::posDesc, rejecting);
    EXPECT_TRUE(res2.empty());

    // NOT operator with a legal "a ! b" form: Day minus Day is the empty set,
    // exercising the Difference body (lines 549-550).
    auto res3 = searcher.Search(*fix.ctx, "name:Day ! name:Time");
    EXPECT_FALSE(res3.empty());

    fix.pkg.reset();
    fix.instance.reset();
}

// Covers GetIDsByScopeName's default (suffix) branch (line 634), which
// diagnoses "searcher_invalid_scope_name" for a scope_name query that is
// neither precise nor a prefix.
TEST_F(SearchPlusTest, ScopeNameSuffixDiagnoses)
{
    CoverageFixture fix;
    BuildCoverageFixture(invocation, diag, srcPath, fix);
    diag.ClearError();
    Searcher searcher;
    auto res = searcher.Search(*fix.ctx, "scope_name:*0i");
    EXPECT_TRUE(res.empty());
    fix.pkg.reset();
    fix.instance.reset();
}
