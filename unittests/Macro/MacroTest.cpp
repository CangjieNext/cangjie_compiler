// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <cstdlib>
#include <string>
#include "gtest/gtest.h"

#include "TestCompilerInstance.h"
#include "cangjie/AST/Match.h"
#include "cangjie/AST/Walker.h"

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
    char **env = environ;
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
        invocation.globalOptions.executablePath = projectPath + "\\output\\bin\\cjc";
#elif defined(__unix__)
        invocation.globalOptions.target.os = Cangjie::Triple::OSType::LINUX;
        invocation.globalOptions.executablePath = projectPath + "/output/bin/cjc";
#endif
        std::string cangjieHome = projectPath + "/output";
#ifdef __x86_64__
        std::string cangjiePath = cangjieHome + "/modules/linux_x86_64_cjnative";
#else
        std::string cangjiePath = cangjieHome + "/modules/linux_aarch64_cjnative";
#endif
        setenv("CANGJIE_HOME", cangjieHome.c_str(), 1);
        setenv("CANGJIE_PATH", cangjiePath.c_str(), 1);
        invocation.globalOptions.ReadPathsFromEnvironmentVars(GetEnvironmentVars());
        invocation.globalOptions.importPaths = {definePath};
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
    DiagnosticEngine diag;
    CompilerInvocation invocation;
    std::unique_ptr<TestCompilerInstance> instance;
};

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
    std::string command = "cd " + definePath + " && cjc define.cj --compile-macro";
    int err = system(command.c_str());
    ASSERT_EQ(0, err);
 
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
#endif
