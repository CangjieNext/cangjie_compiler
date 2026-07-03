// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "TypeCheckerImpl.h"

#include "DiagSuppressor.h"
#include "Diags.h"
#include "TypeCheckUtil.h"
#include "ExtraScopes.h"

#include "cangjie/Frontend/CompilerInstance.h"

using namespace Cangjie;
using namespace AST;
using namespace Sema;
using namespace TypeCheckUtil;
using namespace Utils;

namespace {
// we should clear the nodes in the block and then recheck the body according to the new `target`.
void ClearLambdaBodyForReCheck(const LambdaExpr& le)
{
    Walker(le.funcBody->body.get(), [](Ptr<Node> node) {
        if (Is<Type>(node) || Is<Generic>(node)) {
            // In the `PreCheck` stage:
            // The `Ty` of `Type` was set by `GetTyFromASTType`.
            // The `Ty` of `GenericParamDecl` was set by `SetDeclTy`.
            return VisitAction::SKIP_CHILDREN;
        } else if (Is<FuncParam>(node) && node->TestAttr(AST::Attribute::HAS_INITIAL)) {
            // The sugar for default parameter was set by `AddDefaultFunction` in the `PrepareTypeCheck` stage.
            return VisitAction::SKIP_CHILDREN;
        } else {
            node->Clear();
            return VisitAction::WALK_CHILDREN;
        }
    }).Walk();
}

bool IsAnyParamTypeOmitted(const LambdaExpr& le)
{
    CJC_ASSERT(le.funcBody && !le.funcBody->paramLists.empty());
    auto& paramList = le.funcBody->paramLists[0];
    return std::any_of(paramList->params.cbegin(), paramList->params.cend(), [](auto& param) {
        return param->type == nullptr;
    });
}

void ClearCacheForNames(ASTContext& ctx, const LambdaExpr& le, const std::vector<std::string>& paramNames)
{
    std::vector<Ptr<const Node>> parentStack;
    std::unordered_set<Ptr<const Node>> clearedNodes;
    std::unordered_set<std::string> aliases(paramNames.cbegin(), paramNames.cend()); // potential aliases of parameters
    std::unordered_map<Ptr<const Node>, std::vector<std::string>> aliasDecl;
    std::unordered_map<Ptr<const Node>, std::vector<std::string>> aliasShadow; // a simplistic version of symbol table
    auto shadowName = [&aliases, &aliasShadow](const LambdaExpr& le) {
        CJC_ASSERT(le.funcBody && !le.funcBody->paramLists.empty());
        for (auto& param : le.funcBody->paramLists[0]->params) {
            CJC_NULLPTR_CHECK(param);
            auto id = param->identifier;
            if (aliases.count(id) > 0) {
                aliasShadow[&le].push_back(id);
                aliases.erase(id);
            }
        }
    };
    auto preAction = [&ctx, &aliases, &clearedNodes, &parentStack, &shadowName](Ptr<Node> node) -> VisitAction {
        parentStack.push_back(node);
        if (auto re = DynamicCast<RefExpr*>(node); re && aliases.count(re->ref.identifier) > 0) {
            for (size_t i = parentStack.size(); i > 0; i--) {
                auto p = parentStack[i - 1];
                if (clearedNodes.count(p) > 0) {
                    break;
                }
                ctx.RemoveTypeCheckCache(*p);
                clearedNodes.insert(p);
            }
        } else if (auto lam = DynamicCast<LambdaExpr*>(node)) {
            shadowName(*lam); // shadow by local var is too complicated to track. skip here
        }
        return aliases.empty() ? VisitAction::SKIP_CHILDREN : VisitAction::WALK_CHILDREN;
    };
    auto postAction = [&aliases, &aliasDecl, &aliasShadow, &parentStack, &clearedNodes](
                          Ptr<const Node> /* node */) -> VisitAction {
        auto top = parentStack.back();
        parentStack.pop_back();
        if (auto decl = DynamicCast<VarDecl*>(top); decl && clearedNodes.count(decl) > 0) {
            aliases.insert(decl->identifier); // VarDecl is cleared ==> it contains alias ==> the var is a new alias
            aliasDecl[parentStack.back()].push_back(decl->identifier);
        }
        for (auto name : aliasShadow[top]) {
            aliases.insert(name);
        }
        for (auto name : aliasDecl[top]) {
            aliases.erase(name);
        }
        return VisitAction::WALK_CHILDREN;
    };
    Walker(le.funcBody->body.get(), preAction, postAction).Walk();
}

void ClearInvalidTypeCheckCache(ASTContext& ctx, const LambdaExpr& le, const ModalTy target)
{
    if ((ctx.lastTargetTypeMap.count(&le) == 0 || ctx.lastTargetTypeMap[&le] != target) && IsAnyParamTypeOmitted(le)) {
        std::vector<std::string> names;
        CJC_ASSERT(le.funcBody && !le.funcBody->paramLists.empty());
        for (auto& param : le.funcBody->paramLists[0]->params) {
            CJC_NULLPTR_CHECK(param);
            if (!param->type) {
                names.push_back(param->identifier);
            }
        }
        ClearCacheForNames(ctx, le, names);
    }
}

// Return true if lambda has any mismatched parameter type and any of the parameter's type is omitted.
bool IsLambdaIncompatible(const LambdaExpr& le, bool paramsMismatched)
{
    return paramsMismatched && IsAnyParamTypeOmitted(le);
}

// return: true if error, false if no error
bool DiagInferParamTyFail(DiagnosticEngine& diag, LambdaExpr& le)
{
    for (auto& node : le.funcBody->paramLists[0]->params) {
        if (!node->type) {
            diag.DiagnoseRefactor(DiagKindRefactor::sema_lambdaExpr_must_have_type_annotation, *node);
            le.SetTy({TypeManager::GetInvalidTy()});
            return true;
        }
    }
    return false;
}

// if it's a member func call to a param, return the param's id and possible type decls
// otherwise return empty string and empty set
std::pair<std::string, std::set<Ptr<Decl>>> FindCandidatesFromCall(
    ASTContext& ctx, const CallExpr& ce, const std::map<std::string, Ptr<FuncParam>>& unsolvedParams)
{
    if (auto ma = DynamicCast<MemberAccess*>(ce.baseFunc.get())) {
        if (auto re = DynamicCast<RefExpr*>(ma->baseExpr.get())) {
            if (unsolvedParams.count(re->ref.identifier) > 0) {
                auto id = re->ref.identifier;
                auto sig = MemSig{ma->field, false, ce.args.size(), ma->typeArguments.size()};
                return {id, ctx.Mem2Decls(sig)};
            }
        }
    }
    return {"", {}};
}

// if it's a member access to a param, return the param's id and possible type decls
// otherwise return empty string and empty set
std::pair<std::string, std::set<Ptr<Decl>>> FindCandidatesFromAccess(
    ASTContext& ctx, MemberAccess& ma, const std::map<std::string, Ptr<FuncParam>>& unsolvedParams)
{
    if (auto re = DynamicCast<RefExpr*>(ma.baseExpr.get())) {
        if (unsolvedParams.count(re->ref.identifier) > 0) {
            auto id = re->ref.identifier;
            auto sig = MemSig{ma.field, true};
            return {id, ctx.Mem2Decls(sig)};
        }
    }
    return {"", {}};
}

Block* GetFuncLikeBody(const Node& funcLike)
{
    if (auto fd = DynamicCast<FuncDecl>(&funcLike)) {
        return fd->funcBody ? fd->funcBody->body.get() : nullptr;
    }
    if (auto le = DynamicCast<LambdaExpr>(&funcLike)) {
        return le->funcBody ? le->funcBody->body.get() : nullptr;
    }
    return nullptr;
}

FuncBody* GetFuncLikeFuncBody(Node& funcLike)
{
    if (auto fd = DynamicCast<FuncDecl>(&funcLike)) {
        return fd->funcBody.get();
    }
    if (auto le = DynamicCast<LambdaExpr>(&funcLike)) {
        return le->funcBody.get();
    }
    return nullptr;
}

LocalModal GetRefCaptureLocalModal(const ASTContext& ctx, const NameReferenceExpr& nre)
{
    if (auto ma = DynamicCast<MemberAccess>(&nre)) {
        if (ma->baseExpr && IsThisOrSuper(*ma->baseExpr)) {
            auto local = ma->baseExpr->TyMode().local;
            if (local == LocalModal::NOT) {
                local = GetCurThisModal(ctx, nre.scopeName).local;
            }
            return local;
        }
    }
    if (auto re = DynamicCast<RefExpr>(&nre)) {
        if (re->isThis || re->isSuper) {
            auto local = re->TyMode().local;
            if (local == LocalModal::NOT) {
                local = GetCurThisModal(ctx, re->scopeName).local;
            }
            return local;
        }
        if (auto target = re->GetTarget()) {
            if (target->IsMemberDecl() && !target->TestAnyAttr(Attribute::STATIC, Attribute::GLOBAL)) {
                return GetCurThisModal(ctx, re->scopeName).local;
            }
            return target->TyMode().local;
        }
    }
    return nre.TyMode().local;
}

std::string GetRefCaptureName(const NameReferenceExpr& nre)
{
    if (auto ma = DynamicCast<MemberAccess>(&nre)) {
        if (ma->baseExpr && IsThisOrSuper(*ma->baseExpr)) {
            return "this";
        }
        return ma->field.Val();
    }
    if (auto re = DynamicCast<RefExpr>(&nre)) {
        if (re->isThis || re->isSuper) {
            return "this";
        }
        if (auto target = re->GetTarget()) {
            return target->identifier.Val();
        }
    }
    return "";
}

bool IsImplicitThisMemberRef(const ASTContext& ctx, const RefExpr& re)
{
    if (re.isThis || re.isSuper) {
        return false;
    }
    auto target = re.GetTarget();
    if (!target || !target->IsMemberDecl() || target->TestAnyAttr(Attribute::STATIC, Attribute::GLOBAL)) {
        return false;
    }
    return GetCurThisModal(ctx, re.scopeName).local != LocalModal::NOT;
}

bool IsReceiverCapture(const ASTContext& ctx, const NameReferenceExpr& nre)
{
    if (auto ma = DynamicCast<MemberAccess>(&nre)) {
        return ma->baseExpr && IsThisOrSuper(*ma->baseExpr);
    }
    if (auto re = DynamicCast<RefExpr>(&nre)) {
        if (re->isThis || re->isSuper) {
            return true;
        }
        return IsImplicitThisMemberRef(ctx, *re);
    }
    return false;
}

template <typename Fn>
void WalkFuncLikeLocalCaptureRefs(const ASTContext& ctx, const Node& funcLike, Block* body, Fn&& visit)
{
    Walker(body, [&ctx, &funcLike, &visit](Ptr<Node> node) {
        if (node->astKind == ASTKind::LAMBDA_EXPR || node->astKind == ASTKind::FUNC_DECL) {
            return VisitAction::SKIP_CHILDREN;
        }
        if (node->TestAttr(Attribute::COMPILER_ADD)) {
            return VisitAction::SKIP_CHILDREN;
        }
        if (auto ma = DynamicCast<MemberAccess>(node); ma && ma->baseExpr && IsThisOrSuper(*ma->baseExpr)) {
            visit(*ma);
            return VisitAction::SKIP_CHILDREN;
        }
        if (auto re = DynamicCast<RefExpr>(node)) {
            if (re->isThis || re->isSuper) {
                visit(*re);
                return VisitAction::SKIP_CHILDREN;
            }
            if (IsImplicitThisMemberRef(ctx, *re)) {
                visit(*re);
                return VisitAction::SKIP_CHILDREN;
            }
            if (auto target = DynamicCast<VarDecl>(re->GetTarget());
                target && !target->TestAnyAttr(Attribute::STATIC, Attribute::GLOBAL)) {
                auto targetDefSite =
                    ScopeManager::GetCurSymbolByKind(SymbolKind::FUNC_LIKE, ctx, target->scopeName);
                if (!targetDefSite || !targetDefSite->node || targetDefSite->node == &funcLike) {
                    return VisitAction::WALK_CHILDREN;
                }
                visit(*re);
            }
        }
        return VisitAction::WALK_CHILDREN;
    }).Walk();
}
} // namespace

void TypeChecker::TypeCheckerImpl::ResetLambdaForReinfer(ASTContext& ctx, const AST::LambdaExpr& le)
{
    le.funcBody->retType.reset(nullptr);
    AddRetTypeNode(*le.funcBody);
    le.funcBody->Clear();
    Walker(le.funcBody->body.get(), [](Ptr<Node> node) {
        if ((Is<Decl>(node) || Is<Expr>(node)) && !Ty::IsInitialTy(node->DataTy())) {
            node->Clear();
        }
        return VisitAction::WALK_CHILDREN;
    }).Walk();
    ctx.ClearTypeCheckCache(*le.funcBody);
}

bool TypeChecker::TypeCheckerImpl::SolveLamExprParamTys(ASTContext& ctx, AST::LambdaExpr& le)
{
    auto tyVars = typeManager.GetInnermostUnsolvedTyVars();
    auto sol = SolveConstraints(typeManager.constraints);
    bool successful = false;
    if (sol && !HasUnsolvedTyVars(*sol, tyVars)) {
        ResetLambdaForReinfer(ctx, le);
        for (auto& node : le.funcBody->paramLists[0]->params) {
            node->SetTy(typeManager.GetInstantiatedTy(node->GetTy(), *sol));
        }
        if (Synthesize({ctx, SynPos::EXPR_ARG}, le.funcBody.get()).IsCorrect() && le.funcBody->body &&
            le.funcBody->body->GetTy().IsCorrect()) {
            le.SetTy(le.funcBody->GetTy());
            successful = true;
        }
    }
    if (!successful && DiagInferParamTyFail(diag, le)) {
        return false;
    }
    return true;
}

void TypeChecker::TypeCheckerImpl::TryInferFromSyntaxInfo(ASTContext& ctx, const AST::LambdaExpr& le)
{
    if (typeManager.GetInnermostUnsolvedTyVars().empty()) {
        return;
    }
    std::map<std::string, Ptr<FuncParam>> unsolvedParams;
    std::map<std::string, std::set<Ptr<Decl>>> candidates;
    for (auto& param : le.funcBody->paramLists[0]->params) {
        if (param->GetTy()->IsPlaceholder()) {
            unsolvedParams[param->identifier] = param;
        }
    }
    if (unsolvedParams.empty()) {
        return;
    }

    auto candiHandler = [&candidates](const std::pair<std::string, std::set<Ptr<Decl>>>& candi) {
        if (candi.first.empty()) {
            return false;
        }
        if (candidates.count(candi.first) == 0) {
            candidates[candi.first] = candi.second;
        } else {
            EraseIf(candidates[candi.first], [&candi](Ptr<Decl> d) { return candi.second.count(d) == 0; });
        }
        return true;
    };
    std::function<VisitAction(Ptr<Node>)> memberScanner = [&ctx, &unsolvedParams, &candiHandler, &memberScanner](
                                                              Ptr<Node> n) -> VisitAction {
        if (auto ce = DynamicCast<CallExpr*>(n)) {
            if (candiHandler(FindCandidatesFromCall(ctx, *ce, unsolvedParams))) {
                // need to skip baseFunc to avoid handling it again as an access to var/prop
                for (auto& arg : ce->args) {
                    Walker(arg.get(), memberScanner).Walk();
                }
                return VisitAction::SKIP_CHILDREN;
            }
        } else if (auto ma = DynamicCast<MemberAccess*>(n)) {
            candiHandler(FindCandidatesFromAccess(ctx, *ma, unsolvedParams));
        }
        return VisitAction::WALK_CHILDREN;
    };
    Walker(le.funcBody.get(), memberScanner).Walk();

    for (auto& [id, decls] : candidates) {
        TryEnforceCandidate(*StaticCast<TyVar*>(unsolvedParams[id]->DataTy()), decls, typeManager);
    }
}

ModalTy TypeChecker::TypeCheckerImpl::SynLamExpr(ASTContext& ctx, LambdaExpr& le)
{
    if (le.funcBody == nullptr || le.funcBody->paramLists.empty()) {
        return {TypeManager::GetInvalidTy()};
    }
    // Lambda's return type is added by compiler, so reset it here to allow re-checking.
    le.funcBody->retType.reset(nullptr);
    AddRetTypeNode(*le.funcBody);
    le.funcBody->retType->begin = le.begin;
    le.funcBody->retType->end = le.end;
    CJC_ASSERT(le.funcBody && !le.funcBody->paramLists.empty());
    TyVarScope sc(typeManager);
    for (auto& node : le.funcBody->paramLists[0]->params) {
        CJC_NULLPTR_CHECK(node);
        ModalTy ty = Synthesize({ctx, SynPos::NONE}, node->type.get());
        if (Ty::IsTyCorrect(ty)) {
            node->SetTy(ty);
        } else if (ctx.funcArgReachable.count(&le) == 0 && !node->type) {
            node->SetTy({typeManager.AllocTyVar("T-Lam", true)});
        } else {
            diag.DiagnoseRefactor(DiagKindRefactor::sema_lambdaExpr_must_have_type_annotation, *node);
            le.SetTy({TypeManager::GetInvalidTy()});
            return le.GetTy();
        }
    }
    TryInferFromSyntaxInfo(ctx, le);
    bool skipSolving = typeManager.GetInnermostUnsolvedTyVars().empty();
    ModalTy leTy{};
    {
        DiagSuppressor ds(diag);
        leTy = Synthesize({ctx, SynPos::EXPR_ARG}, le.funcBody.get());
        if (skipSolving) {
            ds.ReportDiag();
        }
    }
    // `funcBody` containing invalid expressions can have valid types.
    // We have to use `funcBody->body` to determine if there are errors.
    if (!Ty::IsTyCorrect(leTy) || !le.funcBody->body || !le.funcBody->body->GetTy().IsCorrect()) {
        le.SetTy({TypeManager::GetInvalidTy()});
        if (DiagInferParamTyFail(diag, le)) {
            return le.GetTy();
        }
    } else {
        if (skipSolving) {
            le.SetTy(le.funcBody->GetTy());
        } else {
            if (!SolveLamExprParamTys(ctx, le)) {
                return le.GetTy();
            }
        }
    }
    if (auto ft = DynamicCast<FuncTy*>(le.DataTy())) {
        le.funcBody->retType->SetTy(ft->retTy);
    } else {
        le.funcBody->retType->SetTy({TypeManager::GetInvalidTy()});
    }
    if (le.GetTy().IsCorrect()) {
        InferLamExprModal(ctx, le);
        CheckFuncLikeLocalCaptures(ctx, le);
    }
    return le.GetTy();
}

void TypeChecker::TypeCheckerImpl::DiagInvalidLocalFuncType(const Node& le)
{
    diag.DiagnoseRefactor(DiagKindRefactor::sema_invalid_local_func_type, le,
        le.astKind == ASTKind::LAMBDA_EXPR ? "lambda" : "local function");
}

bool TypeChecker::TypeCheckerImpl::IsLocalFuncDecl(const FuncDecl& fd)
{
    return !IsGlobalOrMember(fd);
}

bool TypeChecker::TypeCheckerImpl::CheckLocalCapture(
    const ASTContext& ctx, const Node& func, const NameReferenceExpr& expr) const
{
    auto funcLocal = func.TyMode().local;
    auto varLocal = GetRefCaptureLocalModal(ctx, expr);
    if (!IsReceiverCapture(ctx, expr)) {
        if (auto target = expr.GetTarget()) {
            if (typeManager.ImplementsCopyInterface(target->DataTy())) {
                return true;
            }
        }
    }
    if (funcLocal == LocalModal::NOT && varLocal == LocalModal::NOT) {
        return true;
    }
    if (funcLocal == LocalModal::HALF && varLocal != LocalModal::FULL) {
        return true;
    }
    diag.DiagnoseRefactor(DiagKindRefactor::sema_bad_capture_local, expr,
        std::string(ToString(GetRefCaptureLocalModal(ctx, expr))), GetRefCaptureName(expr),
        func.TyMode().LocalString(), func.astKind == ASTKind::FUNC_DECL ? "function" : "lambda");
    return false;
}

void TypeChecker::TypeCheckerImpl::CheckFuncLikeLocalCaptures(const ASTContext& ctx, Node& funcLike)
{
    if (!funcLike.GetTy().IsCorrect()) {
        return;
    }
    auto body = GetFuncLikeBody(funcLike);
    if (!body) {
        return;
    }
    bool ok = true;
    WalkFuncLikeLocalCaptureRefs(ctx, funcLike, body, [this, &ctx, &funcLike, &ok](const NameReferenceExpr& nre) {
        ok = CheckLocalCapture(ctx, funcLike, nre) && ok;
    });
    if (!ok) {
        funcLike.SetTy({TypeManager::GetInvalidTy()});
        if (auto fb = GetFuncLikeFuncBody(funcLike)) {
            fb->SetTy({TypeManager::GetInvalidTy()});
        }
    }
}

void TypeChecker::TypeCheckerImpl::InferLamExprModal(ASTContext& ctx, LambdaExpr& le)
{
    if (!le.funcBody || !le.funcBody->body || !le.GetTy().IsCorrect()) {
        return;
    }

    // @local? if any outer @local? var / this / super / implicit-this member; @local! ignored for inference.
    bool capturesLocalHalf = false;
    WalkFuncLikeLocalCaptureRefs(ctx, le, le.funcBody->body.get(), [&ctx, &capturesLocalHalf](const NameReferenceExpr& nre) {
        if (GetRefCaptureLocalModal(ctx, nre) == LocalModal::HALF) {
            capturesLocalHalf = true;
        }
    });

    if (capturesLocalHalf) {
        le.SetTy(le.GetTy().With(LocalModal::HALF));
        le.funcBody->SetTy(le.funcBody->GetTy().With(LocalModal::HALF));
    }
}

bool TypeChecker::TypeCheckerImpl::ChkLamExpr(ASTContext& ctx, ModalTy target, LambdaExpr& le)
{
    CJC_ASSERT(le.funcBody != nullptr && !le.funcBody->paramLists.empty());
    ClearInvalidTypeCheckCache(ctx, le, target);
    ctx.lastTargetTypeMap[&le] = target;
    if ((target && target->IsAny()) || (le.TestAttr(AST::Attribute::C) && target && target->IsCType())) {
        le.SetTy(Synthesize({ctx, SynPos::EXPR_ARG}, &le));
        (void)ReplaceIdealTy(le);
        return le.GetTy().IsCorrect();
    }
    ModalTy targetTy = TypeCheckUtil::UnboxOptionType(target);
    if (!Ty::IsTyCorrect(targetTy) || !targetTy->IsFunc()) {
        auto ds = DiagSuppressor(diag);
        auto synTy = Synthesize({ctx, SynPos::EXPR_ARG}, &le);
        le.SetTy({TypeManager::GetInvalidTy()});
        if (!ds.HasError() && Ty::IsTyCorrect(synTy)) { // Only report type mismatch when no error happens.
            DiagMismatchedTypesWithFoundTy(diag, le, target, synTy);
        }
        ds.ReportDiag();
        return false;
    }
    if (targetTy.Mode().local == LocalModal::FULL) {
        DiagInvalidLocalFuncType(le);
        le.SetTy({TypeManager::GetInvalidTy()});
        return false;
    }

    bool chkSucceeded = false;
    { // Suppress errors if check lambda failed. Only report when type check succeed.
        auto ds = DiagSuppressor(diag);
        auto tgtTy = StaticCast<FuncTy*>(targetTy.Ty());
        std::vector<ModalTy> lamParamTys;
        // Check arguments' types against parameters' types.
        bool paramsMatched = ChkLamParamTys(ctx, le, tgtTy->paramTys, lamParamTys);
        // In the check mode, a lambda's return type is explicitly given.
        // To ease the following check, we create a retType node from the given target type.
        le.funcBody->retType = MakeOwned<RefType>();
        le.funcBody->retType->begin = le.begin;
        le.funcBody->retType->end = le.end;
        le.funcBody->retType->SetTy(tgtTy->retTy);
        le.funcBody->retType->EnableAttr(AST::Attribute::COMPILER_ADD);
        ClearLambdaBodyForReCheck(le);

        // Should not be short-circuited.
        if (ChkLamBody(ctx, *le.funcBody) && paramsMatched) {
            ds.ReportDiag();
            // The call to GetFunctionTy is necessary to create (cached) CPointer types if necessary.
            ModalTy fnTy{typeManager.GetFunctionTy(lamParamTys, StaticCast<FuncTy*>(le.funcBody->DataTy())->retTy,
                {tgtTy->isC, tgtTy->isClosureTy, tgtTy->hasVariableLenArg})};
            le.funcBody->SetTy(fnTy.With(target.Mode()));
            le.SetTy(le.funcBody->GetTy());
            chkSucceeded = true;
        } else if (IsLambdaIncompatible(le, !paramsMatched)) {
            // User omitted parameter's type. We should quit early.
            ds.ReportDiag();
            return false;
        }
    }
    if (chkSucceeded) {
        CheckFuncLikeLocalCaptures(ctx, le);
        return le.GetTy().IsCorrect();
    }
    // In the LSP that uses macros, the ast before the macro expansion needs to save the complete ty information to
    // prevent 'Synthesize' skipping child nodes in the lambda body due to cache.
    if (!ci->invocation.globalOptions.enableMacroInLSP) {
        ClearLambdaBodyForReCheck(le);
    }
    le.funcBody->retType.reset(nullptr);
    auto bodyTy = Synthesize({ctx, SynPos::EXPR_ARG}, le.funcBody.get());
    if (Ty::IsTyCorrect(bodyTy) && !typeManager.IsSubtype(bodyTy, targetTy)) {
        DiagMismatchedTypesWithFoundTy(diag, le, targetTy, bodyTy);
    }
    le.funcBody->SetTy({TypeManager::GetInvalidTy()});
    le.SetTy(le.funcBody->GetTy());
    return false;
}

bool TypeChecker::TypeCheckerImpl::ChkLamParamTys(
    ASTContext& ctx, LambdaExpr& le, const std::vector<AST::ModalTy>& tgtParamTys, std::vector<ModalTy>& lamParamTys)
{
    CJC_ASSERT(le.funcBody && !le.funcBody->paramLists.empty());
    if (le.funcBody->paramLists[0]->params.size() != tgtParamTys.size()) {
        auto& paramList = le.funcBody->paramLists[0];
        auto builder = diag.DiagnoseRefactor(DiagKindRefactor::sema_param_miss_match, *paramList);
        builder.AddMainHintArguments(std::to_string(tgtParamTys.size()), std::to_string(paramList->params.size()));
        return false;
    }
    size_t i = 0;
    for (auto& node : le.funcBody->paramLists[0]->params) {
        CJC_NULLPTR_CHECK(node);
        ModalTy paramTy = tgtParamTys[i]; // Caller guarantees the 'paramTy' is valid.
        if (!Check(ctx, paramTy, node.get())) {
            le.SetTy({TypeManager::GetInvalidTy()});
            return false;
        }
        // Lambda's parameter type not support auto box.
        // NOTE: lambda should not report parameter type mismatch, only report lambda type mismatchs.
        if (!typeManager.IsSubtype(paramTy, node->GetTy(), false, false)) {
            le.SetTy({TypeManager::GetInvalidTy()});
            return false;
        }
        lamParamTys.push_back(node->GetTy());
        i++;
    }
    return true;
}

bool TypeChecker::TypeCheckerImpl::ChkLamBody(ASTContext& ctx, FuncBody& lamFb)
{
    if (CheckFuncBody(ctx, lamFb) && lamFb.GetTy().IsCorrect() && lamFb.body->GetTy().IsCorrect()) {
        return true;
    }
    // Since the return type of lambda body is added in 'ChkLamExpr', all type mismatching errors are reported before.
    lamFb.SetTy({TypeManager::GetInvalidTy()});
    return false;
}
