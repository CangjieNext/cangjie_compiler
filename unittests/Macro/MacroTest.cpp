// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <mutex>
#include <string>
#include "gtest/gtest.h"

#include "TestCompilerInstance.h"
#include "cangjie/AST/Clone.h"
#include "cangjie/AST/Match.h"
#include "cangjie/AST/Walker.h"
#include "cangjie/ConditionalCompilation/ConditionalCompilation.h"
#include "cangjie/Utils/FileUtil.h"

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

namespace {
std::unique_ptr<TestCompilerInstance> MakeInstance(
    CompilerInvocation& invocation, DiagnosticEngine& diag, const std::string& code)
{
    auto inst = std::make_unique<TestCompilerInstance>(invocation, diag);
    inst->invocation.globalOptions.implicitPrelude = true;
    inst->code = code;
    return inst;
}
}

class MacroTest : public testing::Test {
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
#elif defined(__APPLE__)
        invocation.globalOptions.target.os = Cangjie::Triple::OSType::DARWIN;
        invocation.globalOptions.executablePath = projectPath + "/output/bin/";
#elif defined(__unix__)
        invocation.globalOptions.target.os = Cangjie::Triple::OSType::LINUX;
        invocation.globalOptions.executablePath = projectPath + "/output/bin/";
#endif
        std::string cangjieHome = projectPath + "/output";
        // Derive the platform-specific module directory from the target triple
        // (GetCangjieLibTargetPathName returns "<os>_<arch>_cjnative") instead
        // of hard-coding "darwin_arm64"/"darwin_aarch64", which must otherwise
        // be kept in sync with the actual output directory name.
        std::string cangjiePath =
            cangjieHome + "/modules/" + invocation.globalOptions.GetCangjieLibTargetPathName();

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
    // Gets the absolute path of the project from the compile parameter.
    std::string projectPath = PROJECT_SOURCE_DIR;
#else
    // Just in case, give it a default value.
    // Assume the initial is in the build directory.
    std::string projectPath = "..";
#endif
    std::string srcPath;
    std::string definePath;
    std::string savedCangjieHome;
    std::string savedCangjiePath;
    DiagnosticEngine diag;
    CompilerInvocation invocation;
    std::unique_ptr<TestCompilerInstance> instance;

    // ImportManager friends this fixture; the TEST_F bodies live in derived classes
    // (friendship is not inherited), so route the private accessor through here.
    static const std::vector<std::string>& MacroImportStrings(
        const ImportManager& im, const std::string& pkgName)
    {
        return im.GetMacroImportStrings(pkgName);
    }

    // Compile `code` as one package in-process and export its cjo into `outDir`.
    // TestCompilerInstance::ParseCode bypasses the production parse assembler
    // (CompileStrategy::CheckPackageConsistency), so replicate its macro-import
    // aggregation (and macro-package marking, see MacroImportCjoRoundTrip) before
    // exporting. Returns the written cjo path.
    std::string BuildCjo(const std::string& code, const std::string& outDir, bool markMacro = true)
    {
        auto inst = MakeInstance(invocation, diag, code);
        (void)inst->Compile(CompileStage::IMPORT_PACKAGE);
        auto* srcPkg = &*inst->GetSourcePackages()[0];
        if (markMacro) {
            srcPkg->isMacroPackage = true;
        }
        for (auto& f : srcPkg->files) {
            for (auto& stmt : f->package->macroImports) {
                srcPkg->macroImports.push_back(ASTCloner::CloneMacroImportList(*stmt));
            }
        }
        std::vector<uint8_t> cjoData;
        inst->importManager->ExportAST(false, cjoData, *srcPkg);
        EXPECT_FALSE(cjoData.empty());
        auto cjoPath = FileUtil::JoinPath(outDir, srcPkg->fullPackageName + ".cjo");
        EXPECT_TRUE(FileUtil::WriteBufferToASTFile(cjoPath, cjoData));
        return cjoPath;
    }

    // Compile the shared `define.cj` macro plugin exactly once per process: the
    // plugin does not vary per test, so repeating the cjc invocation per test only
    // adds runtime and a PATH dependency to every case (UT-D-01~04, ExpandDecl_*).
    void BuildDefinePlugin()
    {
        static std::once_flag once;
        std::call_once(once, [this]() {
            std::string command = "cd " + definePath + " && cjc define.cj --compile-macro";
            EXPECT_EQ(std::system(command.c_str()), 0);
        });
    }
};

TEST_F(MacroTest, ForeignBlockMacroExpansion)
{
    BuildDefinePlugin();

    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {srcPath + "foreign_block.cj"};
    instance->Compile(CompileStage::PARSE);
    ASSERT_EQ(diag.GetErrorCount(), 0);
    instance->PerformImportPackage();
    instance->PerformMacroExpand();
    ASSERT_EQ(diag.GetErrorCount(), 0);

    const auto& decls = instance->GetSourcePackages()[0]->files[0]->decls;
    ASSERT_EQ(decls.size(), 5);
    const std::vector<std::string> names = {"first", "second", "third", "fourth", "ordinary"};
    for (size_t i = 0; i < decls.size(); ++i) {
        auto func = AST::As<ASTKind::FUNC_DECL>(decls[i].get());
        ASSERT_NE(func, nullptr);
        EXPECT_EQ(func->identifier, names[i]);
        EXPECT_EQ(func->TestAttr(Attribute::FOREIGN), i < 4);
        ASSERT_NE(func->funcBody, nullptr);
        EXPECT_EQ(func->funcBody->body == nullptr, i < 4);
    }
}

TEST_F(MacroTest, MacroProcess_Curfile)
{
    auto src = srcPath + "func.cj";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::PARSE);
    instance->PerformImportPackage();

    // Test mapping curfile in macro expansion.
    for (auto& decl : instance->GetSourcePackages()[0]->files[0]->decls) {
        if (auto med = AST::As<ASTKind::MACRO_EXPAND_DECL>(decl.get()); med) {
            med->invocation.newTokens = med->invocation.args;
        }
    }
    instance->PerformMacroExpand();

    EXPECT_EQ(diag.GetErrorCount(), 0);
}

TEST_F(MacroTest, MacroCall_GetNewPos)
{
    auto src = srcPath + "func_not_annotation.cj";
    invocation.globalOptions.enableMacroInLSP = true;
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::PARSE);
    auto file = instance->GetSourcePackages()[0]->files[0].get();

    // Test GetNewPos.
    for (auto& decl : file->decls) {
        if (auto med = AST::As<ASTKind::MACRO_EXPAND_DECL>(decl.get()); med) {
            med->invocation.newTokens = med->invocation.args;
            auto& tok = med->invocation.newTokens[0];
            tok.SetValuePos(tok.Value(), tok.Begin() + 1, tok.End() + 1);
            med->invocation.newTokensStr = "func test():Unit\n {\n return }";
        }
    }
    instance->PerformMacroExpand();
    for (auto& decl : file->decls) {
        if (!decl->curMacroCall) {
            continue;
        }
        auto macrocall = decl->curMacroCall;
        if (auto fd = AST::As<ASTKind::FUNC_DECL>(decl.get()); fd && macrocall) {
            // Given a position which could be 't'{1, 4, 6}, 'e'{1, 4, 7}, 's'{1, 4, 8}, 't'{1, 4, 9},
            auto srcPos = Position{1, 4, 8};
            // Get the new begin position of identifier "test" after @M.
            auto newPos = macrocall->GetMacroCallNewPos(srcPos);
            EXPECT_EQ(newPos.fileID, 1);
            EXPECT_EQ(newPos.line, 3);
            // Get the original begin position of identifier "test" in func.cj.
            auto pos = decl->GetMacroCallPos(newPos);
            auto dstPos = Position{1, 4, 6};
            ASSERT_TRUE(pos == dstPos);
            // Given a position {1, 3, 3} after @M, get an INVALID_POSITION.
            srcPos = Position{1, 3, 3};
            newPos = macrocall->GetMacroCallNewPos(srcPos);
            ASSERT_TRUE(newPos == INVALID_POSITION);
        }
    }
    // error: undeclared identifier 'M'
    EXPECT_EQ(diag.GetErrorCount(), 1);
}

TEST_F(MacroTest, MacroCall_GetEndPos)
{
    auto src = srcPath + "var.cj";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::PARSE);
    auto file = instance->GetSourcePackages()[0]->files[0].get();

    // Test GetEndPos.
    for (auto& decl : file->decls) {
        if (auto med = AST::As<ASTKind::MACRO_EXPAND_DECL>(decl.get()); med) {
            med->invocation.newTokens = med->invocation.args;
            auto& tok = med->invocation.newTokens[1];
            tok.SetValuePos(tok.Value(), tok.Begin() + 1, tok.End() + 1);
            med->invocation.newTokensStr = "var a = 1";
        }
    }
    instance->PerformMacroExpand();
    EXPECT_EQ(diag.GetErrorCount(), 0);
}

TEST_F(MacroTest, MacroCall_Complementatcion_For_LSP)
{
    invocation.globalOptions.enableMacroInLSP = true;
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {srcPath + "class.cj"};
    instance->Compile(CompileStage::PARSE);

    auto file = instance->GetSourcePackages()[0]->files[0].get();
    // Simulation scenario: macrocall M1 expand success.
    std::function<VisitAction(Ptr<Node>)> visitPre1 = [&](Ptr<Node> curNode) -> VisitAction {
        if (curNode->astKind == ASTKind::MACRO_EXPAND_DECL) {
            auto med = StaticAs<ASTKind::MACRO_EXPAND_DECL>(curNode);
            if (med->invocation.macroCallDiagInfo.identifier == "M1") {
                med->invocation.newTokens = med->invocation.attrs;
                med->invocation.newTokens.emplace_back(Token(TokenKind::SEMI, ";"));
                med->invocation.newTokens.insert(
                    med->invocation.newTokens.end(), med->invocation.args.begin(), med->invocation.args.end());
                med->invocation.newTokensStr =
                    "class Ca4{};class Ca5{\n    var ab = 3\n    Ca5(x:Int64){\n        this.ab\n    }\n}";
            }
            return VisitAction::SKIP_CHILDREN;
        }
        return VisitAction::WALK_CHILDREN;
    };
    Walker walker1(file, visitPre1);
    walker1.Walk();
    instance->PerformMacroExpand();

    // Test MacroCall Complementation for lsp.
    instance->PerformImportPackage();
    instance->PerformSema();
    std::function<VisitAction(Ptr<Node>)> visitPre2 = [&](Ptr<Node> curNode) -> VisitAction {
        if (curNode->astKind == ASTKind::FILE) {
            auto file = StaticAs<ASTKind::FILE>(curNode);
            for (auto& it : file->originalMacroCallNodes) {
                Walker(it.get(), visitPre2).Walk();
            }
        }
        return VisitAction::WALK_CHILDREN;
    };
    Walker walker2(file, visitPre2);
    walker2.Walk();

    // error: undeclared identifier 'M1'
    EXPECT_EQ(diag.GetErrorCount(), 1);
}

TEST_F(MacroTest, MacroCall_Check_For_LSP)
{
    Cangjie::ICE::TriggerPointSetter iceSetter(static_cast<int64_t>(Cangjie::ICE::UNITTEST_TP));
    auto defInstance = std::make_unique<TestCompilerInstance>(invocation, diag);
    defInstance->compileOnePackageFromSrcFiles = true;
    defInstance->srcFilePaths = {definePath + "define.cj", definePath + "define2.cj"};
    defInstance->Compile();

    diag.Reset();

    std::vector<uint8_t> astData;
    defInstance->importManager->ExportAST(false, astData, *defInstance->GetSourcePackages()[0]);
    std::string astFile = definePath + "define.cjo";
    ASSERT_TRUE(FileUtil::WriteBufferToASTFile(astFile, astData));

    invocation.globalOptions.enableMacroInLSP = true;
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {srcPath + "func_arg.cj"};
    instance->Compile(CompileStage::PARSE);

    auto file = instance->GetSourcePackages()[0]->files[0].get();
    // Simulation scenario: macrocall B1 expand success, macrocall B2 expand failed.
    std::function<VisitAction(Ptr<Node>)> visitPre1 = [&](Ptr<Node> curNode) -> VisitAction {
        if (curNode->astKind == ASTKind::MACRO_EXPAND_EXPR) {
            auto mee = StaticAs<ASTKind::MACRO_EXPAND_EXPR>(curNode);
            if (mee->invocation.macroCallDiagInfo.identifier == "B1") {
                mee->invocation.newTokens = mee->invocation.args;
                mee->invocation.newTokensStr = "6";
            }
            return VisitAction::SKIP_CHILDREN;
        }
        return VisitAction::WALK_CHILDREN;
    };
    Walker walker1(file, visitPre1);
    walker1.Walk();
    // Test MacroCall target for lsp.
    instance->PerformImportPackage();
    instance->PerformMacroExpand();

    instance->PerformSema();
    std::function<VisitAction(Ptr<Node>)> visitPre2 = [&](Ptr<Node> curNode) -> VisitAction {
        if (curNode->astKind == ASTKind::FILE) {
            auto file = StaticAs<ASTKind::FILE>(curNode);
            for (auto& it : file->originalMacroCallNodes) {
                Walker(it.get(), visitPre2).Walk();
            }
        }
        if (curNode->astKind == ASTKind::MACRO_EXPAND_EXPR) {
            // Targets can be found for both failed and successful macrocalls.
            auto mee = StaticAs<ASTKind::MACRO_EXPAND_EXPR>(curNode);
            EXPECT_TRUE(mee->invocation.target);
            auto fileID = mee->invocation.target->begin.fileID;
            auto path = instance->GetSourceManager().GetSource(fileID).path;
            if (mee->invocation.macroCallDiagInfo.identifier == "B1") {
#ifdef _WIN32
                EXPECT_EQ(path, "define\\define.cj");
#else
                EXPECT_EQ(path, "define/define.cj");
#endif
            }
            if (mee->invocation.macroCallDiagInfo.identifier == "B2") {
#ifdef _WIN32
                EXPECT_EQ(path, "define\\define2.cj");
#else
                EXPECT_EQ(path, "define/define2.cj");
#endif
            }
            return VisitAction::SKIP_CHILDREN;
        }
        return VisitAction::WALK_CHILDREN;
    };
    Walker walker2(file, visitPre2);
    walker2.Walk();
    // error: macro evaluation has failed for macro call 'B1'
    // error: macro evaluation has failed for macro call 'B2'
    EXPECT_EQ(diag.GetErrorCount(), 2);
}

TEST_F(MacroTest, MacroCall_Check_For_LSP_Paralle)
{
    Cangjie::ICE::TriggerPointSetter iceSetter(static_cast<int64_t>(Cangjie::ICE::UNITTEST_TP));
    auto defInstance = std::make_unique<TestCompilerInstance>(invocation, diag);
    defInstance->compileOnePackageFromSrcFiles = true;
#ifdef _WIN32
    invocation.globalOptions.macroLib.emplace_back("\\");
#else
    invocation.globalOptions.macroLib.emplace_back("./");
#endif
    defInstance->srcFilePaths = {definePath + "define.cj", definePath + "define2.cj"};
    defInstance->Compile();

    diag.Reset();

    std::vector<uint8_t> astData;
    defInstance->importManager->ExportAST(false, astData, *defInstance->GetSourcePackages()[0]);
    std::string astFile = definePath + "define.cjo";
    ASSERT_TRUE(FileUtil::WriteBufferToASTFile(astFile, astData));

    invocation.globalOptions.enableMacroInLSP = true;
    invocation.globalOptions.enableParallelMacro = true;
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {srcPath + "func_arg.cj"};
    instance->Compile(CompileStage::PARSE);

    auto file = instance->GetSourcePackages()[0]->files[0].get();
    // Simulation scenario: macrocall B1 expand success, macrocall B2 expand failed.
    std::function<VisitAction(Ptr<Node>)> visitPre1 = [&](Ptr<Node> curNode) -> VisitAction {
        if (curNode->astKind == ASTKind::MACRO_EXPAND_EXPR) {
            auto mee = StaticAs<ASTKind::MACRO_EXPAND_EXPR>(curNode);
            if (mee->invocation.macroCallDiagInfo.identifier == "B1") {
                mee->invocation.newTokens = mee->invocation.args;
                mee->invocation.newTokensStr = "6";
            }
            return VisitAction::SKIP_CHILDREN;
        }
        return VisitAction::WALK_CHILDREN;
    };
    Walker walker1(file, visitPre1);
    walker1.Walk();
    // Test MacroCall target for lsp.
    instance->PerformImportPackage();
    instance->PerformMacroExpand();

    instance->PerformSema();
    std::function<VisitAction(Ptr<Node>)> visitPre2 = [&](Ptr<Node> curNode) -> VisitAction {
        if (curNode->astKind == ASTKind::FILE) {
            auto file = StaticAs<ASTKind::FILE>(curNode);
            for (auto& it : file->originalMacroCallNodes) {
                Walker(it.get(), visitPre2).Walk();
            }
        }
        if (curNode->astKind == ASTKind::MACRO_EXPAND_EXPR) {
            // Targets can be found for both failed and successful macrocalls.
            auto mee = StaticAs<ASTKind::MACRO_EXPAND_EXPR>(curNode);
            EXPECT_TRUE(mee->invocation.target);
            auto fileID = mee->invocation.target->begin.fileID;
            auto path = instance->GetSourceManager().GetSource(fileID).path;
            if (mee->invocation.macroCallDiagInfo.identifier == "B1") {
#ifdef _WIN32
                EXPECT_EQ(path, "define\\define.cj");
#else
                EXPECT_EQ(path, "define/define.cj");
#endif
            }
            if (mee->invocation.macroCallDiagInfo.identifier == "B2") {
#ifdef _WIN32
                EXPECT_EQ(path, "define\\define2.cj");
#else
                EXPECT_EQ(path, "define/define2.cj");
#endif
            }
            return VisitAction::SKIP_CHILDREN;
        }
        return VisitAction::WALK_CHILDREN;
    };
    Walker walker2(file, visitPre2);
    walker2.Walk();
    // error: macro evaluation has failed for macro call 'B1'
    // error: macro evaluation has failed for macro call 'B2'
    // error: Cannot dlopen from the dynamic library
    auto errSize = 3;
    EXPECT_EQ(diag.GetErrorCount(), errSize);
}

#ifndef _WIN32
TEST_F(MacroTest, IfAvailable_In_LSP)
{
    auto src = srcPath + "test_IfAvailable_LSP.cj";
    invocation.globalOptions.enableMacroInLSP = true;
    invocation.globalOptions.enableParallelMacro = true;
    invocation.globalOptions.executablePath = projectPath + "/output/bin/";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::SEMA);
 
    Cangjie::MacroProcMsger::GetInstance().CloseMacroSrv();
}

TEST_F(MacroTest, MacroCall_Check_For_LSP_context)
{
    std::string command = "cd " + definePath + " && cjc define_childMessage.cj --compile-macro";
    int err = system(command.c_str());
    ASSERT_EQ(0, err);
    auto src = srcPath + "test_macroWithContext.cj";
    invocation.globalOptions.enableMacroInLSP = true;
    invocation.globalOptions.enableParallelMacro = true;
    invocation.globalOptions.executablePath = projectPath + "/output/bin/";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::SEMA);
    EXPECT_EQ(diag.GetErrorCount(), 0);

    Cangjie::MacroProcMsger::GetInstance().CloseMacroSrv();
}

TEST_F(MacroTest, MacroDiagReportForLsp)
{
    std::string command = "cd " + definePath + " && cjc define_report.cj --compile-macro";
    int err = system(command.c_str());
    ASSERT_EQ(0, err);
    err = system("echo $CANGJIE_HOME && echo $LD_LIBRARY_PATH && echo $PATH");
    ASSERT_EQ(0, err);

    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->invocation.globalOptions.enableMacroInLSP = true;
    invocation.globalOptions.executablePath = projectPath + "/output/bin/";
    instance->compileOnePackageFromSrcFiles = true;
    Warningln("exe path ", invocation.globalOptions.executablePath);
    instance->srcFilePaths = {srcPath + "func_report.cj"};
    instance->Compile(CompileStage::SEMA);

    EXPECT_EQ(diag.GetErrorCount(), 1);
    Cangjie::MacroProcMsger::GetInstance().CloseMacroSrv();
}

TEST_F(MacroTest, NoErrorInLSPMacro)
{
    BuildDefinePlugin();
 
    auto src = srcPath + "derive_enum.cj";
    invocation.globalOptions.enableMacroInLSP = true;
    invocation.globalOptions.executablePath = projectPath + "/output/bin/";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::SEMA);
 
    EXPECT_EQ(diag.GetErrorCount(), 0);
    Cangjie::MacroProcMsger::GetInstance().CloseMacroSrv();
}

TEST_F(MacroTest, NoErrorInDeriveEnum)
{
    auto src = srcPath + "derive_enum2.cj";
    invocation.globalOptions.enableMacroInLSP = true;
    invocation.globalOptions.executablePath = projectPath + "/output/bin/";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::SEMA);

    EXPECT_EQ(diag.GetErrorCount(), 0);
    Cangjie::MacroProcMsger::GetInstance().CloseMacroSrv();
}

#if !defined(__APPLE__)
// on mac, type_info emitted by cjc and dylib do not link to the same symbol so typecast failed.
TEST_F(MacroTest, MacroCall_HighLight_LSP)
{
    std::string command = "cd " + definePath + " && cjc define3.cj --compile-macro";
    int err = system(command.c_str());
    ASSERT_EQ(0, err);
    auto src = srcPath + "test_highlight.cj";
    invocation.globalOptions.executablePath = projectPath + "/output/bin/";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::SEMA);
    auto file = instance->GetSourcePackages()[0]->files[0].get();

    for (auto& decl : file->decls) {
        if (!decl->curMacroCall) {
            continue;
        }
        auto macrocall = decl->curMacroCall;
        if (auto cd = AST::As<ASTKind::CLASS_DECL>(decl.get()); cd && macrocall) {
            auto newTks = macrocall->GetInvocation()->newTokens;
            // class A
            ASSERT_TRUE(macrocall->GetMacroCallNewPos(Position{1, 13, 7}).isCurFile);
            ASSERT_EQ(macrocall->GetMacroCallNewPos(Position{1, 13, 7}), (Position{1, 11, 10}));
            // var a
            auto pos = Position{1, 15, 9};
            ASSERT_TRUE(macrocall->GetMacroCallNewPos(Position{1, 15, 9}).isCurFile);
            ASSERT_EQ(macrocall->GetMacroCallNewPos(Position{1, 15, 9}), (Position{1, 11, 20}));
            // identifier ttt, define in macro Rename
            ASSERT_EQ(newTks[12].Value(), "ttt");
        }
    }
}
#endif // !__APPLE__
#endif

// Single node expand to single node
TEST_F(MacroTest, ExpandDecl_WithMacroCall_ExpandsSuccessfully01)
{
    BuildDefinePlugin();

    auto src = srcPath + "test_gen.cj";
    invocation.globalOptions.executablePath = projectPath + "/output/bin/";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::PARSE);
    instance->PerformImportPackage();

    auto file = instance->GetSourcePackages()[0]->files[0].get();
    ASSERT_FALSE(file->decls.empty());

    auto result = instance->ExpandDecl(std::move(file->decls[0]));
    ASSERT_EQ(result.size(), 1);
    auto expandedDecl = AST::As<ASTKind::CLASS_DECL>(result[0].get());
    ASSERT_TRUE(expandedDecl != nullptr);
    EXPECT_EQ(expandedDecl->identifier, "A");
    ASSERT_TRUE(expandedDecl->body != nullptr);
    EXPECT_TRUE(expandedDecl->body->decls.empty());

    EXPECT_EQ(diag.GetErrorCount(), 0);
}

#if !defined(__APPLE__)
// on mac, type_info emitted by cjc and dylib do not link to the same symbol so typecast failed.
// Single node expand to multiple node
TEST_F(MacroTest, ExpandDecl_WithMacroCall_ExpandsSuccessfully02)
{
    BuildDefinePlugin();

    auto src = srcPath + "test_gen.cj";
    invocation.globalOptions.executablePath = projectPath + "/output/bin/";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::PARSE);
    instance->PerformImportPackage();

    auto file = instance->GetSourcePackages()[0]->files[0].get();
    ASSERT_FALSE(file->decls.empty());

    auto result = instance->ExpandDecl(std::move(file->decls[1]));
    auto expectResSize = 2;
    ASSERT_EQ(result.size(), expectResSize);
    auto expandedDecl = AST::As<ASTKind::ENUM_DECL>(result[0].get());
    ASSERT_TRUE(expandedDecl != nullptr);
    EXPECT_EQ(expandedDecl->constructors.size(), 1);

    auto extendDecl = AST::As<ASTKind::EXTEND_DECL>(result[1].get());
    ASSERT_TRUE(extendDecl != nullptr);
    auto expectMemberSize = 2;
    EXPECT_EQ(extendDecl->members.size(), expectMemberSize);

    EXPECT_EQ(diag.GetErrorCount(), 0);
}
#endif // !__APPLE__

// Multi node expand to multiple node
TEST_F(MacroTest, ExpandDecl_WithMacroCall_ExpandsSuccessfully03)
{
    BuildDefinePlugin();

    auto src = srcPath + "test_gen.cj";
    invocation.globalOptions.executablePath = projectPath + "/output/bin/";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::PARSE);
    instance->PerformImportPackage();

    auto file = instance->GetSourcePackages()[0]->files[0].get();
    ASSERT_FALSE(file->decls.empty());

    auto result = instance->ExpandDecl(std::move(file->decls[2]));
    ASSERT_EQ(result.size(), 1);
    auto expandedDecl = AST::As<ASTKind::CLASS_DECL>(result[0].get());
    ASSERT_TRUE(expandedDecl != nullptr);
    auto expectBodySize = 2;
    EXPECT_EQ(expandedDecl->body->decls.size(), expectBodySize);
    EXPECT_EQ(expandedDecl->body->decls[0]->identifier, "a_gen_var");
    EXPECT_EQ(expandedDecl->body->decls[1]->identifier, "a_gen_let");

    EXPECT_EQ(diag.GetErrorCount(), 0);
}

// Single node with multiple macro
TEST_F(MacroTest, ExpandDecl_WithMacroCall_ExpandsSuccessfully04)
{
    BuildDefinePlugin();

    auto src = srcPath + "test_gen.cj";
    invocation.globalOptions.executablePath = projectPath + "/output/bin/";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::PARSE);
    instance->PerformImportPackage();

    auto file = instance->GetSourcePackages()[0]->files[0].get();
    ASSERT_FALSE(file->decls.empty());

    auto result = instance->ExpandDecl(std::move(file->decls[3]));
    ASSERT_EQ(result.size(), 1);
    auto expandedDecl = AST::As<ASTKind::VAR_DECL>(result[0].get());
    ASSERT_TRUE(expandedDecl != nullptr);
    EXPECT_EQ(expandedDecl->identifier, "a_gen_var");

    EXPECT_EQ(diag.GetErrorCount(), 0);
}

// Nest macro on different nodes
#if !defined(__APPLE__)
// on mac, type_info emitted by cjc and dylib do not link to the same symbol so typecast failed.
TEST_F(MacroTest, ExpandDecl_WithMacroCall_ExpandsSuccessfully05)
{
    BuildDefinePlugin();

    auto src = srcPath + "test_gen.cj";
    invocation.globalOptions.executablePath = projectPath + "/output/bin/";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::PARSE);
    instance->PerformImportPackage();

    auto file = instance->GetSourcePackages()[0]->files[0].get();
    ASSERT_FALSE(file->decls.empty());

    auto result = instance->ExpandDecl(std::move(file->decls[4]));
    auto expectResSize = 2;
    ASSERT_EQ(result.size(), expectResSize);
    auto expandedDecl = AST::As<ASTKind::ENUM_DECL>(result[0].get());
    ASSERT_TRUE(expandedDecl != nullptr);
    EXPECT_EQ(expandedDecl->constructors.size(), 1);
    auto constructor = std::move(expandedDecl->constructors[0]);

    EXPECT_EQ(constructor->identifier, "M");

    EXPECT_EQ(diag.GetErrorCount(), 0);
}
#endif // !__APPLE__

// Generate new macro node
TEST_F(MacroTest, ExpandDecl_WithMacroCall_ExpandsSuccessfully06)
{
    BuildDefinePlugin();

    auto src = srcPath + "test_gen.cj";
    invocation.globalOptions.executablePath = projectPath + "/output/bin/";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::PARSE);
    instance->PerformImportPackage();

    auto file = instance->GetSourcePackages()[0]->files[0].get();
    ASSERT_FALSE(file->decls.empty());

    auto result = instance->ExpandDecl(std::move(file->decls[5]));
    ASSERT_EQ(result.size(), 1);
    auto expandedDecl = AST::As<ASTKind::VAR_DECL>(result[0].get());
    ASSERT_TRUE(expandedDecl != nullptr);
    EXPECT_EQ(expandedDecl->identifier, "a_gen_let");

    EXPECT_EQ(diag.GetErrorCount(), 0);
}

// Single Annotation
TEST_F(MacroTest, ExpandDecl_WithMacroCall_ExpandsSuccessfully07)
{
    BuildDefinePlugin();

    auto src = srcPath + "test_anno.cj";
    invocation.globalOptions.executablePath = projectPath + "/output/bin/";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::PARSE);
    instance->PerformImportPackage();

    auto file = instance->GetSourcePackages()[0]->files[0].get();
    ASSERT_FALSE(file->decls.empty());

    auto result = instance->ExpandDecl(std::move(file->decls[1]));
    ASSERT_EQ(result.size(), 1);
    auto expandedDecl = AST::As<ASTKind::CLASS_DECL>(result[0].get());
    ASSERT_TRUE(expandedDecl != nullptr);
    EXPECT_EQ(expandedDecl->identifier, "SingleAnno");
    EXPECT_EQ(expandedDecl->annotations.size(), 1);
    EXPECT_EQ(expandedDecl->annotations[0]->identifier, "LogWithLevel");

    EXPECT_EQ(diag.GetErrorCount(), 0);
}

// Macro nest annotation
TEST_F(MacroTest, ExpandDecl_WithMacroCall_ExpandsSuccessfully08)
{
    BuildDefinePlugin();

    auto src = srcPath + "test_anno.cj";
    invocation.globalOptions.executablePath = projectPath + "/output/bin/";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::PARSE);
    instance->PerformImportPackage();

    auto file = instance->GetSourcePackages()[0]->files[0].get();
    ASSERT_FALSE(file->decls.empty());

    auto result = instance->ExpandDecl(std::move(file->decls[2]));
    ASSERT_EQ(result.size(), 1);
    auto expandedDecl = AST::As<ASTKind::CLASS_DECL>(result[0].get());
    ASSERT_TRUE(expandedDecl != nullptr);
    EXPECT_EQ(expandedDecl->identifier, "A");
    EXPECT_EQ(expandedDecl->annotations.size(), 0);

    EXPECT_EQ(diag.GetErrorCount(), 0);
}

// Annotation nest macro
TEST_F(MacroTest, ExpandDecl_WithMacroCall_ExpandsSuccessfully09)
{
    BuildDefinePlugin();

    auto src = srcPath + "test_anno.cj";
    invocation.globalOptions.executablePath = projectPath + "/output/bin/";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::PARSE);
    instance->PerformImportPackage();

    auto file = instance->GetSourcePackages()[0]->files[0].get();
    ASSERT_FALSE(file->decls.empty());

    auto result = instance->ExpandDecl(std::move(file->decls[3]));
    ASSERT_EQ(result.size(), 1);
    auto expandedDecl = AST::As<ASTKind::CLASS_DECL>(result[0].get());
    ASSERT_TRUE(expandedDecl != nullptr);
    EXPECT_EQ(expandedDecl->identifier, "A");
    EXPECT_EQ(expandedDecl->annotations.size(), 1);
    EXPECT_EQ(expandedDecl->annotations[0]->identifier, "LogWithLevel");

    EXPECT_EQ(diag.GetErrorCount(), 0);
}

// Annotation nest child macro node
TEST_F(MacroTest, ExpandDecl_WithMacroCall_ExpandsSuccessfully10)
{
    BuildDefinePlugin();

    auto src = srcPath + "test_anno.cj";
    invocation.globalOptions.executablePath = projectPath + "/output/bin/";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::PARSE);
    instance->PerformImportPackage();

    auto file = instance->GetSourcePackages()[0]->files[0].get();
    ASSERT_FALSE(file->decls.empty());

    auto result = instance->ExpandDecl(std::move(file->decls[4]));
    ASSERT_EQ(result.size(), 1);
    auto expandedDecl = AST::As<ASTKind::CLASS_DECL>(result[0].get());
    ASSERT_TRUE(expandedDecl != nullptr);
    EXPECT_EQ(expandedDecl->identifier, "AnnoNestChildMacro");
    EXPECT_EQ(expandedDecl->annotations.size(), 1);
    EXPECT_EQ(expandedDecl->annotations[0]->identifier, "LogWithLevel");
    EXPECT_EQ(expandedDecl->body->decls.size(), 1);
    EXPECT_EQ(expandedDecl->body->decls[0]->identifier, "a_gen_var");

    EXPECT_EQ(diag.GetErrorCount(), 0);
}

// Expand MacroExpandExpr
TEST_F(MacroTest, ExpandDecl_WithMacroCall_ExpandsSuccessfully11)
{
    BuildDefinePlugin();

    auto src = srcPath + "test_gen.cj";
    invocation.globalOptions.executablePath = projectPath + "/output/bin/";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::PARSE);
    instance->PerformImportPackage();

    auto file = instance->GetSourcePackages()[0]->files[0].get();
    ASSERT_FALSE(file->decls.empty());

    auto result = instance->ExpandDecl(std::move(file->decls[6]));
    ASSERT_EQ(result.size(), 1);
    auto expandedDecl = AST::As<ASTKind::FUNC_DECL>(result[0].get());
    ASSERT_TRUE(expandedDecl != nullptr);
    EXPECT_EQ(expandedDecl->identifier, "ExpandExpr");
    auto expectBodySize = 3;
    EXPECT_EQ(expandedDecl->funcBody->body->body.size(), expectBodySize);
    auto varDecl = AST::As<ASTKind::VAR_DECL>(expandedDecl->funcBody->body->body[0]);
    EXPECT_EQ(varDecl->identifier, "a_gen_var");

    auto intDecl = AST::As<ASTKind::VAR_DECL>(expandedDecl->funcBody->body->body[1]);
    EXPECT_EQ(intDecl->identifier, "b");
    auto litExpr = AST::As<ASTKind::LIT_CONST_EXPR>(intDecl->initializer);
    EXPECT_EQ(litExpr->stringValue, "100");

    EXPECT_EQ(diag.GetErrorCount(), 0);
}

// Expand MacroExpandParam
TEST_F(MacroTest, ExpandDecl_WithMacroCall_ExpandsSuccessfully12)
{
    BuildDefinePlugin();

    auto src = srcPath + "test_gen.cj";
    invocation.globalOptions.executablePath = projectPath + "/output/bin/";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::PARSE);
    instance->PerformImportPackage();

    auto file = instance->GetSourcePackages()[0]->files[0].get();
    ASSERT_FALSE(file->decls.empty());

    auto result = instance->ExpandDecl(std::move(file->decls[7]));
    ASSERT_EQ(result.size(), 1);
    auto expandedDecl = AST::As<ASTKind::FUNC_DECL>(result[0].get());
    ASSERT_TRUE(expandedDecl != nullptr);
    EXPECT_EQ(expandedDecl->identifier, "ExpandParam");
    auto param = expandedDecl->funcBody->paramLists[0]->params[0].get();
    EXPECT_TRUE(param != nullptr);
    EXPECT_EQ(param->identifier, "param");

    EXPECT_EQ(diag.GetErrorCount(), 0);
}

TEST_F(MacroTest, ExpandDecl_WithVarMacroCall_ExpandsSuccessfully)
{
    BuildDefinePlugin();

    auto src = srcPath + "var.cj";
    invocation.globalOptions.executablePath = projectPath + "/output/bin/";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::PARSE);
    instance->PerformImportPackage();

    auto file = instance->GetSourcePackages()[0]->files[0].get();
    ASSERT_FALSE(file->decls.empty());

    auto result = instance->ExpandDecl(std::move(file->decls[0]));
    ASSERT_EQ(result.size(), 1);
    auto expandedDecl = AST::As<ASTKind::VAR_DECL>(result[0].get());
    ASSERT_TRUE(expandedDecl != nullptr);
    EXPECT_EQ(expandedDecl->identifier, "a");

    EXPECT_EQ(diag.GetErrorCount(), 0);
}

TEST_F(MacroTest, ExpandDecl_WithNoMacroCall_ReturnsSameDecl)
{
    BuildDefinePlugin();

    auto src = srcPath + "test_anno.cj";
    invocation.globalOptions.executablePath = projectPath + "/output/bin/";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::PARSE);
    instance->PerformImportPackage();

    auto file = instance->GetSourcePackages()[0]->files[0].get();
    ASSERT_FALSE(file->decls.empty());

    auto& decl = file->decls[1];
    auto result = instance->ExpandDecl(std::move(decl));
    EXPECT_EQ(result.size(), 1);
    auto expandedDecl = AST::As<ASTKind::CLASS_DECL>(result[0].get());
    ASSERT_TRUE(expandedDecl != nullptr);
    EXPECT_EQ(expandedDecl->identifier, "SingleAnno");
    EXPECT_EQ(expandedDecl->annotations.size(), 1);
    EXPECT_EQ(expandedDecl->annotations[0]->identifier, "LogWithLevel");
}

TEST_F(MacroTest, ExpandDecl_WithNullDecl_ReturnsEmpty)
{
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    auto result = instance->ExpandDecl(nullptr);
    EXPECT_TRUE(result.empty());
}

TEST_F(MacroTest, ExpandDecl_WithMemberDecl_ReturnsEmpty)
{
    auto src = srcPath + "class.cj";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::PARSE);

    auto file = instance->GetSourcePackages()[0]->files[0].get();

    for (auto& decl : file->decls) {
        if (auto cd = AST::As<ASTKind::CLASS_DECL>(decl.get()); cd && cd->body && !cd->body->decls.empty()) {
            auto vd = AST::As<ASTKind::VAR_DECL>(cd->body->decls[0].get());
            if (vd) {
                auto result = instance->ExpandDecl(std::move(cd->body->decls[0]));
                EXPECT_TRUE(result.empty());
                break;
            }
        }
    }
}

// Child macro expand failed, parent macro not expand
TEST_F(MacroTest, ExpandDecl_WithFailedMacroCall_HandlesError01)
{
    BuildDefinePlugin();

    auto src = srcPath + "test_failExpand.cj";
    invocation.globalOptions.executablePath = projectPath + "/output/bin/";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::PARSE);
    instance->PerformImportPackage();

    auto file = instance->GetSourcePackages()[0]->files[0].get();
    ASSERT_FALSE(file->decls.empty());

    auto result = instance->ExpandDecl(std::move(file->decls[0]));
    ASSERT_EQ(result.size(), 1);
    auto med = AST::As<ASTKind::MACRO_EXPAND_DECL>(result[0].get());
    EXPECT_EQ(med->identifier, "M");

    EXPECT_EQ(diag.GetErrorCount(), 1);
}

// One child macro expand failed, another child macro expand success
TEST_F(MacroTest, ExpandDecl_WithFailedMacroCall_HandlesError02)
{
    BuildDefinePlugin();

    auto src = srcPath + "test_failExpand.cj";
    invocation.globalOptions.executablePath = projectPath + "/output/bin/";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::PARSE);
    instance->PerformImportPackage();

    auto file = instance->GetSourcePackages()[0]->files[0].get();
    ASSERT_FALSE(file->decls.empty());

    auto result = instance->ExpandDecl(std::move(file->decls[1]));
    ASSERT_EQ(result.size(), 1);
    auto expandedDecl = AST::As<ASTKind::CLASS_DECL>(result[0].get());
    ASSERT_TRUE(expandedDecl != nullptr);
    EXPECT_EQ(expandedDecl->identifier, "FailExpand");
    auto expectBodySize = 2;
    EXPECT_EQ(expandedDecl->body->decls.size(), expectBodySize);
    EXPECT_EQ(expandedDecl->body->decls[0]->identifier, "a_gen_var");

    EXPECT_EQ(diag.GetErrorCount(), 1);
}

// One child MacroExpandExpr expand failed, another child macro expand success
TEST_F(MacroTest, ExpandDecl_WithFailedMacroCall_HandlesError03)
{
    BuildDefinePlugin();

    auto src = srcPath + "test_failExpand.cj";
    invocation.globalOptions.executablePath = projectPath + "/output/bin/";
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->compileOnePackageFromSrcFiles = true;
    instance->srcFilePaths = {src};
    instance->Compile(CompileStage::PARSE);
    instance->PerformImportPackage();

    auto file = instance->GetSourcePackages()[0]->files[0].get();
    ASSERT_FALSE(file->decls.empty());

    auto result = instance->ExpandDecl(std::move(file->decls[2]));
    ASSERT_EQ(result.size(), 1);
    auto expandedDecl = AST::As<ASTKind::FUNC_DECL>(result[0].get());
    ASSERT_TRUE(expandedDecl != nullptr);
    EXPECT_EQ(expandedDecl->identifier, "ExpandExpr");
    auto expectBodySize = 3;
    EXPECT_EQ(expandedDecl->funcBody->body->body.size(), expectBodySize);
    auto varDecl = AST::As<ASTKind::VAR_DECL>(expandedDecl->funcBody->body->body[0]);
    EXPECT_EQ(varDecl->identifier, "a_gen_var");

    EXPECT_EQ(diag.GetErrorCount(), 1);
}

// ---------------------------------------------------------------------------
// Macro import semantic tests (MacroImportDT UT-M-01 ~ UT-M-10).
//
// Strategy: macro packages and their cjos are built in-process (no external cjc
// run, no plugin dlopen), then a downstream package is compiled through the
// production import path (Compile(IMPORT_PACKAGE), which injects the conditional
// compilation evaluator) and the injection effects are asserted via the
// ImportManager observation getters. MakeInstance lives in the anonymous
// namespace above the fixture; BuildCjo is a fixture method.

// Compile `code` as one package in-process and export its cjo into `outDir`.
// TestCompilerInstance::ParseCode bypasses the production parse assembler
// (CompileStrategy::CheckPackageConsistency), so replicate its macro-import
// aggregation (and macro-package marking, see MacroImportCjoRoundTrip) before
// exporting. Returns the written cjo path.
// (Implemented as a MacroTest fixture method above.)

// UT-M-01: rule 1 — a macro package's own macro imports are never fed back into
// itself. Replicates the production sequence (inject evaluator, then BuildIndex)
// and asserts no warning is emitted and nothing is recorded for .macrocall.
TEST_F(MacroTest, MacroImport_NotForSelf)
{
    instance = MakeInstance(invocation, diag, R"(macro package mself
import std.ast.*
macro import std.collection.ArrayList
)");
    ASSERT_TRUE(instance->Compile(CompileStage::CONDITION_COMPILE));
    auto* srcPkg = &*instance->GetSourcePackages()[0];
    srcPkg->isMacroPackage = true;
    for (auto& f : srcPkg->files) {
        for (auto& stmt : f->package->macroImports) {
            srcPkg->macroImports.push_back(ASTCloner::CloneMacroImportList(*stmt));
        }
    }
    ASSERT_EQ(srcPkg->macroImports.size(), 1U);
    diag.Reset();
    AST::ConditionalCompilation cc(instance.get());
    instance->importManager->SetConditionalCompilation(&cc);
    std::vector<Ptr<AST::Package>> pkgs;
    for (auto& it : instance->GetSourcePackages()) {
        pkgs.push_back(it);
    }
    auto modulesDir = FileUtil::JoinPath(FileUtil::JoinPath(instance->cangjieHome, "modules"),
        invocation.globalOptions.GetCangjieLibTargetPathName());
    EXPECT_TRUE(instance->importManager->BuildIndex(modulesDir, invocation.globalOptions, pkgs));
    // Self-skip is rule-1 semantics (visited pre-seeding), not the defensive path.
    EXPECT_EQ(diag.GetWarningCount(), 0);
    EXPECT_EQ(diag.GetErrorCount(), 0);
    EXPECT_TRUE(MacroImportStrings(*instance->importManager, pkgs[0]->fullPackageName).empty());
}

// UT-M-02: rule 2 — a downstream package importing a macro package obtains the
// symbols named by the macro package's macro import declaration.
TEST_F(MacroTest, MacroImport_InjectToDownstream)
{
    auto dir = FileUtil::JoinPath(projectPath, "unittests/Macro/testTempFiles");
    FileUtil::CreateDirs(dir);
    BuildCjo(R"(macro package msrc
import std.ast.*
macro import std.collection.ArrayList
)", dir);
    invocation.globalOptions.importPaths.push_back(dir);
    auto inst = MakeInstance(invocation, diag, "package app\nimport msrc\nmain(): Int64 { 0 }\n");
    ASSERT_TRUE(inst->Compile(CompileStage::IMPORT_PACKAGE));
    EXPECT_EQ(diag.GetErrorCount(), 0);
    const auto& strs = MacroImportStrings(*inst->importManager, "app");
    ASSERT_EQ(strs.size(), 1U);
    EXPECT_EQ(strs[0], "std.collection.ArrayList");
    // The injected symbol is reachable from the consumer file's import map.
    EXPECT_FALSE(
        inst->importManager->GetImportedDeclsByName(*inst->GetSourcePackages()[0]->files[0], "ArrayList").empty());
}

// UT-M-03: rule 2 file-level visibility — only files that actually import the
// macro package receive the injected symbols.
TEST_F(MacroTest, MacroImport_FileScope)
{
    auto dir = FileUtil::JoinPath(projectPath, "unittests/Macro/testTempFiles");
    FileUtil::CreateDirs(dir);
    BuildCjo(R"(macro package msrc
import std.ast.*
macro import std.collection.ArrayList
)", dir);
    invocation.globalOptions.importPaths.push_back(dir);
    auto consumerPath = FileUtil::JoinPath(dir, "m03_consumer.cj");
    auto plainPath = FileUtil::JoinPath(dir, "m03_plain.cj");
    {
        std::ofstream out(consumerPath);
        out << "package app\nimport msrc\nmain(): Int64 { 0 }\n";
    }
    {
        std::ofstream out(plainPath);
        out << "package app\nfunc Dummy(): Unit {}\n";
    }
    auto inst = std::make_unique<TestCompilerInstance>(invocation, diag);
    inst->invocation.globalOptions.implicitPrelude = true;
    inst->compileOnePackageFromSrcFiles = true;
    inst->srcFilePaths = {consumerPath, plainPath};
    ASSERT_TRUE(inst->Compile(CompileStage::IMPORT_PACKAGE));
    ASSERT_EQ(diag.GetErrorCount(), 0);
    auto& files = inst->GetSourcePackages()[0]->files;
    ASSERT_EQ(files.size(), 2U);
    // Identify the files by their source paths: parse order is not guaranteed and
    // implicit-prelude imports make imports.empty() unreliable as a discriminator.
    auto isFile = [&files](const char* tag) -> AST::File* {
        for (auto& f : files) {
            if (f->filePath.find(tag) != std::string::npos) {
                return f.get();
            }
        }
        return nullptr;
    };
    auto* consumer = isFile("m03_consumer");
    auto* plain = isFile("m03_plain");
    ASSERT_NE(consumer, nullptr);
    ASSERT_NE(plain, nullptr);
    auto hasArrayList = [&inst](const AST::File& f) {
        return !inst->importManager->GetImportedDeclsByName(f, "ArrayList").empty();
    };
    EXPECT_TRUE(hasArrayList(*consumer));
    EXPECT_FALSE(hasArrayList(*plain));
}

// UT-M-04: rule 3.3 lifter chain — downstream imports pkgB only; pkgB declares
// `macro import pkgA` and pkgA declares `macro import std.collection.ArrayList`;
// both targets must be lifted onto the downstream package.
TEST_F(MacroTest, MacroImport_LiftChain)
{
    auto dir = FileUtil::JoinPath(projectPath, "unittests/Macro/testTempFiles");
    FileUtil::CreateDirs(dir);
    BuildCjo(R"(macro package pkgA
import std.ast.*
macro import std.collection.ArrayList
)", dir);
    BuildCjo(R"(macro package pkgB
import std.ast.*
macro import pkgA
)", dir);
    invocation.globalOptions.importPaths.push_back(dir);
    auto inst = MakeInstance(invocation, diag, "package app\nimport pkgB\nmain(): Int64 { 0 }\n");
    ASSERT_TRUE(inst->Compile(CompileStage::IMPORT_PACKAGE));
    EXPECT_EQ(diag.GetErrorCount(), 0);
    const auto& pkgs = inst->importManager->GetMacroImportPkgsFromMacroDeps("app");
    EXPECT_NE(pkgs.find("pkgA"), pkgs.end());
    EXPECT_NE(pkgs.find("std.collection"), pkgs.end());
    const auto& strs = MacroImportStrings(*inst->importManager, "app");
    ASSERT_EQ(strs.size(), 2U);
    EXPECT_EQ(strs[0], "pkgA");
    EXPECT_EQ(strs[1], "std.collection.ArrayList");
}

// UT-M-05: declaration-chain cycle — cycA `macro import cycB`, cycB `macro import
// cycA`. Chain walking must terminate and neither side may be re-injected into
// itself (rule 1); downstream obtains each side's declared import exactly once.
TEST_F(MacroTest, MacroImport_Cycle)
{
    auto dir = FileUtil::JoinPath(projectPath, "unittests/Macro/testTempFiles");
    FileUtil::CreateDirs(dir);
    BuildCjo(R"(macro package cycA
import std.ast.*
macro import cycB
)", dir);
    BuildCjo(R"(macro package cycB
import std.ast.*
macro import cycA
)", dir);
    invocation.globalOptions.importPaths.push_back(dir);
    auto inst = MakeInstance(invocation, diag, "package app\nimport cycA\nmain(): Int64 { 0 }\n");
    ASSERT_TRUE(inst->Compile(CompileStage::IMPORT_PACKAGE));
    EXPECT_EQ(diag.GetErrorCount(), 0);
    const auto& strs = MacroImportStrings(*inst->importManager, "app");
    ASSERT_EQ(strs.size(), 2U);
    EXPECT_EQ(strs[0], "cycB");
    EXPECT_EQ(strs[1], "cycA");
}

// UT-M-06: a block whose every entry is @When-false in the downstream target
// injects nothing and loads nothing.
TEST_F(MacroTest, MacroImport_AllConditionsFalse)
{
    auto dir = FileUtil::JoinPath(projectPath, "unittests/Macro/testTempFiles");
    FileUtil::CreateDirs(dir);
    BuildCjo(R"(macro package mfalse
import std.ast.*
macro {
    @When[os == "Windows"] import std.reflect
    @When[os == "Windows"] import std.unittest
}
)", dir);
    invocation.globalOptions.importPaths.push_back(dir);
    auto inst = MakeInstance(invocation, diag, "package app\nimport mfalse\nmain(): Int64 { 0 }\n");
    ASSERT_TRUE(inst->Compile(CompileStage::IMPORT_PACKAGE));
    EXPECT_EQ(diag.GetErrorCount(), 0);
    EXPECT_TRUE(MacroImportStrings(*inst->importManager, "app").empty());
    EXPECT_TRUE(inst->importManager->GetMacroImportPkgsFromMacroDeps("app").empty());
}

// UT-M-07: the same macro-package cjo evaluates differently per downstream
// target — only the entry whose @When holds in the current target survives.
TEST_F(MacroTest, MacroImport_PerTargetEval)
{
    auto dir = FileUtil::JoinPath(projectPath, "unittests/Macro/testTempFiles");
    FileUtil::CreateDirs(dir);
    BuildCjo(R"(macro package mtgt
import std.ast.*
macro {
    @When[os == "Linux"] import std.math
    @When[os == "Windows"] import std.reflect
}
)", dir);
    invocation.globalOptions.importPaths.push_back(dir);
    {
        // Fixture default target (Linux on unix hosts).
        auto inst = MakeInstance(invocation, diag, "package app\nimport mtgt\nmain(): Int64 { 0 }\n");
        ASSERT_TRUE(inst->Compile(CompileStage::IMPORT_PACKAGE));
        EXPECT_EQ(diag.GetErrorCount(), 0);
        const auto& strs = MacroImportStrings(*inst->importManager, "app");
        ASSERT_EQ(strs.size(), 1U);
        EXPECT_EQ(strs[0], "std.math");
    }
    diag.Reset();
    {
        invocation.globalOptions.target.os = Triple::OSType::WINDOWS;
        auto inst = MakeInstance(invocation, diag, "package app2\nimport mtgt\nmain(): Int64 { 0 }\n");
        ASSERT_TRUE(inst->Compile(CompileStage::IMPORT_PACKAGE));
        const auto& strs = MacroImportStrings(*inst->importManager, "app2");
        ASSERT_EQ(strs.size(), 1U);
        EXPECT_EQ(strs[0], "std.reflect");
    }
}

// UT-M-08: no dedicated macro-import conflict check — two macro packages whose
// macro imports carry the same symbol name from different source packages
// compile cleanly while unused; the clash surfaces at the use site as an
// ordinary ambiguous-use diagnostic.
TEST_F(MacroTest, MacroImport_UseTimeConflict)
{
    auto dir = FileUtil::JoinPath(projectPath, "unittests/Macro/testTempFiles");
    FileUtil::CreateDirs(dir);
    BuildCjo("package plain1\npublic func f(): Int64 { 1 }\n", dir, false);
    BuildCjo("package plain2\npublic func f(): Int64 { 2 }\n", dir, false);
    BuildCjo(R"(macro package mone
import std.ast.*
macro import plain1.f
)", dir);
    BuildCjo(R"(macro package mtwo
import std.ast.*
macro import plain2.f
)", dir);
    invocation.globalOptions.importPaths.push_back(dir);
    {
        // Declarations only, no use: no conflict is reported at import time.
        auto inst = MakeInstance(invocation, diag, "package app\nimport mone\nimport mtwo\nmain(): Int64 { 0 }\n");
        ASSERT_TRUE(inst->Compile(CompileStage::IMPORT_PACKAGE));
        EXPECT_EQ(diag.GetErrorCount(), 0);
    }
    diag.Reset();
    {
        // Use site: two same-name decls injected into the same file map produce
        // the ordinary ambiguity diagnostic in Sema.
        auto inst = MakeInstance(invocation, diag,
            "package app\nimport mone\nimport mtwo\nmain(): Int64 { let _ = f()\n 0 }\n");
        (void)inst->Compile(CompileStage::SEMA);
        EXPECT_GT(diag.GetErrorCount(), 0);
    }
}

// UT-M-09: an empty macro-import block parses to one list with zero items and
// injects nothing downstream.
TEST_F(MacroTest, MacroImport_EmptyBlock)
{
    auto dir = FileUtil::JoinPath(projectPath, "unittests/Macro/testTempFiles");
    FileUtil::CreateDirs(dir);
    auto cjoPath = BuildCjo(R"(macro package mempty
import std.ast.*
macro {
}
)", dir);
    // LoadPackageFromCjo resolves the cjo through the import search path, so the
    // temp dir must be registered before loading.
    invocation.globalOptions.importPaths.push_back(dir);
    {
        // LoadPackageFromCjo resolves the cjo through the import search path, which
        // a fresh ImportManager only builds inside BuildIndex — seed it manually.
        auto inst = std::make_unique<TestCompilerInstance>(invocation, diag);
        auto modulesDir = FileUtil::JoinPath(FileUtil::JoinPath(inst->cangjieHome, "modules"),
            invocation.globalOptions.GetCangjieLibTargetPathName());
        inst->importManager->UpdateSearchPath(modulesDir);
        auto loaded = inst->importManager->LoadPackageFromCjo("mempty", cjoPath);
        ASSERT_NE(loaded, nullptr);
        ASSERT_EQ(loaded->macroImports.size(), 1U);
        EXPECT_TRUE(loaded->macroImports[0]->items.empty());
    }
    diag.Reset();
    auto inst = MakeInstance(invocation, diag, "package app\nimport mempty\nmain(): Int64 { 0 }\n");
    ASSERT_TRUE(inst->Compile(CompileStage::IMPORT_PACKAGE));
    EXPECT_EQ(diag.GetErrorCount(), 0);
    EXPECT_TRUE(MacroImportStrings(*inst->importManager, "app").empty());
}

// UT-M-10: rule 2 does not exclude downstream macro packages — a downstream
// `macro package` importing a macro package still receives its macro imports.
TEST_F(MacroTest, MacroImport_DownstreamMacroPkg)
{
    auto dir = FileUtil::JoinPath(projectPath, "unittests/Macro/testTempFiles");
    FileUtil::CreateDirs(dir);
    BuildCjo(R"(macro package msrc
import std.ast.*
macro import std.collection.ArrayList
)", dir);
    invocation.globalOptions.importPaths.push_back(dir);
    auto inst = MakeInstance(invocation, diag,
        "macro package dmacro\nimport msrc\n\n");
    ASSERT_TRUE(inst->Compile(CompileStage::IMPORT_PACKAGE));
    EXPECT_EQ(diag.GetErrorCount(), 0);
    const auto& strs = MacroImportStrings(*inst->importManager, "dmacro");
    ASSERT_EQ(strs.size(), 1U);
    EXPECT_EQ(strs[0], "std.collection.ArrayList");
}

// P1-7: an unresolvable macro-import target (no such package anywhere on the
// search path) is diagnosed instead of silently dropped, and nothing is injected
// or recorded for .macrocall generation. The compile itself still succeeds.
TEST_F(MacroTest, MacroImport_UnresolvedTarget)
{
    auto dir = FileUtil::JoinPath(projectPath, "unittests/Macro/testTempFiles");
    FileUtil::CreateDirs(dir);
    BuildCjo(R"(macro package mbad
import std.ast.*
macro import nosuchpkg.*
)", dir);
    invocation.globalOptions.importPaths.push_back(dir);
    auto inst = MakeInstance(invocation, diag, "package app\nimport mbad\nmain(): Int64 { 0 }\n");
    ASSERT_TRUE(inst->Compile(CompileStage::IMPORT_PACKAGE));
    EXPECT_GT(diag.GetErrorCount(), 0);
    EXPECT_TRUE(MacroImportStrings(*inst->importManager, "app").empty());
    EXPECT_TRUE(inst->importManager->GetMacroImportPkgsFromMacroDeps("app").empty());
}

// P1-1: for `macro import pkg.sym as alias`, the alias rebinding is preserved
// (aligned with ordinary imports): the injected symbols are keyed by the alias,
// so the consumer reaches `alias` but never the original `sym`.
TEST_F(MacroTest, MacroImport_AliasSymbolVisibility)
{
    auto dir = FileUtil::JoinPath(projectPath, "unittests/Macro/testTempFiles");
    FileUtil::CreateDirs(dir);
    BuildCjo("package vsrc\npublic var v: Int32 = 1\n", dir, /*markMacro=*/false);
    BuildCjo(R"(macro package masrc
import std.ast.*
macro import vsrc.v as q
)", dir);
    invocation.globalOptions.importPaths.push_back(dir);
    auto inst = MakeInstance(invocation, diag, "package app\nimport masrc\nmain(): Int64 { 0 }\n");
    ASSERT_TRUE(inst->Compile(CompileStage::IMPORT_PACKAGE));
    EXPECT_EQ(diag.GetErrorCount(), 0);
    auto& file = *inst->GetSourcePackages()[0]->files[0];
    EXPECT_FALSE(inst->importManager->GetImportedDeclsByName(file, "q").empty());
    EXPECT_TRUE(inst->importManager->GetImportedDeclsByName(file, "v").empty());
}

// P1-3: macro-import state is keyed per package — a second package indexed by
// the same ImportManager (the LSP rebuild shape) must get its own survivors and
// must not clobber the first package's recorded entries.
TEST_F(MacroTest, MacroImport_MultiPackageIsolation)
{
    auto dir = FileUtil::JoinPath(projectPath, "unittests/Macro/testTempFiles");
    FileUtil::CreateDirs(dir);
    BuildCjo(R"(macro package msrc
import std.ast.*
macro import std.collection.ArrayList
)", dir);
    invocation.globalOptions.importPaths.push_back(dir);
    auto inst = MakeInstance(invocation, diag, "package isoa\nimport msrc\nmain(): Int64 { 0 }\n");
    ASSERT_TRUE(inst->Compile(CompileStage::IMPORT_PACKAGE));
    ASSERT_EQ(diag.GetErrorCount(), 0);
    const auto& strsA = MacroImportStrings(*inst->importManager, "isoa");
    ASSERT_EQ(strsA.size(), 1U);
    EXPECT_EQ(strsA[0], "std.collection.ArrayList");

    // Re-index a second package through the same ImportManager.
    auto inst2 = MakeInstance(invocation, diag, "package isob\nimport msrc\nmain(): Int64 { 0 }\n");
    ASSERT_TRUE(inst2->Compile(CompileStage::CONDITION_COMPILE));
    std::vector<Ptr<AST::Package>> pkgs;
    for (auto& it : inst2->GetSourcePackages()) {
        pkgs.push_back(it);
    }
    ASSERT_FALSE(pkgs.empty());
    auto modulesDir = FileUtil::JoinPath(FileUtil::JoinPath(inst->cangjieHome, "modules"),
        invocation.globalOptions.GetCangjieLibTargetPathName());
    EXPECT_TRUE(inst->importManager->BuildIndex(modulesDir, invocation.globalOptions, pkgs));
    // isob recorded its own survivor; isoa's entry was left untouched.
    const auto& strsB = MacroImportStrings(*inst->importManager, "isob");
    ASSERT_EQ(strsB.size(), 1U);
    EXPECT_EQ(strsB[0], "std.collection.ArrayList");
    const auto& strsA2 = MacroImportStrings(*inst->importManager, "isoa");
    ASSERT_EQ(strsA2.size(), 1U);
    EXPECT_EQ(strsA2[0], "std.collection.ArrayList");
}

// ---------------------------------------------------------------------------
// Macro import .macrocall injection tests (MacroImportDT UT-D-01 ~ UT-D-03).
// These exercise the --debug-macro path: a downstream package with a real macro
// call (define.M identity plugin, built exactly like ForeignBlockMacroExpansion)
// is compiled through macro expansion with enableMacroDebug, and the generated
// .macrocall file (file path + ".macrocall") is read back and inspected.
// ---------------------------------------------------------------------------

namespace {
std::string ReadFileOrEmpty(const std::string& path)
{
    std::ifstream in(path);
    std::string content{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>{}};
    return content;
}
}

// UT-D-01: the generated .macrocall carries one bare `import <entry>` line per
// surviving macro import — placed after the package statement and before the
// file's own imports and first macro call, with no @When, no modifier and no
// 'macro import' wording left.
TEST_F(MacroTest, MacroCall_InjectMacroImports)
{
    BuildDefinePlugin();
    auto dir = FileUtil::JoinPath(projectPath, "unittests/Macro/testTempFiles");
    FileUtil::CreateDirs(dir);
    BuildCjo(R"(macro package d01src
import std.ast.*
macro import std.collection.ArrayList
)", dir);
    invocation.globalOptions.importPaths.push_back(dir);
    invocation.globalOptions.enableMacroDebug = true;
    auto srcFile = FileUtil::JoinPath(dir, "d01_downstream.cj");
    {
        std::ofstream out(srcFile);
        out << "package d\nimport d01src\nimport define.*\n\n@M func dummy(): Unit {}\n";
    }
    auto inst = std::make_unique<TestCompilerInstance>(invocation, diag);
    inst->invocation.globalOptions.implicitPrelude = true;
    inst->compileOnePackageFromSrcFiles = true;
    inst->srcFilePaths = {srcFile};
    ASSERT_TRUE(inst->Compile(CompileStage::PARSE));
    ASSERT_TRUE(inst->PerformImportPackage());
    ASSERT_TRUE(inst->PerformMacroExpand());
    EXPECT_EQ(diag.GetErrorCount(), 0);
    // In --debug-macro mode the file node is replaced by one re-parsed from the
    // .macrocall file (ReplaceEachFileNode), so read the .macrocall via the
    // source file path instead of the (stale) File::macroCallFilePath.
    auto content = ReadFileOrEmpty(srcFile + ".macrocall");
    EXPECT_FALSE(content.empty());
    auto pInj = content.find("import std.collection.ArrayList");
    ASSERT_NE(pInj, std::string::npos);
    EXPECT_EQ(content.find("@When"), std::string::npos);
    EXPECT_EQ(content.find("macro import"), std::string::npos);
    EXPECT_EQ(content.find("public import"), std::string::npos);
    // After the package statement, before the file's own imports and the first
    // (expanded) macro call.
    EXPECT_GT(pInj, content.find("package d"));
    EXPECT_LT(pInj, content.find("import d01src"));
    EXPECT_LT(pInj, content.find("func dummy"));
}

// UT-D-02: duplicate macro import declarations (same import string twice) are
// injected only once; multiple entries keep their evaluation survival order.
TEST_F(MacroTest, MacroCall_InjectDedup)
{
    BuildDefinePlugin();
    auto dir = FileUtil::JoinPath(projectPath, "unittests/Macro/testTempFiles");
    FileUtil::CreateDirs(dir);
    BuildCjo(R"(macro package d02src
import std.ast.*
macro import std.math
macro import std.collection.ArrayList
macro import std.math
)", dir);
    invocation.globalOptions.importPaths.push_back(dir);
    invocation.globalOptions.enableMacroDebug = true;
    auto srcFile = FileUtil::JoinPath(dir, "d02_downstream.cj");
    {
        std::ofstream out(srcFile);
        out << "package d\nimport d02src\nimport define.*\n\n@M func dummy(): Unit {}\n";
    }
    auto inst = std::make_unique<TestCompilerInstance>(invocation, diag);
    inst->invocation.globalOptions.implicitPrelude = true;
    inst->compileOnePackageFromSrcFiles = true;
    inst->srcFilePaths = {srcFile};
    ASSERT_TRUE(inst->Compile(CompileStage::PARSE));
    ASSERT_TRUE(inst->PerformImportPackage());
    ASSERT_TRUE(inst->PerformMacroExpand());
    EXPECT_EQ(diag.GetErrorCount(), 0);
    auto content = ReadFileOrEmpty(srcFile + ".macrocall");
    auto pMath = content.find("import std.math");
    auto pArray = content.find("import std.collection.ArrayList");
    ASSERT_NE(pMath, std::string::npos);
    ASSERT_NE(pArray, std::string::npos);
    // "std.math" declared twice, injected exactly once.
    EXPECT_EQ(content.find("import std.math", pMath + 1), std::string::npos);
    // Survival order follows the declaration order.
    EXPECT_LT(pMath, pArray);
}

// UT-D-03: a macro import whose @When condition is false in the downstream
// target never reaches GetMacroImportStrings, so the .macrocall has no line
// for it (and never any @When wording).
TEST_F(MacroTest, MacroCall_FilterFalseWhen)
{
    BuildDefinePlugin();
    auto dir = FileUtil::JoinPath(projectPath, "unittests/Macro/testTempFiles");
    FileUtil::CreateDirs(dir);
    BuildCjo(R"(macro package d03src
import std.ast.*
macro {
    @When[os == "Windows"] import std.reflect
    @When[os == "Linux"] import std.math
}
)", dir);
    invocation.globalOptions.importPaths.push_back(dir);
    invocation.globalOptions.enableMacroDebug = true;
    auto srcFile = FileUtil::JoinPath(dir, "d03_downstream.cj");
    {
        std::ofstream out(srcFile);
        out << "package d\nimport d03src\nimport define.*\n\n@M func dummy(): Unit {}\n";
    }
    auto inst = std::make_unique<TestCompilerInstance>(invocation, diag);
    inst->invocation.globalOptions.implicitPrelude = true;
    inst->compileOnePackageFromSrcFiles = true;
    inst->srcFilePaths = {srcFile};
    ASSERT_TRUE(inst->Compile(CompileStage::PARSE));
    ASSERT_TRUE(inst->PerformImportPackage());
    ASSERT_TRUE(inst->PerformMacroExpand());
    EXPECT_EQ(diag.GetErrorCount(), 0);
    auto content = ReadFileOrEmpty(srcFile + ".macrocall");
    EXPECT_NE(content.find("import std.math"), std::string::npos);
    EXPECT_EQ(content.find("std.reflect"), std::string::npos);
    EXPECT_EQ(content.find("@When"), std::string::npos);
}

// UT-D-04: an alias on a macro import rebinding downstream (aligned with ordinary
// imports). The alias survives both the injection keying and the `import xxx as q`
// emitted to .macrocall, so downstream resolves the alias exactly as an ordinary
// `import std.math as q` would — upholding the ordinary-private-import equivalence
// of rule 2.
TEST_F(MacroTest, MacroCall_InjectKeepsAlias)
{
    BuildDefinePlugin();
    auto dir = FileUtil::JoinPath(projectPath, "unittests/Macro/testTempFiles");
    FileUtil::CreateDirs(dir);
    BuildCjo(R"(macro package d04src
import std.ast.*
macro import std.math as q
)", dir);
    invocation.globalOptions.importPaths.push_back(dir);
    invocation.globalOptions.enableMacroDebug = true;
    auto srcFile = FileUtil::JoinPath(dir, "d04_downstream.cj");
    {
        std::ofstream out(srcFile);
        out << "package d\nimport d04src\nimport define.*\n\n@M func dummy(): Unit {}\n";
    }
    auto inst = std::make_unique<TestCompilerInstance>(invocation, diag);
    inst->invocation.globalOptions.implicitPrelude = true;
    inst->compileOnePackageFromSrcFiles = true;
    inst->srcFilePaths = {srcFile};
    ASSERT_TRUE(inst->Compile(CompileStage::PARSE));
    ASSERT_TRUE(inst->PerformImportPackage());
    ASSERT_TRUE(inst->PerformMacroExpand());
    EXPECT_EQ(diag.GetErrorCount(), 0);
    auto content = ReadFileOrEmpty(srcFile + ".macrocall");
    EXPECT_FALSE(content.empty());
    // The alias is preserved: the full rebinding form `import std.math as q` is
    // injected, matching ordinary import semantics.
    EXPECT_NE(content.find("import std.math as q"), std::string::npos);
}

// P1-6: per-file consumer filtering reaches the .macrocall path too — only files
// that import the declaring macro package get injected lines; a sibling file of
// the same package with macro calls but no such import gets none.
TEST_F(MacroTest, MacroCall_NonConsumerFileNoInject)
{
    BuildDefinePlugin();
    auto dir = FileUtil::JoinPath(projectPath, "unittests/Macro/testTempFiles");
    FileUtil::CreateDirs(dir);
    BuildCjo(R"(macro package d05src
import std.ast.*
macro import std.collection.ArrayList
)", dir);
    invocation.globalOptions.importPaths.push_back(dir);
    invocation.globalOptions.enableMacroDebug = true;
    auto consumerFile = FileUtil::JoinPath(dir, "d05_consumer.cj");
    auto plainFile = FileUtil::JoinPath(dir, "d05_plain.cj");
    {
        std::ofstream out(consumerFile);
        out << "package d\nimport d05src\nimport define.*\n\n@M func dummy(): Unit {}\n";
    }
    {
        std::ofstream out(plainFile);
        out << "package d\nimport define.*\n\n@M func dummy2(): Unit {}\n";
    }
    auto inst = std::make_unique<TestCompilerInstance>(invocation, diag);
    inst->invocation.globalOptions.implicitPrelude = true;
    inst->compileOnePackageFromSrcFiles = true;
    inst->srcFilePaths = {consumerFile, plainFile};
    ASSERT_TRUE(inst->Compile(CompileStage::PARSE));
    ASSERT_TRUE(inst->PerformImportPackage());
    ASSERT_TRUE(inst->PerformMacroExpand());
    EXPECT_EQ(diag.GetErrorCount(), 0);
    auto consumer = ReadFileOrEmpty(consumerFile + ".macrocall");
    auto plain = ReadFileOrEmpty(plainFile + ".macrocall");
    ASSERT_FALSE(consumer.empty());
    ASSERT_FALSE(plain.empty());
    EXPECT_NE(consumer.find("import std.collection.ArrayList"), std::string::npos);
    EXPECT_EQ(plain.find("import std.collection.ArrayList"), std::string::npos);
}
