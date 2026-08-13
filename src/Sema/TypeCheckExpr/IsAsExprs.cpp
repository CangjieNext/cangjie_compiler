// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "TypeCheckerImpl.h"

#include "Diags.h"

using namespace Cangjie;
using namespace Sema;

Ptr<Ty> TypeChecker::TypeCheckerImpl::SynIsExpr(ASTContext& ctx, IsExpr& ie)
{
    if (Ty::IsTyCorrect(Synthesize({ctx, SynPos::EXPR_ARG}, ie.leftExpr.get())) &&
        Ty::IsTyCorrect(Synthesize({ctx, SynPos::NONE}, ie.isType.get())) && ReplaceIdealTy(*ie.leftExpr)) {
        ie.SetTy(TypeManager::GetPrimitiveTy(TypeKind::TYPE_BOOLEAN));
    } else {
        ie.SetTy(TypeManager::GetInvalidTy());
    }
    return ie.GetTy();
}

bool TypeChecker::TypeCheckerImpl::ChkIsExpr(ASTContext& ctx, Ty& target, IsExpr& ie)
{
    // Always type checking the expression even if the target type mismatches.
    auto ty = SynIsExpr(ctx, ie);
    if (!Ty::IsTyCorrect(ty)) {
        return false;
    }
    bool isWellTyped = ty->IsBoolean();

    auto boolTy = TypeManager::GetPrimitiveTy(TypeKind::TYPE_BOOLEAN);
    if (!typeManager.IsLitBoxableType(boolTy, &target)) {
        DiagMismatchedTypesWithFoundTy(diag, ie, target, *boolTy);
        isWellTyped = false;
    }

    ie.SetTy(isWellTyped ? ie.GetTy() : TypeManager::GetInvalidTy());
    return isWellTyped;
}

Ptr<Ty> TypeChecker::TypeCheckerImpl::SynAsExpr(ASTContext& ctx, AsExpr& ae)
{
    if (Ty::IsTyCorrect(Synthesize({ctx, SynPos::EXPR_ARG}, ae.leftExpr.get())) &&
        Ty::IsTyCorrect(Synthesize({ctx, SynPos::NONE}, ae.asType.get())) && ReplaceIdealTy(*ae.leftExpr)) {
        // Tuple-destructuring cast: `e as (A1, A2, ..., An)`. Each element of the
        // left-hand tuple is cast to the corresponding `Ai` independently, producing
        // a tuple of per-element Options. This branch is gated on `asType` being a
        // TupleType, leaving the existing Option<T> wrapping for single-type casts
        // untouched for backwards compatibility.
        if (ae.asType && ae.asType->astKind == ASTKind::TUPLE_TYPE) {
            return SynAsExprWithTupleType(ctx, ae);
        }
        auto optionDecl = RawStaticCast<EnumDecl*>(importManager.GetCoreDecl("Option"));
        if (optionDecl) {
            ae.SetTy(typeManager.GetEnumTy(*optionDecl, {ae.asType->GetTy()}));
        } else {
            diag.Diagnose(ae, DiagKind::sema_no_core_object);
            ae.SetTy(TypeManager::GetInvalidTy());
        }
    } else {
        ae.SetTy(TypeManager::GetInvalidTy());
    }
    return ae.GetTy();
}

bool TypeChecker::TypeCheckerImpl::ChkAsExpr(ASTContext& ctx, Ty& target, AsExpr& ae)
{
    if (!Ty::IsTyCorrect(SynAsExpr(ctx, ae))) {
        return false;
    }
    // Tuple-destructuring cast: target must be a tuple of matching arity, and the
    // per-element options propagate directly into the resulting tuple type.
    if (ae.asType && ae.asType->astKind == ASTKind::TUPLE_TYPE) {
        return ChkAsExprWithTupleType(ctx, target, ae);
    }
    if (!CheckOptionBox(target, *ae.GetTy())) {
        DiagMismatchedTypes(diag, ae, target);
        ae.SetTy(TypeManager::GetInvalidTy());
        return false;
    }
    return true;
}

Ptr<Ty> TypeChecker::TypeCheckerImpl::SynAsExprWithTupleType(ASTContext& ctx, AsExpr& ae)
{
    auto leftTy = ae.leftExpr->GetTy();
    auto tupleAsType = StaticAs<ASTKind::TUPLE_TYPE>(ae.asType.get());
    CJC_NULLPTR_CHECK(tupleAsType);

    if (!Ty::IsTyCorrect(leftTy) || !leftTy->IsTuple()) {
        diag.DiagnoseRefactor(DiagKindRefactor::sema_invalid_tuple_destructuring_cast_left,
            *ae.leftExpr);
        ae.SetTy(TypeManager::GetInvalidTy());
        return ae.GetTy();
    }
    auto leftTupleTy = StaticCast<TupleTy*>(leftTy);
    if (leftTupleTy->typeArgs.size() != tupleAsType->fieldTypes.size()) {
        diag.DiagnoseRefactor(DiagKindRefactor::sema_invalid_tuple_destructuring_cast_arity,
            *ae.leftExpr, std::to_string(leftTupleTy->typeArgs.size()),
            std::to_string(tupleAsType->fieldTypes.size()));
        ae.SetTy(TypeManager::GetInvalidTy());
        return ae.GetTy();
    }
    // The resulting tuple type is (Option<A1>, Option<A2>, ..., Option<An>).
    // Each element gets wrapped by the per-element `as` cast (see DesugarAsExpr).
    std::vector<Ptr<Ty>> elemTys;
    elemTys.reserve(tupleAsType->fieldTypes.size());
    auto optionDecl = RawStaticCast<EnumDecl*>(importManager.GetCoreDecl("Option"));
    CJC_NULLPTR_CHECK(optionDecl);
    for (auto& ft : tupleAsType->fieldTypes) {
        elemTys.push_back(typeManager.GetEnumTy(*optionDecl, {ft->GetTy()}));
    }
    ae.SetTy(typeManager.GetTupleTy(elemTys));
    return ae.GetTy();
}

bool TypeChecker::TypeCheckerImpl::ChkAsExprWithTupleType(ASTContext& ctx, Ty& target, AsExpr& ae)
{
    if (!Ty::IsTyCorrect(ae.GetTy()) || !ae.GetTy()->IsTuple()) {
        DiagMismatchedTypes(diag, ae, target);
        ae.SetTy(TypeManager::GetInvalidTy());
        return false;
    }
    auto resultTupleTy = StaticCast<TupleTy*>(ae.GetTy());
    auto castTupleTy = StaticAs<ASTKind::TUPLE_TYPE>(ae.asType.get());
    if (!Ty::IsTyCorrect(target) || !target.IsTuple() ||
        StaticCast<TupleTy*>(&target)->typeArgs.size() != castTupleTy->fieldTypes.size()) {
        DiagMismatchedTypes(diag, ae, target);
        ae.SetTy(TypeManager::GetInvalidTy());
        return false;
    }
    auto targetTupleTy = StaticCast<TupleTy*>(&target);
    if (targetTupleTy->typeArgs.size() != resultTupleTy->typeArgs.size()) {
        DiagMismatchedTypes(diag, ae, target);
        ae.SetTy(TypeManager::GetInvalidTy());
        return false;
    }
    for (size_t i = 0; i < resultTupleTy->typeArgs.size(); ++i) {
        if (!CheckOptionBox(*targetTupleTy->typeArgs[i], *resultTupleTy->typeArgs[i])) {
            DiagMismatchedTypes(diag, ae, target);
            ae.SetTy(TypeManager::GetInvalidTy());
            return false;
        }
    }
    return true;
}
