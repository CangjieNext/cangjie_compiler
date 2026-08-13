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

namespace Cangjie::Sema::Desugar::AfterTypeCheck {
// Helpers in this translation unit only. `static` gives them internal
// linkage so they do not pollute the namespace. They MUST live inside
// `Cangjie::Sema::Desugar::AfterTypeCheck` so unqualified names like
// `LookupEnumMember` and `CreateRuntimePreparedTypePattern` (defined in the
// same namespace) resolve correctly.
static OwnedPtr<CallExpr> CreateAsExprSomeCall(
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

// Build a per-element tuple-access SubscriptExpr `base[i]` carrying the
// static type of the i-th tuple element. Equivalent to CreateTupleAccess but
// keeps ownership transfer explicit (no intermediate OwnedPtr that is later
// moved into a field, which trips static analysis).
static OwnedPtr<SubscriptExpr> BuildElemAccess(Expr& base, size_t i, Ty& elemTy)
{
    auto elemAccess = MakeOwnedNode<SubscriptExpr>();
    elemAccess->baseExpr = ASTCloner::Clone(Ptr(&base));
    elemAccess->indexExprs.emplace_back(
        MakeOwnedNode<LitConstExpr>(LitConstKind::INTEGER, std::to_string(i)));
    elemAccess->indexExprs[0]->constNumValue.asInt.SetUint64(i);
    elemAccess->isTupleAccess = true;
    elemAccess->SetTy(&elemTy);
    return elemAccess;
}

// Build a sub-AsExpr `e[i] as Ai` whose Ty is the per-element Option<Ai>.
static OwnedPtr<AsExpr> BuildElemAsExpr(AsExpr& ae, OwnedPtr<Expr> elemAccess, Type& aiTy, Ty& optionElemTy)
{
    auto elemAs = MakeOwnedNode<AsExpr>();
    elemAs->begin = ae.begin;
    elemAs->end = ae.end;
    elemAs->asPos = ae.asPos;
    elemAs->leftExpr = std::move(elemAccess);
    elemAs->asType = ASTCloner::Clone(Ptr(&aiTy));
    elemAs->SetTy(&optionElemTy);
    return elemAs;
}

// Desugar the tuple-destructuring cast `e as (A1, ..., An)`. Returns true if
// the cast was handled (success or defensive skip). Caller must check
// `ae.asType->astKind == TUPLE_TYPE` before invoking.
static bool DesugarAsExprTuple(TypeManager& typeManager, AsExpr& ae)
{
    if (!Ty::IsTyCorrect(ae.GetTy()) || !ae.GetTy()->IsTuple()) {
        return true;
    }
    auto tupleAsType = StaticCast<TupleType*>(ae.asType.get());
    auto outerTupleTy = StaticCast<TupleTy*>(ae.GetTy());
    if (outerTupleTy->typeArgs.size() != tupleAsType->fieldTypes.size()) {
        return true; // rejected by SynAsExprWithTupleType
    }
    auto baseForAccess = ASTCloner::Clone(Ptr(ae.leftExpr.get()));
    auto tupleLit = MakeOwnedNode<TupleLit>();
    tupleLit->leftParenPos = tupleAsType->leftParenPos;
    CopyBasicInfo(&ae, tupleLit.get());
    for (size_t i = 0; i < tupleAsType->fieldTypes.size(); ++i) {
        auto elemAccess = BuildElemAccess(*baseForAccess, i, *outerTupleTy->typeArgs[i]);
        auto elemAs = BuildElemAsExpr(ae, std::move(elemAccess), *tupleAsType->fieldTypes[i],
            *outerTupleTy->typeArgs[i]);
        // Recursively desugar so each child becomes the match form in place.
        DesugarAsExpr(typeManager, *elemAs);
        tupleLit->children.emplace_back(std::move(elemAs));
    }
    tupleLit->SetTy(ae.GetTy());
    ae.desugarExpr = std::move(tupleLit);
    // TupleLit does not derive from MatchExpr and has no `sugarKind` field;
    // the surrounding `Expr::SugarKind::AS` for the tuple destructuring is
    // carried by the outer AsExpr node itself, not its desugarExpr. Skip the
    // sugarKind assignment here (Compare with DesugarAsExprOption which
    // constructs a MatchExpr that does carry sugarKind).
    AddCurFile(*ae.desugarExpr, ae.curFile);
    return true;
}

// Desugar the scalar `e as T` cast producing `match { Some(newVar), None }`.
// Returns true if the cast was handled.
static bool DesugarAsExprOption(TypeManager& typeManager, AsExpr& ae)
{
    if (!Ty::IsTyCorrect(ae.GetTy()) || !ae.GetTy()->IsCoreOptionType()) {
        return true;
    }
    auto optionTy = ae.GetTy();
    auto selectorTy = ae.leftExpr->GetTy();
    auto theAsType = ASTCloner::Clone(ae.asType.get());
    auto theAsTy = ae.asType->GetTy();
    auto optionDecl = StaticCast<EnumTy*>(optionTy)->decl;
    auto someDecl = StaticCast<FuncDecl*>(LookupEnumMember(optionDecl, OPTION_VALUE_CTOR));
    auto varPattern = CreateVarPattern(V_COMPILER, theAsTy);
    auto varDecl = varPattern->varDecl.get();
    varPattern->begin = ae.asPos;
    varPattern->end = ae.asPos;
    varDecl->fullPackageName = ae.GetFullPackageName();
    auto noneDecl = LookupEnumMember(optionDecl, OPTION_NONE_CTOR);
    auto wildcard = MakeOwnedNode<WildcardPattern>();
    wildcard->SetTy(selectorTy);
    auto none = CreateRefExpr(*noneDecl);
    CopyBasicInfo(&ae, none.get());
    none->typeArguments.emplace_back(std::move(theAsType));
    none->instTys.emplace_back(theAsTy);
    none->SetTy(optionTy);
    std::vector<OwnedPtr<MatchCase>> matchCases;
    matchCases.emplace_back(
        CreateMatchCase(
            CreateRuntimePreparedTypePattern(typeManager, std::move(varPattern), std::move(ae.asType),
                *ae.leftExpr),
            CreateAsExprSomeCall(*someDecl, *none->typeArguments[0], *varDecl,
                *typeManager.GetFunctionTy({theAsTy}, optionTy))));
    matchCases.emplace_back(CreateMatchCase(std::move(wildcard), std::move(none)));
    ae.desugarExpr = CreateMatchExpr(std::move(ae.leftExpr), std::move(matchCases), optionTy, Expr::SugarKind::AS);
    CopyBasicInfo(&ae, ae.desugarExpr.get());
    AddCurFile(*ae.desugarExpr, ae.curFile);
    return true;
}

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
    // Mirror the Sema gating: the tuple-destructuring branch only applies
    // when `asType` is a multi-element TUPLE_TYPE AND the synthesized type
    // is itself a tuple (i.e. (Option<A1>, ..., Option<An>)). Otherwise
    // the cast fell back to `Option<T>` in SynAsExpr and must be desugared
    // as a scalar Option cast. Without this gating we would route a
    // legacy `Option<(T1, T2)>` cast into the tuple branch and leave
    // `ae.desugarExpr` unset, which crashes the code generator when it
    // walks the as-expression's desugarExpr chain.
    auto tupleAsType = ae.asType->astKind == ASTKind::TUPLE_TYPE
        ? StaticCast<TupleType*>(ae.asType.get()) : nullptr;
    bool isTupleCast = tupleAsType != nullptr && tupleAsType->fieldTypes.size() > 1 &&
        Ty::IsTyCorrect(ae.GetTy()) && ae.GetTy()->IsTuple();
    if (isTupleCast) {
        DesugarAsExprTuple(typeManager, ae);
    } else {
        DesugarAsExprOption(typeManager, ae);
    }
}
} // namespace Cangjie::Sema::Desugar::AfterTypeCheck