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
        // multi-element TupleType AND the left operand also being a tuple of the
        // same arity. Single-element tuple types and casts where the left side is
        // not a tuple fall back to the existing `Option<T>` wrapping to preserve
        // backwards compatibility (e.g. `someValue as ((Int64, String))`).
        auto tupleAsType = ae.asType && ae.asType->astKind == ASTKind::TUPLE_TYPE
            ? StaticCast<TupleType*>(ae.asType.get()) : nullptr;
        auto leftTy = ae.leftExpr->GetTy();
        auto leftTupleTy = Ty::IsTyCorrect(leftTy) && leftTy->IsTuple() ? StaticCast<TupleTy*>(leftTy) : nullptr;
        if (tupleAsType != nullptr && tupleAsType->fieldTypes.size() > 1 &&
            leftTupleTy != nullptr && leftTupleTy->typeArgs.size() == tupleAsType->fieldTypes.size()) {
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
    // Tuple-destructuring cast: same gating as the synthesizer; only enter the
    // tuple branch if asType is a multi-element TupleType AND the synthesized
    // type is a matching-arity tuple of Options. Single-element tuple types
    // (e.g. `someValue as ((Int64, String))`) fall through to the normal
    // Option<T> type-check path.
    auto tupleAsType = ae.asType && ae.asType->astKind == ASTKind::TUPLE_TYPE
        ? StaticCast<TupleType*>(ae.asType.get()) : nullptr;
    if (tupleAsType != nullptr && tupleAsType->fieldTypes.size() > 1 &&
        Ty::IsTyCorrect(ae.GetTy()) && ae.GetTy()->IsTuple()) {
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
    (void)ctx; // reserved for future scope/type context checks
    // Pre-condition: `ae.asType` is a TUPLE_TYPE with fieldTypes.size() > 1 and
    // `ae.leftExpr` already type-checked to a TUPLE_TY with the same arity. The
    // dispatcher in SynAsExpr enforces these checks; we only need to materialize
    // the per-element Option<T> tuple here.
    auto tupleAsType = StaticCast<TupleType*>(ae.asType.get());
    CJC_NULLPTR_CHECK(tupleAsType);
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
    (void)ctx; // reserved for future scope/type context checks
    if (!Ty::IsTyCorrect(ae.GetTy()) || !ae.GetTy()->IsTuple()) {
        DiagMismatchedTypes(diag, ae, target);
        ae.SetTy(TypeManager::GetInvalidTy());
        return false;
    }
    auto resultTupleTy = StaticCast<TupleTy*>(ae.GetTy());
    auto castTupleTy = StaticCast<TupleType*>(ae.asType.get());
    if (!Ty::IsTyCorrect(&target) || !target.IsTuple() ||
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
