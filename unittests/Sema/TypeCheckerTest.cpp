// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <string>
#include <unordered_map>
#include <vector>
#include "gtest/gtest.h"
#include "SemaTestBase.h"
#include "cangjie/AST/Match.h"
#include "cangjie/AST/PrintNode.h"
#include "cangjie/AST/Walker.h"
#include "cangjie/Basic/Match.h"

using namespace Cangjie;
using namespace AST;

class TypeCheckerTest : public SemaTestBase {
};

TEST_F(TypeCheckerTest, TypecheckTest)
{
    std::string code = R"(
main() {
    var t: Int32
    t = 1
    return 0
}
    )";

    instance->code = code;
    instance->Compile(CompileStage::SEMA);

    Walker walker(instance->GetSourcePackages()[0]->files[0].get(), [](Ptr<Node> node) -> VisitAction {
        Meta::match (*node)(
            [](const FuncDecl& decl) {
                auto ty = dynamic_cast<FuncTy*>(decl.GetTy().get());
                EXPECT_EQ(ty->retTy->kind, TypeKind::TYPE_INT64);
            },
            [](const VarDecl& decl) { EXPECT_EQ(decl.TyKind(), TypeKind::TYPE_INT32); });
        return VisitAction::WALK_CHILDREN;
    });
    walker.Walk();
    EXPECT_EQ(diag.GetErrorCount(), 0);
}

TEST_F(TypeCheckerTest, MacroDeclTest)
{
    std::string code = R"(
    public macro
    )";

    instance->code = code;
    instance->PerformParse();
    instance->PerformImportPackage();
    instance->PerformSema();
    std::vector<uint8_t> astData;
    instance->importManager->ExportAST(false, astData, *instance->GetSourcePackages()[0]);

    EXPECT_EQ(diag.GetErrorCount(), 3);
}

TEST_F(TypeCheckerTest, MacroCallInLSPTest)
{
    std::string code = R"(
    @M1
    func test() {
        var a = 1
        a = 2
    }
    @M2
    class A {
        var a = 1
        func test() {
            this.a = 2
        }
        func test1() {
            var b = 1
            b = 2
        }
    }
    @M3
    enum E {
        EE
        func test() {
            let a = EE
        }
    }
    main() {0}
    )";

    instance->code = code;
    instance->invocation.globalOptions.enableMacroInLSP = true;
    instance->Compile(CompileStage::SEMA);
    // Test MemberAccess has target or not in macrocall.
    auto visitPre = [&](Ptr<Node> curNode) -> VisitAction {
        if (curNode->astKind == ASTKind::MEMBER_ACCESS) {
            auto ma = StaticAs<ASTKind::MEMBER_ACCESS>(curNode);
            EXPECT_TRUE(ma->target);
            if (ma->target) {
                std::cout << "MemberAccess field " << ma->field.Val() << " has target." << std::endl;
            } else {
                std::cout << "MemberAccess field " << ma->field.Val() << " has no target." << std::endl;
            }
        }
        if (curNode->astKind == ASTKind::REF_EXPR) {
            auto re = StaticAs<ASTKind::REF_EXPR>(curNode);
            if (re->ref.identifier == "M1" || re->ref.identifier == "M2" || re->ref.identifier == "M3") {
                return VisitAction::SKIP_CHILDREN;
            }
            EXPECT_TRUE(re->ref.target);
            if (re->ref.target) {
                std::cout << re->ref.identifier.Val() << " has target." << std::endl;
            } else {
                std::cout << re->ref.identifier.Val() << " has no target." << std::endl;
            }
            return VisitAction::SKIP_CHILDREN;
        }
        return VisitAction::WALK_CHILDREN;
    };
    Walker macroCallWalker(instance->GetSourcePackages()[0]->files[0].get(), visitPre);
    macroCallWalker.Walk();
    // error: undeclared identifier 'M1'/'M2'/'M3'.
    EXPECT_EQ(diag.GetErrorCount(), 3);
    Cangjie::MacroProcMsger::GetInstance().CloseMacroSrv();
}

TEST_F(TypeCheckerTest, MacroDiagInLSPTest)
{
    instance->invocation.globalOptions.enableMacroInLSP = true;
    srcPath = projectPath + "/unittests/Sema/SemaCangjieFiles/";
    invocation.globalOptions.executablePath = projectPath + "/output/bin/";
    instance->compileOnePackageFromSrcFiles = true;

    instance->srcFilePaths = {srcPath + "UnableToInferGenericArgumentInTest.cj"};
    invocation.globalOptions.outputMode = GlobalOptions::OutputMode::STATIC_LIB;
    invocation.globalOptions.enableCompileTest = true;
    instance->Compile(CompileStage::SEMA);
    auto dv = diag.GetCategoryDiagnostic(DiagCategory::SEMA);
    bool findOptNone = false;
    for (auto& d : dv) {
        for (auto& n : d.subDiags) {
            if (n.mainHint.str.find("Enum-Option<Int64>") != std::string::npos) {
                findOptNone = true;
                EXPECT_EQ(n.mainHint.range.end.line, 14);
                EXPECT_EQ(n.mainHint.range.end.column, 55);
            }
        }
    }
    EXPECT_EQ(findOptNone, true);
    Cangjie::MacroProcMsger::GetInstance().CloseMacroSrv();
}

TEST_F(TypeCheckerTest, DISABLED_NoDiagInLSPMacroCallTest)
{
    srcPath = projectPath + "/unittests/Sema/SemaCangjieFiles/";
    std::string command = "cjc " + srcPath + "AddClassTyInfoMacro.cj --compile-macro -Woff all";
    int err = system(command.c_str());
    ASSERT_EQ(0, err);

    instance->invocation.globalOptions.enableMacroInLSP = false;
    invocation.globalOptions.executablePath = projectPath + "/output/bin/";
    instance->compileOnePackageFromSrcFiles = true;

    instance->srcFilePaths = {srcPath + "NoDiagInLSPMacroCall.cj"};
    invocation.globalOptions.outputMode = GlobalOptions::OutputMode::STATIC_LIB;
    invocation.globalOptions.enableCompileTest = true;
    instance->Compile(CompileStage::SEMA);
    EXPECT_EQ(diag.GetErrorCount(), 0);
    Cangjie::MacroProcMsger::GetInstance().CloseMacroSrv();
}

TEST_F(TypeCheckerTest, NoDiagInLSPMacroCallForTest)
{
    srcPath = projectPath + "/unittests/Sema/SemaCangjieFiles/";
    std::string command = "cjc " + srcPath + "ModifyClassBuildFunc.cj --compile-macro -Woff all";
    int err = system(command.c_str());
    ASSERT_EQ(0, err);

    instance->invocation.globalOptions.enableMacroInLSP = true;
    invocation.globalOptions.executablePath = projectPath + "/output/bin/";
    instance->compileOnePackageFromSrcFiles = true;

    instance->srcFilePaths = {srcPath + "NoDiagInLSPMacroCallNode.cj"};
    invocation.globalOptions.outputMode = GlobalOptions::OutputMode::STATIC_LIB;
    invocation.globalOptions.enableCompileTest = true;
    instance->Compile(CompileStage::SEMA);
    EXPECT_EQ(diag.GetErrorCount(), 0);
    Cangjie::MacroProcMsger::GetInstance().CloseMacroSrv();
}

TEST_F(TypeCheckerTest, MacroCallOfTopLevelInLSPTest)
{
    std::string code = R"(
    @M1
    func test(v: String) {
        var a = 1
        a = 2
    }
    main() {0}
    )";

    instance->code = code;
    instance->invocation.globalOptions.implicitPrelude = true;
    instance->invocation.globalOptions.enableMacroInLSP = true;
    instance->Compile(CompileStage::IMPORT_PACKAGE);
    // Skip macro expand stage and move node in 'originalMacroCallNodes' to simulate original macro code for LSP.
    auto file = instance->GetSourcePackages()[0]->files[0].get();
    for (auto& it : file->decls) {
        if (auto md = DynamicCast<MacroExpandDecl>(it.get())) {
            file->originalMacroCallNodes.emplace_back(std::move(md->invocation.decl));
        }
    }
    instance->PerformSema();
    unsigned checkCount = 0;
    // Test ty is set correctly.
    auto visitPre = [&](Ptr<Node> curNode) -> VisitAction {
        if (auto fp = DynamicCast<FuncParam>(curNode); fp && fp->identifier == "v") {
            checkCount++;
            EXPECT_TRUE(fp->type != nullptr);
            if (fp->type != nullptr) {
                EXPECT_EQ(fp->type->GetTy()->String(), "Struct-String");
            }
        }
        return VisitAction::WALK_CHILDREN;
    };
    for (auto& it : file->originalMacroCallNodes) {
        Walker(it.get(), visitPre).Walk();
    }
    EXPECT_EQ(checkCount, 1);
    Cangjie::MacroProcMsger::GetInstance().CloseMacroSrv();
}

TEST_F(TypeCheckerTest, AssumptionTest)
{
#ifdef _WIN32
    srcPath = projectPath + "\\unittests\\Sema\\SemaCangjieFiles\\AssumptionTest";
#elif defined(__unix__)
    srcPath = projectPath + "/unittests/Sema/SemaCangjieFiles/AssumptionTest";
#endif

    instance->srcDirs = {srcPath};
    instance->invocation.globalOptions.implicitPrelude = true;
    instance->Compile();

    for (auto& decl : instance->GetSourcePackages()[0]->files[0]->decls) {
        if (auto cd = AST::As<ASTKind::CLASS_DECL>(decl.get()); cd) {
            if (cd->identifier == "A") {
                EXPECT_EQ(cd->generic->assumptionCollection.size(), 1);
                auto it = cd->generic->assumptionCollection.begin();
                EXPECT_EQ(it->second.size(), 1);
            } else if (cd->identifier == "B") {
                EXPECT_EQ(cd->generic->assumptionCollection.size(), 1);
                auto it = cd->generic->assumptionCollection.begin();
                EXPECT_EQ(it->second.size(), 2);
            } else if (cd->identifier == "D") {
                for (auto& it : cd->generic->assumptionCollection) {
                    if (dynamic_cast<GenericsTy*>(it.first.get())->name == "V") {
                        EXPECT_EQ(it.second.size(), 2);
                    }
                    if (dynamic_cast<GenericsTy*>(it.first.get())->name == "U") {
                        EXPECT_EQ(it.second.size(), 3);
                    }
                }
            } else if (cd->identifier == "E") {
                EXPECT_EQ(cd->generic->assumptionCollection.size(), 1);
                auto it = cd->generic->assumptionCollection.begin();
                EXPECT_EQ(it->second.size(), 1);
            } else if (cd->identifier == "F") {
                EXPECT_EQ(cd->generic->assumptionCollection.size(), 1);
                auto it = cd->generic->assumptionCollection.begin();
                EXPECT_EQ(it->second.size(), 2);
            } else if (cd->identifier == "G") {
                EXPECT_EQ(cd->generic->assumptionCollection.size(), 1);
                auto it = cd->generic->assumptionCollection.begin();
                EXPECT_EQ(it->second.size(), 2);
            }
        }
    }
}

TEST_F(TypeCheckerTest, DISABLED_SpawnTest)
{
#ifdef _WIN32
    srcPath = projectPath + "\\unittests\\Sema\\SemaCangjieFiles\\";
#elif defined(__unix__)
    srcPath = projectPath + "/unittests/Sema/SemaCangjieFiles/";
#endif
    auto srcFile = srcPath + "spawn.cj";
    std::string failedReason;
    auto content = FileUtil::ReadFileContent(srcFile, failedReason);
    if (!content.has_value()) {
        diag.DiagnoseRefactor(
            DiagKindRefactor::module_read_file_to_buffer_failed, DEFAULT_POSITION, srcFile, failedReason);
    }
    instance->code = std::move(content.value());
    instance->invocation.globalOptions.implicitPrelude = true;
    instance->Compile();

    auto expectedErrorCount = 0;
    EXPECT_EQ(diag.GetErrorCount(), expectedErrorCount);
    EXPECT_EQ(instance->GetSourcePackages()[0]->files.size(), 1);

    Ptr<Node> targetFutureObj1{nullptr};
    Ptr<Node> targetFutureObj2{nullptr};
    for (auto& decl : instance->GetSourcePackages()[0]->files[0]->decls) {
        auto main = As<ASTKind::FUNC_DECL>(decl.get());
        EXPECT_TRUE(main);
        EXPECT_TRUE(main->funcBody->body->body.size() == 4);
        targetFutureObj1 = main->funcBody->body->body[1].get();
        targetFutureObj2 = main->funcBody->body->body[2].get();
    }

    auto var1 = As<ASTKind::VAR_DECL>(targetFutureObj1);
    auto var2 = As<ASTKind::VAR_DECL>(targetFutureObj2);
    EXPECT_TRUE(var1);
    EXPECT_TRUE(var2);
    auto ty1 = dynamic_cast<ClassTy*>(var1->GetTy().get());
    auto ty2 = dynamic_cast<ClassTy*>(var2->GetTy().get());
    EXPECT_TRUE(ty1 && ty2 && ty1 == ty2);
    EXPECT_TRUE(ty1->decl->identifier == "Future");
    EXPECT_TRUE(ty1->typeArgs.size() == 1);
    EXPECT_TRUE(ty1->typeArgs[0]->kind == TypeKind::TYPE_INT64);
}

TEST_F(TypeCheckerTest, DiagnosticNodeTest)
{
    std::string code = R"(
interface I {
    func foo(): Unit
}

class C1 <: I {}
struct S1 <: I {}

main() {}
    )";

    instance->code = code;
    instance->invocation.globalOptions.implicitPrelude = true;
    instance->Compile(CompileStage::SEMA);

    auto diags = diag.GetCategoryDiagnostic(DiagCategory::SEMA);
    int foundDiag = 0;
    for (auto& d : diags) {
        if (d.rKind == DiagKindRefactor::sema_class_need_abstract_modifier_or_func_need_impl) {
            foundDiag++;
            EXPECT_TRUE(d.node != nullptr);
            EXPECT_EQ(d.node->astKind, ASTKind::CLASS_DECL);
            auto classDecl = StaticAs<ASTKind::CLASS_DECL>(d.node);
            EXPECT_EQ(classDecl->identifier.Val(), "C1");
            EXPECT_FALSE(d.subDiags.empty());
            auto noteNode = d.subDiags[0].GetNodeSubDiagAt();
            EXPECT_TRUE(noteNode != nullptr);
            EXPECT_EQ(noteNode->astKind, ASTKind::FUNC_DECL);
            auto funcDecl = StaticAs<ASTKind::FUNC_DECL>(noteNode);
            EXPECT_EQ(funcDecl->identifier.Val(), "foo");
            EXPECT_EQ(funcDecl->outerDecl->identifier.Val(), "I");
        }
        if (d.rKind == DiagKindRefactor::sema_need_member_implementation) {
            foundDiag++;
            EXPECT_TRUE(d.node != nullptr);
            EXPECT_EQ(d.node->astKind, ASTKind::STRUCT_DECL);
            auto structDecl = StaticAs<ASTKind::STRUCT_DECL>(d.node);
            EXPECT_EQ(structDecl->identifier.Val(), "S1");
            EXPECT_FALSE(d.subDiags.empty());
            auto noteNode = d.subDiags[0].GetNodeSubDiagAt();
            EXPECT_TRUE(noteNode != nullptr);
            EXPECT_EQ(noteNode->astKind, ASTKind::FUNC_DECL);
            auto funcDecl = StaticAs<ASTKind::FUNC_DECL>(noteNode);
            EXPECT_EQ(funcDecl->identifier.Val(), "foo");
            EXPECT_EQ(funcDecl->outerDecl->identifier.Val(), "I");
        }
    }
    EXPECT_EQ(foundDiag, 2);
}
