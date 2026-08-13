// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "Desugar/AfterTypeCheck.h"

#include "TypeCheckUtil.h"

#include "cangjie/AST/Create.h"
#include "cangjie/AST/Utils.h"

using namespace Cangjie;
using namespace AST;
using namespace TypeCheckUtil;

namespace {
OwnedPtr<CallExpr> CreateAsExprSomeCall(
    FuncDecl& someDecl, Type& theAsType, VarDecl& varDecl, FuncTy& someCtorTy)
{
    CJC_ASSERT(someCtorTy.paramTys.size() == 1);
    auto some = MakeOwnedNode<CallExpr>();
    auto someRef = CreateRefExpr(someDecl);
    auto theAsTy = someCtorTy.paramTys[0];
    auto optionTy = someCtorTy.retTy;
    someRef->typeArguments.emplace_back(ASTCloner::Clone(Ptr(&theAsType)));
    someRef->instTys.emplace_back(theAsTy);
    someRef->ref.targets.emplace_back(&someDecl);
    someRef->isAlone = false;
    someRef->SetTy(&someCtorTy);
    someRef->callOrPattern = some.get();
    auto newVarRef = CreateRefExpr(varDecl);
    newVarRef->SetTy(theAsTy);
    auto someArg = MakeOwnedNode<FuncArg>();
    someArg->expr = std::move(newVarRef);
    someArg->SetTy(theAsTy);
    some->baseFunc = std::move(someRef);
    some->args.emplace_back(std::move(someArg));
    some->resolvedFunction = &someDecl;
    some->callKind = CallKind::CALL_DECLARED_FUNCTION;
    some->SetTy(optionTy);
    return some;
}
} // namespace

namespace Cangjie::Sema::Desugar::AfterTypeCheck {
/**
 * Desugar AsExpr to TypePattern of MatchExpr.
 * *************** before desugar ****************
 * e as T
 * *************** after desugar ****************
 * match (e) {
 *     case newVar : T => Some(newVar)
 *     case _ => None
 * }
 * */
void DesugarAsExpr(TypeManager& typeManager, AsExpr& ae)
{
    if (ae.desugarExpr) {
        return;
    }
    CJC_NULLPTR_CHECK(ae.leftExpr);
    CJC_NULLPTR_CHECK(ae.asType);
    CJC_NULLPTR_CHECK(ae.leftExpr->GetTy());
    CJC_NULLPTR_CHECK(ae.asType->GetTy());

    // Tuple-destructuring cast: `e as (A1, A2, ..., An)`. The result is a TupleLit
    // whose i-th child is `e[i] as Ai`; each child is itself an AsExpr. Because
    // the desugar walker traverses the package tree only (not the desugarExpr
    // chains of new sub-AST), we must recursively desugar each child AsExpr
    // here. Each child becomes the standard `match { Some / None }` form.
    if (ae.asType->astKind == ASTKind::TUPLE_TYPE) {
        if (!Ty::IsTyCorrect(ae.GetTy()) || !ae.GetTy()->IsTuple()) {
            return;
        }
        auto tupleAsType = StaticAs<ASTKind::TUPLE_TYPE>(ae.asType.get());
        auto outerTupleTy = StaticCast<TupleTy*>(ae.GetTy());
        if (outerTupleTy->typeArgs.size() != tupleAsType->fieldTypes.size()) {
            // Defensive: should have been rejected by SynAsExprWithTupleType.
            return;
        }
        auto tupleLit = MakeOwnedNode<TupleLit>();
        tupleLit->leftParenPos = tupleAsType->leftParenPos;
        CopyBasicInfo(&ae, tupleLit.get());
        auto baseForAccess = ASTCloner::Clone(Ptr(ae.leftExpr.get()));
        for (size_t i = 0; i < tupleAsType->fieldTypes.size(); ++i) {
            // Project the i-th element via `e[i]` (a SubscriptExpr flagged
            // `isTupleAccess`). The per-element tuple access carries the static
            // type of the tuple element on the subscript expression. We inline
            // the body of CreateTupleAccess here because we already know
            // baseForAccess's type is a TupleTy (caller sets outerTupleTy via
            // SynAsExprWithTupleType) and to keep ownership transfer explicit.
            auto elemAccess = MakeOwnedNode<SubscriptExpr>();
            elemAccess->baseExpr = ASTCloner::Clone(Ptr(baseForAccess.get()));
            elemAccess->indexExprs.emplace_back(
                MakeOwnedNode<LitConstExpr>(LitConstKind::INTEGER, std::to_string(i)));
            elemAccess->indexExprs[0]->constNumValue.asInt.SetUint64(i);
            elemAccess->isTupleAccess = true;
            elemAccess->SetTy(outerTupleTy->typeArgs[i]);
            // Build the per-element cast `e[i] as Ai`.
            auto elemAs = MakeOwnedNode<AsExpr>();
            elemAs->begin = ae.begin;
            elemAs->end = ae.end;
            elemAs->asPos = ae.asPos;
            elemAs->leftExpr = std::move(elemAccess);
            elemAs->asType = ASTCloner::Clone(Ptr(tupleAsType->fieldTypes[i].get()));
            // Use the Option<Ai> type already computed for the outer tuple
            // (carried in outerTupleTy->typeArgs[i]). SynAsExprWithTupleType set
            // this for the entire tuple; we mirror it on the sub-AsExpr so the
            // recursive DesugarAsExpr call can recognize it as Core Option and
            // emit the standard match rewrite.
            elemAs->SetTy(outerTupleTy->typeArgs[i]);
            // Recursively desugar the sub-AsExpr so it becomes a match expression
            // in place. This is needed because the desugar walker does not follow
            // the outer AsExpr's desugarExpr chain.
            DesugarAsExpr(typeManager, *elemAs);
            tupleLit->children.emplace_back(std::move(elemAs));
        }
        tupleLit->SetTy(ae.GetTy());
        ae.desugarExpr = std::move(tupleLit);
        ae.desugarExpr->sugarKind = Expr::SugarKind::AS;
        AddCurFile(*ae.desugarExpr, ae.curFile);
        return;
    }

    if (!Ty::IsTyCorrect(ae.GetTy()) || !ae.GetTy()->IsCoreOptionType()) {
        return;
    }
    auto optionTy = ae.GetTy();
    auto selectorTy = ae.leftExpr->GetTy();
    auto theAsType = ASTCloner::Clone(ae.asType.get());
    auto theAsTy = ae.asType->GetTy();
    auto optionDecl = StaticCast<EnumTy*>(optionTy)->decl;
    CJC_NULLPTR_CHECK(optionDecl);
    auto someDecl = StaticCast<FuncDecl*>(LookupEnumMember(optionDecl, OPTION_VALUE_CTOR));
    CJC_NULLPTR_CHECK(someDecl);
    std::vector<OwnedPtr<MatchCase>> matchCases;
    auto varPattern = CreateVarPattern(V_COMPILER, theAsTy);
    varPattern->begin = ae.asPos;
    varPattern->end = ae.asPos;
    auto varDecl = varPattern->varDecl.get();
    varDecl->fullPackageName = ae.GetFullPackageName();
    matchCases.emplace_back(
        CreateMatchCase(
            CreateRuntimePreparedTypePattern(typeManager, std::move(varPattern), std::move(ae.asType), *ae.leftExpr),
            CreateAsExprSomeCall(*someDecl, *theAsType, *varDecl, *typeManager.GetFunctionTy({theAsTy}, optionTy))));
    auto wildcard = MakeOwnedNode<WildcardPattern>();
    wildcard->SetTy(selectorTy);
    auto noneDecl = LookupEnumMember(optionDecl, OPTION_NONE_CTOR);
    CJC_NULLPTR_CHECK(noneDecl);
    auto none = CreateRefExpr(*noneDecl);
    CopyBasicInfo(&ae, none.get());
    none->typeArguments.emplace_back(std::move(theAsType));
    none->instTys.emplace_back(theAsTy);
    none->SetTy(optionTy);
    matchCases.emplace_back(CreateMatchCase(std::move(wildcard), std::move(none)));
    ae.desugarExpr = CreateMatchExpr(std::move(ae.leftExpr), std::move(matchCases), optionTy, Expr::SugarKind::AS);
    CopyBasicInfo(&ae, ae.desugarExpr.get());
    AddCurFile(*ae.desugarExpr, ae.curFile);
}
} // namespace Cangjie::Sema::Desugar::AfterTypeCheck
