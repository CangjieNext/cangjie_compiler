// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "InheritanceChecker.h"

#include "NativeFFI/Utils.h"
#include "NativeFFI/Java/Utils.h"
#include "cangjie/AST/Clone.h"
#include "cangjie/AST/Match.h"
#include "cangjie/AST/Utils.h"

namespace Cangjie::Native::FFI::Java {

namespace {

std::string GetAnnoValue(Ptr<Annotation> anno)
{
    CJC_ASSERT(anno);
    CJC_ASSERT(anno->args.size() == 1);

    auto litExpr = DynamicCast<LitConstExpr>(anno->args[0]->expr.get());
    CJC_ASSERT(litExpr);
    return litExpr->stringValue;
}

void DiagConflictingForeignName(
    DiagnosticEngine& diag, const Decl& declWithAnno, const Decl& otherDecl, const Decl& checkingDecl)
{
    auto anno = GetForeignNameAnnotation(declWithAnno);
    CJC_ASSERT(anno);

    auto builder = [&diag, &anno, &declWithAnno]() {
        if (!anno->TestAttr(Attribute::COMPILER_ADD)) {
            auto declWithAnnoRange = MakeRange(anno->GetBegin(), declWithAnno.identifier.End());
            return diag.DiagnoseRefactor(DiagKindRefactor::sema_foreign_name_conflicting_annotation, declWithAnno,
                declWithAnnoRange, declWithAnno.identifier, anno->identifier);
        } else {
            return diag.DiagnoseRefactor(DiagKindRefactor::sema_foreign_name_conflicting_derived_annotation,
                declWithAnno,
                MakeRange(declWithAnno.identifier),
                declWithAnno.identifier,
                anno->identifier,
                GetAnnoValue(anno));
        }
    }();

    auto otherAnno = GetForeignNameAnnotation(otherDecl);
    if (otherAnno && !otherAnno->TestAttr(Attribute::COMPILER_ADD)) {
        auto otherDeclRange = MakeRange(otherAnno->GetBegin(), otherDecl.identifier.End());
        builder.AddNote(otherDecl, otherDeclRange,
            "Other declaration '" + otherDecl.identifier + "' has a different '@ForeignName'");
    } else if (otherAnno) {
        builder.AddNote(otherDecl, MakeRange(otherDecl.identifier),
            "Other declaration '" + otherDecl.identifier + "' has a different derived '@ForeignName' '" +
                GetAnnoValue(otherAnno) + "'");
    } else {
        auto otherDeclRange = MakeRange(otherDecl.identifier);
        builder.AddNote(otherDecl, otherDeclRange,
            "Other declaration '" + otherDecl.identifier + "' doesn't have a '@ForeignName'");
    }

    builder.AddNote(checkingDecl, MakeRange(checkingDecl.identifier),
        "While checking declaration '" + checkingDecl.identifier + "'");
}

bool NeedCheck(const MemberSignature& parent, const MemberSignature& child)
{
    if (child.decl->outerDecl->TestAttr(Attribute::IMPORTED)) {
        return false;
    }
    if (!parent.decl->IsFuncOrProp()) {
        return false;
    }
    CJC_ASSERT(child.decl->IsFuncOrProp());

    if (!parent.decl->outerDecl->IsJavaForeign() && !parent.decl->outerDecl->IsJavaExport()) {
        return false;
    }
    if (!child.decl->outerDecl->IsJavaForeign() && !child.decl->outerDecl->IsJavaExport()) {
        // @JavaMirror annotation might be missing here, will report it later
        return false;
    }
    if (parent.decl->outerDecl == child.decl->outerDecl) {
        return false;
    }

    return true;
}

} // namespace

void CheckForeignName(DiagnosticEngine& diag, TypeManager& typeManager, const MemberSignature& parent,
    const MemberSignature& child, const Decl& checkingDecl)
{
    if (!NeedCheck(parent, child)) {
        return;
    }

    auto childAnno = GetForeignNameAnnotation(*child.decl);
    auto parentAnno = GetForeignNameAnnotation(*parent.decl);
    if (!childAnno && !parentAnno) {
        return;
    }

    if (!typeManager.IsSubtype(child.structTy, parent.structTy)) {
        if (!childAnno && parentAnno) {
            DiagConflictingForeignName(diag, *parent.decl, *child.decl, checkingDecl);
        } else if (!parentAnno && childAnno) {
            DiagConflictingForeignName(diag, *child.decl, *parent.decl, checkingDecl);
        } else if (GetAnnoValue(childAnno) != GetAnnoValue(parentAnno)) {
            DiagConflictingForeignName(diag, *parent.decl, *child.decl, checkingDecl);
        }
        return;
    }

    if (childAnno && !childAnno->TestAttr(Attribute::COMPILER_ADD)) {
        auto range = MakeRange(childAnno->GetBegin(), child.decl->identifier.End());
        diag.DiagnoseRefactor(DiagKindRefactor::sema_foreign_name_appeared_in_child,
            *child.decl,
            range,
            childAnno->identifier);
    } else if (childAnno && !parentAnno) {
        DiagConflictingForeignName(diag, *child.decl, *parent.decl, checkingDecl);
    } else if (!childAnno && parentAnno && child.replaceOther) {
        // NOTE: When replaceOther is true, then this method is overriding some other parent one
        // And if there is no ForeignName, then that parent also hadn't ForeignName. But current
        // parent do have it
        DiagConflictingForeignName(diag, *parent.decl, *child.decl, checkingDecl);
    } else if (!childAnno && parentAnno) {
        auto clonedAnno = ASTCloner::Clone(parentAnno);
        clonedAnno->EnableAttr(Attribute::COMPILER_ADD);
        CopyBasicInfo(child.decl, clonedAnno.get());
        child.decl->annotations.emplace_back(std::move(clonedAnno));
    } else if (GetAnnoValue(childAnno) != GetAnnoValue(parentAnno)) {
        DiagConflictingForeignName(diag, *parent.decl, *child.decl, checkingDecl);
    }
}

namespace {
/**
 * @brief Generates a synthetic function stub based on an existing function declaration.
 *
 * This function creates a clone of the provided function declaration (fd),
 * replaces its outerDecl to synthetic class, and then inserts the
 * modified function declaration into the specified synthetic class declaration.
 *
 * @param synthetic The class declaration where the cloned function stub will be inserted.
 * @param fd The original function declaration that will be cloned and modified.
 */
void GenerateSyntheticClassFuncStub(const ClassDecl& synthetic, FuncDecl& fd)
{
    OwnedPtr<FuncDecl> funcStub = ASTCloner::Clone(Ptr(&fd));
    funcStub->DisableAttr(Attribute::DEFAULT);

    // remove foreign anno from cloned func decl
    for (auto it = funcStub->annotations.begin(); it != funcStub->annotations.end(); ++it) {
        if ((*it)->kind == AnnotationKind::FOREIGN_NAME) {
            funcStub->annotations.erase(it);
            break;
        }
    }

    RebindClonedStubToSynthetic(*funcStub, const_cast<ClassDecl&>(synthetic));
    synthetic.body->decls.emplace_back(std::move(funcStub));
}

void InsertSyntheticAbstractWrapperMethodStubs(const ClassDecl& wrapper,
    const MemberMap& members)
{
    for (const auto& idMemberSignature : members) {
        const auto& signature = idMemberSignature.second;

        // only abstract functions must be inside synthetic class
        if (!signature.decl->TestAttr(Attribute::ABSTRACT)) {
            continue;
        }

        // JObject already has implementation of java ref getter
        if (IsJavaRefGetter(*signature.decl)) {
            continue;
        }

        switch (signature.decl->astKind) {
            case ASTKind::FUNC_DECL:
                GenerateSyntheticClassFuncStub(wrapper, *StaticAs<ASTKind::FUNC_DECL>(signature.decl));
                break;
            default:
                continue;
        }
    }
}
} // namespace

void GenerateJavaForeignAbstractStub(const std::unordered_map<Ptr<const AST::InheritableDecl>, MemberMap>& memberMap)
{
    using namespace Cangjie::Interop::Java;
    for (auto& [decl, members] : memberMap) {
        if (auto cd = DynamicCast<ClassDecl*>(decl); cd && IsSyntheticMirrorWrapper(*decl)) {
            InsertSyntheticAbstractWrapperMethodStubs(*cd, members);
        }
    }
}

} // namespace Cangjie::Native::FFI::Java
