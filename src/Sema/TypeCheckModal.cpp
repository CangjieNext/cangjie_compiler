// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "Diags.h"
#include "NodeContext.h"
#include "ScopeManager.h"
#include "TypeCheckUtil.h"
#include "TypeCheckerImpl.h"
#include "cangjie/AST/ASTContext.h"
#include "cangjie/AST/Create.h"
#include "cangjie/AST/Node.h"
#include "cangjie/AST/Utils.h"
#include "cangjie/AST/Walker.h"
#include "cangjie/Basic/DiagnosticEngine.h"
#include "cangjie/Sema/TypeManager.h"

#include <algorithm>
#include <unordered_set>

namespace Cangjie {
using namespace AST;

static FuncBody* GetFuncBody(const Node& funcLike)
{
    if (auto fd = DynamicCast<FuncDecl>(&funcLike)) {
        return fd->funcBody.get();
    }
    if (auto le = DynamicCast<LambdaExpr>(&funcLike)) {
        return le->funcBody.get();
    }
    if (auto md = DynamicCast<MacroDecl>(&funcLike)) {
        return md->desugarDecl->funcBody.get();
    }
    if (Is<PrimaryCtorDecl>(&funcLike)) {
        // desugared, should not have body
        return nullptr;
    }
    return StaticCast<MainDecl>(&funcLike)->funcBody.get();
}

/// Track function context with default param value state
struct FuncContext {
    Node* func = nullptr;
    bool inDefaultParamValue = false;
};

/// All checks:
/// 1. Check return expression of a function body cannot have internal @local! type
/// 2. Check global var cannot have local type
/// 3. Check call expr args cannot be external @local! type if the param is @local! nor Copy
/// 4. Check validity of @MakeCopy
/// 5. Check local of captured variables
/// 6. Check manually implements Copy
/// 7. Check assignment/member-assignment
/// 8. Check exclave expr is inside a function, or global or static var initializer (not counting default param values
/// as inside that function)
/// 9. Check exclave expr is not in static init, finalizer, main, or spawn expr
/// 10. Check instance&static member var is of data type
/// 11. Check instance member var does not have initializer when there are mixed local type init
/// 12. Check the whole body of local?/local! constructor must be in exclave expr
/// 13. Check finalizer cannot have non ~local this param
/// 14. Check type arguments of a type are data type (function type allowed); skip for function type and fun call
struct ModalTypeChecker {
    ModalTypeChecker(DiagnosticEngine& diag, TypeManager& m) : d(diag), type{m}
    {
    }

    void Check(const ASTContext& ctx, Package& pkg)
    {
        Walker walker(
            &pkg,
            [this, &ctx](Node* node) -> VisitAction {
                if (node->astKind == ASTKind::PACKAGE || node->astKind == ASTKind::FILE) {
                    return VisitAction::WALK_CHILDREN;
                }
                if (node->TestAnyAttr(Attribute::IS_BROKEN, Attribute::HAS_BROKEN)) {
                    return VisitAction::SKIP_CHILDREN;
                }
                if (auto decl = DynamicCast<InheritableDecl>(node)) {
                    CheckImplementsCopy(*decl);
                }
                if (auto decl = DynamicCast<StructDecl>(node)) {
                    if (decl->IsCopyType()) {
                        CheckCopyType(*decl);
                    }
                    CheckMemberVarModality(*decl);
                    if (HasMixedLocalTypeInit(*decl)) {
                        CheckMemberVarInitalizer(*decl);
                    }
                }
                if (auto decl = DynamicCast<ClassDecl>(node)) {
                    if (HasMixedLocalTypeInit(*decl)) {
                        CheckMemberVarInitalizer(*decl);
                    }
                    CheckMemberVarModality(*decl);
                }
                if (auto var = DynamicCast<VarDecl>(node)) {
                    if (!Ty::IsTyCorrect(var->GetTy())) {
                        return VisitAction::SKIP_CHILDREN;
                    }
                    CheckGlobalVarModalType(*var);
                }
                if (auto call = DynamicCast<CallExpr>(node)) {
                    if (!Ty::IsTyCorrect(call->GetTy())) {
                        return VisitAction::SKIP_CHILDREN;
                    }
                    CheckCallExpr(ctx, *call);
                }
                if (auto assign = DynamicCast<AssignExpr>(node)) {
                    CheckAssignExpr(ctx, *assign);
                }
                if (node->IsFuncLike()) {
                    if (!Ty::IsTyCorrect(node->GetTy())) {
                        return VisitAction::SKIP_CHILDREN;
                    }
                    CheckReturnModalType(ctx, *node);
                    if (Is<FuncDecl>(node) || Is<LambdaExpr>(node)) {
                        CheckCaptures(ctx, *node);
                    }
                    // Push function context
                    funcStack.push_back({node, false});
                }
                // Track entering default param value
                if (auto fp = DynamicCast<FuncParam>(node); fp && fp->assignment && !funcStack.empty()) {
                    funcStack.back().inDefaultParamValue = true;
                }
                // Track entering spawn block (only if not already inside one)
                if (Is<SpawnExpr>(node) && currentSpawnExpr == nullptr) {
                    currentSpawnExpr = node;
                }
                // Track entering global/static var initializer (only if not already inside one)
                if (auto var = DynamicCast<VarDecl>(node);
                    var && var->initializer && IsGlobalOrMember(*var) && currentGlobalStaticVar == nullptr) {
                    currentGlobalStaticVar = node;
                }
                // Check exclave expr
                if (auto exclave = DynamicCast<ExclaveExpr>(node)) {
                    CheckExclaveInsideFunction(*exclave);
                    CheckExclaveInCtor(ctx, *exclave);
                }
                if (auto lambda = DynamicCast<LambdaExpr>(node)) {
                    CheckNeedsRegion(*lambda);
                }
                if (auto func = DynamicCast<FuncDecl>(node)) {
                    CheckNeedsRegion(*func);
                    CheckWholeBodyIsExclave(*func);
                    if (func->IsFinalizer()) {
                        CheckFinalizerThisParam(*func);
                    }
                }
                return VisitAction::WALK_CHILDREN;
            },
            [this](Node* node) -> VisitAction {
                // Pop function context when exiting
                if (node->IsFuncLike() && !funcStack.empty() && funcStack.back().func == node) {
                    funcStack.pop_back();
                }
                // Track exiting default param value
                if (auto fp = DynamicCast<FuncParam>(node); fp && fp->assignment && !funcStack.empty()) {
                    funcStack.back().inDefaultParamValue = false;
                }
                // Track exiting spawn block
                if (node == currentSpawnExpr) {
                    currentSpawnExpr = nullptr;
                }
                // Track exiting global/static var initializer
                if (node == currentGlobalStaticVar) {
                    currentGlobalStaticVar = nullptr;
                }
                return VisitAction::WALK_CHILDREN;
            });
        walker.Walk();
    }

    /// 1. external local! used in exclave expr is internal
    /// 2. internal local! used in returned expr of inside exclave expr is external
    /// 3. member access of external local! is external
    /// 4. reference to param of `T` local! is external local!
    /// 5. reference to global/static var is external
    /// 6. all other cases are internal
    bool IsExternalLocal(const ASTContext& ctx, const Expr& expr)
    {
        if (IsInExclaveExpr(ctx, expr)) {
            // rule 2
            return IsReturnedExpr(ctx, expr);
        }
        if (auto ref = DynamicCast<RefExpr>(&expr)) {
            if (auto param = DynamicCast<FuncParam>(ref->GetTarget())) {
                auto node = ScopeManager::GetCurSymbolByKind(SymbolKind::FUNC_LIKE, ctx, expr.scopeName);
                if (!node || !node->node) {
                    return false;
                }
                auto func = DynamicCast<FuncDecl>(node->node);
                if (!func) {
                    return false;
                }
                for (auto& fp : func->funcBody->paramLists[0]->params) {
                    if (fp.get() == param) {
                        // rule 4
                        return true;
                    }
                }
            }
            if (auto var = DynamicCast<VarDecl>(ref->GetTarget())) {
                if (IsGlobalOrStaticVar(*var)) {
                    // rule 5
                    return true;
                }
            }
        }
        if (auto ma = DynamicCast<MemberAccess>(&expr)) {
            if (auto maTarget = ma->GetTarget(); maTarget && IsGlobalOrStaticVar(*maTarget)) {
                // rule 5
                return true;
            }
            // rule 3
            return IsExternalLocal(ctx, *ma->baseExpr);
        }
        return false;
    }

private:
    /// Functions that return internal @local! or @local? must return so via exclave. External @local! or Copy is ok.
    void CheckReturnModalType(const ASTContext& ctx, const Node& func)
    {
        auto body = GetFuncBody(func);
        if (!body || !body->retType || !Ty::IsTyCorrect(body->retType->GetTy())) {
            return;
        }
        auto retLocal = body->retType->GetTy()->Modal().local;
        if (retLocal == LocalModal::NOT) {
            return;
        }
        auto r = CollectReturnedExpr(*body);
        for (auto& e : r) {
            if (!IsExternalLocal(ctx, *e) && type.NeverImplementsCopyInterface(e->GetTy()) &&
                !IsInExclaveExpr(ctx, *e)) {
                DiagBadInternalLocalReturn(*e);
            }
        }
    }

    /// Check that exclave is inside a function body (not counting default param values as inside that function), global
    /// or static var initializer
    void CheckExclaveInsideFunction(const ExclaveExpr& expr)
    {
        // Exclave is allowed inside global/static var initializer
        if (currentGlobalStaticVar) {
            return;
        }
        // Find the first function where we're not in its default param value
        Node* enclosingFunc = nullptr;
        for (auto it = funcStack.rbegin(); it != funcStack.rend(); ++it) {
            if (!it->inDefaultParamValue) {
                enclosingFunc = it->func;
                break;
            }
        }
        if (!enclosingFunc) {
            DiagExclaveOutsideFunc(expr);
        }
    }

    /// Check exclave expr is not in static init, finalizer, main, or spawn expr (in ctor it is allowed)
    void CheckExclaveInCtor(const ASTContext& ctx, const ExclaveExpr& expr)
    {
        // check whether in spawn
        if (currentSpawnExpr) {
            DiagExclaveInMain(expr, *currentSpawnExpr);
            return;
        }
        // check whether in constructor, main, or finalizer
        if (auto ctorSym = ScopeManager::GetCurSatisfiedSymbolUntilTopLevel(ctx, expr.scopeName, [](Symbol& sym) {
                if (auto func = DynamicCast<FuncDecl>(sym.node)) {
                    return func->TestAttr(Attribute::MAIN_ENTRY) || func->IsFinalizer() ||
                        (func->TestAttr(Attribute::CONSTRUCTOR) && func->TestAttr(Attribute::STATIC));
                };
                return false;
            })) {
            DiagExclaveInMain(expr, *ctorSym->node);
        }
    }

    void DiagExclaveInMain(const ExclaveExpr& expr, const Node& node)
    {
        auto name = ASTKIND_TO_STR.at(node.astKind);
        if (auto func = DynamicCast<FuncDecl>(&node)) {
            if (func->IsFinalizer()) {
                name = "finalizer";
            } else if (func->TestAttr(Attribute::MAIN_ENTRY)) {
                name = "main";
            } else if (func->TestAttr(Attribute::STATIC)) {
                name = "static init";
            }
        }
        d.DiagnoseRefactor(DiagKindRefactor::sema_exclave_in_main, expr, name);
    }

    void DiagExclaveOutsideFunc(const ExclaveExpr& expr)
    {
        d.DiagnoseRefactor(DiagKindRefactor::sema_exclave_outside_function, expr);
    }

    /// Check the whole body of local?/local! constructor must be in exclave expr.
    void CheckWholeBodyIsExclave(const FuncDecl& init)
    {
        if (!init.TestAttr(Attribute::CONSTRUCTOR) ||
            init.TestAnyAttr(Attribute::STATIC, Attribute::IS_BROKEN, Attribute::HAS_BROKEN)) {
            return;
        }
        if (TypeCheckUtil::HasModifier(init.modifiers, TokenKind::EXCLAVE)) {
            return;
        }
        auto& body = init.funcBody->body;
        auto initTy = type.GetThisParamTy(init);
        if (!body || !Ty::IsTyCorrect(initTy) || initTy->IsDataType()) {
            return;
        }
        if (body->body.empty()) {
            DiagInitModalMustBeInExclave(*body, initTy);
        } else {
            if (body->body[0]->astKind != ASTKind::EXCLAVE_EXPR) {
                DiagInitModalMustBeInExclave(*body->body[0], initTy);
            }
        }
    }

    /// When the init has expr, use the first expr pos; otherwise use the body pos.
    void DiagInitModalMustBeInExclave(const Node& pos, Ptr<Ty> ty)
    {
        d.DiagnoseRefactor(DiagKindRefactor::sema_init_modal_must_be_in_exclave, pos, ty->Modal().ToString());
    }

    /// The following functions need mark needsRegion:
    /// The function has in its body a func call that returns a non copy non @~local type (including constructor call
    /// and enum constructor call)
    /// specifically when the whole function body is an exclave, don't generate region; otherwise the new region will
    /// be immediately closed, so this is an optimization.
    void CheckNeedsRegion(const Node& body, bool& needsRegion)
    {
        if (!Ty::IsTyCorrect(body.GetTy())) {
            return;
        }
        ConstWalker w{&body, [this, &needsRegion](Ptr<const Node> node) {
            if (Is<ExclaveExpr>(node)) {
                return VisitAction::SKIP_CHILDREN;
            }
            if (auto ref = DynamicCast<NameReferenceExpr>(node)) {
                if (auto target = ref->GetTarget(); target &&
                    target->TestAttr(Attribute::ENUM_CONSTRUCTOR) &&
                    !type.ImplementsCopyInterface(ref->GetTy()) && !ref->GetTy()->IsDataType()) {
                    needsRegion = true;
                    return VisitAction::SKIP_CHILDREN;
                }
            }
            if (auto call = DynamicCast<CallExpr>(node)) {
                if (!call->baseFunc || !Ty::IsTyCorrect(call->GetTy())) {
                    // invalid call node, skip
                    return VisitAction::SKIP_CHILDREN;
                }
                if (call->GetTy()->IsNothing()) {
                    // nothing call itself does not require a region, but exprs before the call may
                    // require a region
                    return VisitAction::WALK_CHILDREN;
                }
                Ptr<Ty> targetTy = nullptr;
                if (call->callKind == CallKind::CALL_FUNCTION_PTR) {
                    // fp call, no target
                    if (auto funcTy = DynamicCast<FuncTy>(call->baseFunc->GetTy())) {
                        targetTy = funcTy->retTy;
                    } else {
                        targetTy = call->baseFunc->GetTy();
                    }
                    // could be a generic ty with funcTy upperbound
                } else if (call->callKind == CallKind::CALL_OBJECT_CREATION ||
                    call->callKind == CallKind::CALL_STRUCT_CREATION) {
                    targetTy = call->GetTy(); // do not use retTy because only targetTy has the correct
                                            // modal type, retTy is of data type
                    if (auto funcTy = DynamicCast<FuncTy>(targetTy)) {
                        targetTy = funcTy->retTy;
                    }
                } else {
                    targetTy = call->GetTy();
                }
                CJC_NULLPTR_CHECK(targetTy);

                // non copy non @~local type, needs a region
                if ((targetTy->IsLocalType() && type.NeverImplementsCopyInterface(targetTy)) ||
                    (!type.ImplementsCopyInterface(targetTy) && !targetTy->IsDataType())) {
                    needsRegion = true;
                    return VisitAction::STOP_NOW;
                }
            }
            if (Is<FuncDecl>(node) || Is<LambdaExpr>(node)) {
                // skip nested func
                return VisitAction::SKIP_CHILDREN;
            }
            return VisitAction::WALK_CHILDREN;
        }};
        w.Walk();
    }

    void CheckNeedsRegion(LambdaExpr& lambda)
    {
        CheckNeedsRegion(*lambda.funcBody, lambda.needsRegion);
    }

    void CheckNeedsRegion(FuncDecl& func)
    {
        if (TypeCheckUtil::HasModifier(func.modifiers, TokenKind::EXCLAVE)) {
            func.needsRegion = false;
            return;
        }
        // constructor uses its caller's region implicitly
        if (func.TestAttr(Attribute::CONSTRUCTOR)) {
            if (auto& thisParam = func.funcBody->paramLists[0]->thisParam) {
                if (thisParam->modal.ToModalInfo() != LocalModal::NOT) {
                    func.needsRegion = false;
                    return;
                }
            }
            return;
        }
        if (!func.funcBody->body) {
            return;
        }
        CheckNeedsRegion(*func.funcBody, func.needsRegion);
    }

    /// Local of lvalue: for RefExpr the referenced var's local, or (implicit this) the ref target's receiver (this)
    /// type; for MemberAccess the referenced object's local (base expr type).
    LocalModal GetLvalueLocal(const ASTContext& ctx, const Expr* left)
    {
        if (auto ref = DynamicCast<RefExpr>(left)) {
            auto target = ref->GetTarget();
            if (target && target->IsMemberDecl()) {
                // Implicit this access: use receiver (this param) type, same as GetThisArgTy.
                if (auto fn =
                        ScopeManager::GetCurSatisfiedSymbolUntilTopLevel(ctx, ref->scopeName, [](const Symbol& sym) {
                            if (auto func = DynamicCast<FuncDecl>(sym.node)) {
                                return Is<InheritableDecl>(func->outerDecl);
                            }
                            return false;
                        })) {
                    auto fd = StaticCast<FuncDecl>(fn->node);
                    if (fd->TestAttr(Attribute::STATIC)) {
                        return target->GetTy()->Modal().local;
                    }
                    Ptr<Ty> thisTy = type.GetThisParamTy(*fd);
                    if (thisTy && Ty::IsTyCorrect(thisTy)) {
                        return thisTy->IsLocalType() ? thisTy->Modal().local : LocalModal::NOT;
                    }
                }
            }
            return left->GetTy()->IsLocalType() ? left->GetTy()->Modal().local : LocalModal::NOT;
        }
        if (auto ma = DynamicCast<MemberAccess>(left)) {
            return ma->baseExpr->GetTy()->IsLocalType() ? ma->baseExpr->GetTy()->Modal().local : LocalModal::NOT;
        }
        CJC_ABORT();
        return {};
    }

    /// Whether the assignment is inside an @local? constructor or any finalizer.
    bool IsInLocalQuestCtorOrAnyFinalizer(const ASTContext& ctx, const std::string& scopeName)
    {
        auto sym = ScopeManager::GetCurSatisfiedSymbolUntilTopLevel(ctx, scopeName, [](Symbol& s) {
            if (auto func = DynamicCast<FuncDecl>(s.node)) {
                return func->TestAttr(Attribute::CONSTRUCTOR) || func->IsFinalizer();
            }
            return false;
        });
        if (!sym) {
            return false;
        }
        auto func = DynamicCast<FuncDecl>(sym->node);
        if (!func || func->IsFinalizer()) {
            return true;
        }
        if (!func->funcBody->paramLists.empty()) {
            if (auto& thisParam = func->funcBody->paramLists[0]->thisParam) {
                return thisParam->GetTy()->Modal().local == LocalModal::HALF;
            }
        }
        return false;
    }

    void CheckAssignExprRef(
        const ASTContext& ctx, const AssignExpr& assign, const Expr* left, LocalModal lLocal, LocalModal rLocal)
    {
        // When llocal is ~, rlocal must be ~
        if (lLocal == LocalModal::NOT) {
            if (rLocal != LocalModal::NOT) {
                DiagBadAssignment(assign, {}, assign.rightExpr->GetTy());
            }
            return;
        }
        // 1. @local? (HALF): member assignment (this/implicit this) only in @local? ctor or finalizer; whole var not
        // restricted
        if (lLocal == LocalModal::HALF) {
            auto ref = StaticCast<RefExpr>(left);
            auto target = ref->GetTarget();
            if (!target || !target->IsMemberDecl()) {
                return; // whole var assignment: rule does not apply
            }
            if (IsInLocalQuestCtorOrAnyFinalizer(ctx, assign.scopeName)) {
                return;
            }
            auto diag = DiagBadAssignment(assign, left->GetTy()->Modal(), left->GetTy());
            diag.AddNote("This assignment is only allowed in @local? constructor or finalizer.");
            return;
        }
        // 2. @local! (FULL): capture
        if (IsCapture(ctx, *left)) {
            if (lLocal != LocalModal::FULL && rLocal == LocalModal::NOT) {
                return;
            }
            DiagBadAssignment(assign, ModalInfo{lLocal}, assign.rightExpr->GetTy());
            return;
        }
        // 3. @local! (FULL): both FULL, external vs internal
        if (rLocal == LocalModal::FULL) {
            auto lext = IsExternalLocal(ctx, *left);
            auto rext = IsExternalLocal(ctx, *assign.rightExpr.get());
            if (lext != rext) {
                DiagBadAssignment(assign, ModalInfo{LocalModal::FULL}, assign.rightExpr->GetTy(), lext, rext);
            }
        } else {
            // 4. @local! (FULL): else
            DiagBadAssignment(assign, ModalInfo{lLocal}, assign.rightExpr->GetTy());
        }
    }

    void CheckAssignExprMa(
        const ASTContext& ctx, const AssignExpr& assign, const Expr* left, LocalModal lLocal, LocalModal rLocal)
    {
        // When llocal is ~, rlocal must be ~
        if (lLocal == LocalModal::NOT) {
            if (rLocal != LocalModal::NOT) {
                DiagBadAssignment(assign, {}, assign.rightExpr->GetTy());
            }
            return;
        }
        // @local? (HALF): ma is allowed; allow only in @local? ctor or finalizer
        if (lLocal == LocalModal::HALF) {
            if (IsInLocalQuestCtorOrAnyFinalizer(ctx, assign.scopeName)) {
                return;
            }
            auto diag = DiagBadAssignment(assign, left->GetTy()->Modal(), left->GetTy());
            diag.AddNote("This assignment is only allowed in @local? constructor or finalizer.");
            return;
        }
        // lLocal == FULL
        if (rLocal == LocalModal::FULL) {
            auto lext = IsExternalLocal(ctx, *left);
            auto rext = IsExternalLocal(ctx, *assign.rightExpr.get());
            if (lext != rext) {
                DiagBadAssignment(assign, ModalInfo{LocalModal::FULL}, assign.rightExpr->GetTy(), lext, rext);
            }
        } else {
            DiagBadAssignment(assign, ModalInfo{lLocal}, assign.rightExpr->GetTy());
        }
    }

    void CheckAssignExpr(const ASTContext& ctx, const AssignExpr& assign)
    {
        if (assign.desugarExpr) {
            return;
        }
        auto right = assign.rightExpr.get();
        if (!Ty::IsTyCorrect(right->GetTy())) {
            return;
        }
        if (type.ImplementsCopyInterface(right->GetTy())) {
            return;
        }
        auto rLocal = right->GetTy()->IsLocalType() ? right->GetTy()->Modal().local : LocalModal::NOT;
        auto left = assign.leftValue.get();
        if (!Ty::IsTyCorrect(left->GetTy()) || Is<WildcardExpr>(left)) {
            return;
        }
        auto lLocal = GetLvalueLocal(ctx, left);
        if (DynamicCast<RefExpr>(left)) {
            CheckAssignExprRef(ctx, assign, left, lLocal, rLocal);
        } else if (DynamicCast<MemberAccess>(left)) {
            CheckAssignExprMa(ctx, assign, left, lLocal, rLocal);
        }
    }

    bool IsCapture(const ASTContext& ctx, const Expr& expr)
    {
        auto ref = DynamicCast<RefExpr>(&expr);
        if (!ref) {
            return false;
        }
        auto target = ref->GetTarget();
        if (target->TestAnyAttr(Attribute::GLOBAL, Attribute::STATIC)) {
            return false;
        }
        auto defSite = ScopeManager::GetCurSymbolByKind(SymbolKind::FUNC_LIKE, ctx, target->scopeName);
        auto useSite = ScopeManager::GetCurSymbolByKind(SymbolKind::FUNC_LIKE, ctx, expr.scopeName);
        if (!defSite || !useSite) {
            return false;
        }
        return defSite->node != useSite->node;
    }

    DiagnosticBuilder DiagBadAssignment(const Node& position, const ModalInfo& lModal, const Ty* rTy)
    {
        std::string arg1 = lModal.local != LocalModal::NOT ? " " + lModal.LocalString() : "";
        std::string arg2 = rTy->String();
        return d.DiagnoseRefactor(DiagKindRefactor::sema_bad_local_assignment, position, arg1, arg2);
    }

    DiagnosticBuilder DiagBadAssignment(
        const Node& position, const ModalInfo& lModal, const Ty* rTy, bool lExternal, bool rExternal)
    {
        std::string arg1 = (lExternal ? " external " : " internal ") + lModal.LocalString();
        std::string arg2 = (rExternal ? " external " : " internal ") + rTy->String();
        return d.DiagnoseRefactor(DiagKindRefactor::sema_bad_local_assignment, position, arg1, arg2);
    }

    void CheckImplementsCopy(const InheritableDecl& decl)
    {
        for (auto& parent : decl.inheritedTypes) {
            if (!Ty::IsTyCorrect(parent->GetTy())) {
                continue;
            }
            if (type.IsCopyInterfaceTy(parent->GetTy())) {
                DiagImplementsCopy(*parent);
            }
        }
    }

    void DiagImplementsCopy(const Node& position)
    {
        d.Diagnose(position, DiagKind::sema_interface_is_not_implementable, std::string{COPY_NAME});
    }

    void CheckFinalizerThisParam(const FuncDecl& func)
    {
        if (func.funcBody->paramLists.empty()) {
            return;
        }
        auto& thisParam = func.funcBody->paramLists[0]->thisParam;
        if (!thisParam) {
            return;
        }
        if (!thisParam->GetTy()->IsDataType()) {
            DiagExpectedDataType(func, thisParam->GetTy()->String());
        }
    }

    /// Any non-static member var with modal type (local type) is not allowed
    void CheckMemberVarModality(const InheritableDecl& decl)
    {
        for (auto member : decl.GetMemberDeclPtrs()) {
            if (auto var = DynamicCast<VarDecl>(member)) {
                CheckMemberVarModalType(*var);
            }
        }
    }

    /// When type has mixed local type init, the member var of non-copy type cannot have initializer
    void CheckMemberVarInitalizer(const InheritableDecl& decl)
    {
        for (auto member : decl.GetMemberDeclPtrs()) {
            if (auto var = DynamicCast<VarDecl>(member)) {
                if (!Ty::IsTyCorrect(var->GetTy()) || var->TestAttr(Attribute::STATIC)) {
                    continue;
                }
                if (var->initializer && !type.ImplementsCopyInterface(var->GetTy())) {
                    DiagMemberVarInitalizerInMixedInit(*var, decl);
                }
            }
        }
    }

    void DiagMemberVarInitalizerInMixedInit(const VarDecl& var, const InheritableDecl& decl)
    {
        d.DiagnoseRefactor(DiagKindRefactor::sema_member_var_initalizer_in_mixed_init, var, var.identifier.Val(), var.GetTy()->String(),
            ASTKIND_TO_STR.at(decl.astKind));
    }

    bool HasMixedLocalTypeInit(const InheritableDecl& decl)
    {
        int lastUsedLocal{-1};
        for (auto member : decl.GetMemberDeclPtrs()) {
            if (member->TestAttr(Attribute::CONSTRUCTOR) && !member->TestAttr(Attribute::STATIC) &&
                !Is<PrimaryCtorDecl>(member)) {
                // primary ctor is already desugared
                auto& init = *StaticCast<FuncDecl>(member)->funcBody->paramLists[0];
                auto thisLocal =
                    static_cast<int>(init.thisParam ? init.thisParam->GetTy()->Modal().local : LocalModal::NOT);
                if (lastUsedLocal == -1) {
                    lastUsedLocal = thisLocal;
                } else if (lastUsedLocal != thisLocal) {
                    return true;
                }
            }
        }
        return false;
    }

    void CheckMemberVarModalType(const VarDecl& var)
    {
        if (!Ty::IsTyCorrect(var.GetTy()) || var.TestAttr(Attribute::STATIC)) {
            return;
        }
        // member var with modal type is not allowed
        if (var.GetTy()->IsLocalType()) {
            DiagMemberVarLocalModalType(var);
        }
    }

    void DiagMemberVarLocalModalType(const VarDecl& var)
    {
        // Use written type's position if available, otherwise use variable location
        if (var.type) {
            DiagExpectedDataType(*var.type, var.GetTy()->String());
        } else {
            DiagExpectedDataType(var, var.GetTy()->String());
        }
    }

    /// Check global var cannot have local type
    void CheckGlobalVarModalType(const VarDecl& var)
    {
        if (var.TestAnyAttr(Attribute::GLOBAL, Attribute::STATIC)) {
            if (Ty::IsTyCorrect(var.GetTy()) && var.GetTy()->IsLocalType()) {
                DiagGlobalVarLocalModalType(var);
            }
        }
    }

    void CheckCapture(const Node& func, const RefExpr& expr)
    {
        auto funcLocal = func.GetTy()->Modal().local;
        if (auto target = expr.GetTarget()) {
            if (type.ImplementsCopyInterface(target->GetTy())) {
                return;
            }
            auto varLocal = target->GetTy()->Modal().local;
            // these are the only allowed cases
            if (funcLocal == LocalModal::NOT && varLocal == LocalModal::NOT) {
                return;
            }
            if (funcLocal == LocalModal::HALF && varLocal != LocalModal::FULL) {
                return;
            }
            DiagBadCapture(func, expr);
        }
    }

    void CheckCaptures(const ASTContext& ctx, const Node& func)
    {
        auto body = GetFuncBody(func);
        Walker walker(body, [this, &ctx, &func](Node* node) {
            if (auto re = DynamicCast<RefExpr>(node)) {
                if (re->isThis || re->isSuper) {
                    if (auto localf = DynamicCast<FuncDecl>(&func); localf && Is<InheritableDecl>(localf->outerDecl)) {
                        return VisitAction::WALK_CHILDREN;
                    }
                    CheckCapture(func, *re);
                    return VisitAction::SKIP_CHILDREN;
                }
                if (auto target = DynamicCast<VarDecl>(re->GetTarget());
                    target && !target->TestAnyAttr(Attribute::STATIC, Attribute::GLOBAL)) {
                    auto targetDefSite =
                        ScopeManager::GetCurSymbolByKind(SymbolKind::FUNC_LIKE, ctx, target->scopeName);
                    if (!targetDefSite || !targetDefSite->node) {
                        return VisitAction::WALK_CHILDREN;
                    }
                    if (targetDefSite->node == &func) {
                        return VisitAction::WALK_CHILDREN;
                    }
                    CheckCapture(func, *re);
                }
            }
            return VisitAction::WALK_CHILDREN;
        });
        walker.Walk();
    }

    void DiagBadCapture(const Node& func, const NameReferenceExpr& capture)
    {
        auto db = d.DiagnoseRefactor(DiagKindRefactor::sema_bad_capture_local, capture, capture.GetTy()->Modal().LocalString(),
            capture.GetTarget()->identifier.Val(), func.GetTy()->Modal().LocalString(),
            func.astKind == ASTKind::FUNC_DECL ? "function" : "lambda");
    }

    static bool IsNonStaticMemberFunction(const FuncDecl& func)
    {
        if (func.ownerFunc || func.TestAnyAttr(Attribute::CONSTRUCTOR, Attribute::ENUM_CONSTRUCTOR)) {
            return false;
        }
        // fix later: check prop
        return Is<InheritableDecl>(func.outerDecl) && !func.TestAttr(Attribute::STATIC);
    }

    const Expr* GetFuncArg(const CallExpr& call, size_t index)
    {
        if (auto inner = DynamicCast<CallExpr>(call.desugarExpr.get())) {
            return GetFuncArg(*inner, index);
        }
        if (auto array = DynamicCast<ArrayExpr>(call.desugarExpr.get())) {
            return array->args[index]->expr.get();
        }
        auto func = call.resolvedFunction;
        if (func && IsNonStaticMemberFunction(*func)) {
            // non static member function call, the first arg is this
            if (auto ma = DynamicCast<MemberAccess>(&*call.baseFunc)) {
                if (index == 0) {
                    return ma->baseExpr.get();
                }
                if (call.desugarArgs) {
                    return call.desugarArgs->at(index - 1)->expr.get();
                }
                return call.args[index - 1]->expr.get();
            }
            // RefExpr, using implicit this, no need to check
            return nullptr;
        }
        if (call.desugarArgs.has_value()) {
            return call.desugarArgs.value()[index]->expr.get();
        }
        return call.args[index]->expr.get();
    }

    struct FuncParamInfo {
        Ptr<Ty> ty;
        ModalInfo modal;
        std::string_view name;
    };
    static FuncParamInfo GetFuncParam(const CallExpr& call, size_t index)
    {
        if (auto inner = DynamicCast<CallExpr>(call.desugarExpr.get())) {
            return GetFuncParam(*inner, index);
        }
        if (auto array = DynamicCast<ArrayExpr>(call.desugarExpr.get())) {
            return {array->args[index]->GetTy(), array->args[index]->GetTy()->Modal(), ""};
        }
        auto func = call.resolvedFunction;
        if (!func) {
            // function pointer call, no need to check 'this' param, no arg name
            return {call.args[index]->GetTy(), call.args[index]->GetTy()->Modal(), ""};
        }
        if (call.baseFunc->GetTy()->IsPointer()) {
            return {call.args[index]->GetTy(), call.args[index]->GetTy()->Modal(), ""};
        }
        if (func->funcBody->paramLists[0]->thisParam) {
            if (index == 0) {
                return {func->funcBody->paramLists[0]->thisParam->GetTy(),
                    func->funcBody->paramLists[0]->thisParam->modal, "this"};
            }
            return {func->funcBody->paramLists[0]->params[index - 1]->GetTy(),
                func->funcBody->paramLists[0]->params[index - 1]->GetTy()->Modal(),
                func->funcBody->paramLists[0]->params[index - 1]->identifier.Val()};
        }
        if (IsNonStaticMemberFunction(*func)) {
            if (index == 0) {
                // fix later: get this type with modal
                return {func->outerDecl->GetTy(), {}, "this"};
            }
            return {func->funcBody->paramLists[0]->params[index - 1]->GetTy(),
                func->funcBody->paramLists[0]->params[index - 1]->GetTy()->Modal(),
                func->funcBody->paramLists[0]->params[index - 1]->identifier.Val()};
        }
        return {func->funcBody->paramLists[0]->params[index]->GetTy(),
            func->funcBody->paramLists[0]->params[index]->GetTy()->Modal(),
            func->funcBody->paramLists[0]->params[index]->identifier.Val()};
    }

    size_t GetParamNum(const CallExpr& call)
    {
        if (auto inner = DynamicCast<CallExpr>(call.desugarExpr.get())) {
            return GetParamNum(*inner);
        }
        auto func = call.resolvedFunction;
        if (!func) {
            if (auto array = DynamicCast<ArrayExpr>(call.desugarExpr.get())) {
                return array->args.size();
            }
            if (call.baseFunc->GetTy()->IsPointer()) {
                return call.args.size();
            }
            if (call.baseFunc->GetTy()->kind == TypeKind::TYPE_CSTRING) {
                return 1UL;
            }
            // function pointer call, no need to check 'this' param
            if (auto fty = DynamicCast<FuncTy>(call.baseFunc->GetTy())) {
                return fty->paramTys.size();
            }
            // invalid
            return -1UL;
        }
        if (IsNonStaticMemberFunction(*func)) {
            return func->funcBody->paramLists[0]->params.size() + 1;
        }
        return func->funcBody->paramLists[0]->params.size();
    }

    std::unordered_map<FuncBody*, std::vector<const Expr*>> returnedExprMap;
    bool IsReturnedExpr(const ASTContext& ctx, const Expr& expr)
    {
        auto node = ScopeManager::GetCurSymbolByKind(SymbolKind::FUNC_LIKE, ctx, expr.scopeName);
        if (!node || !node->node) {
            return false;
        }
        auto body = GetFuncBody(*node->node);
        if (!body) {
            return false;
        }

        auto cache = returnedExprMap.find(body);
        auto& rets =
            cache != returnedExprMap.end() ? cache->second : (returnedExprMap[body] = CollectReturnedExpr(*body));
        return std::find(rets.begin(), rets.end(), &expr) != rets.end();
    }

    std::vector<const Expr*> CollectReturnedExpr(FuncBody& body)
    {
        if (!body.body || body.body->body.empty()) {
            return {};
        }
        std::vector<const Expr*> ret;
        Walker(&body, [&ret](auto n) {
            if (auto re = DynamicCast<ReturnExpr>(n)) {
                ret.push_back(re->expr.get());
                return VisitAction::SKIP_CHILDREN;
            }
            if (n->IsFuncLike()) {
                return VisitAction::SKIP_CHILDREN;
            }
            return VisitAction::WALK_CHILDREN;
        }).Walk();
        auto lastExpr = DynamicCast<Expr>(body.body->body.back().get());
        while (lastExpr && lastExpr->desugarExpr) {
            lastExpr = lastExpr->desugarExpr.get();
        }
        if (lastExpr->astKind == ASTKind::RETURN_EXPR) {
            return ret;
        }

        if (auto exclave = DynamicCast<ExclaveExpr>(lastExpr); exclave && !exclave->body->body.empty()) {
            lastExpr = DynamicCast<Expr>(exclave->body->body.back().get());
            while (lastExpr && lastExpr->desugarExpr) {
                // push last expr of exclave
                lastExpr = lastExpr->desugarExpr.get();
            }
        }
        if (auto funcTy = DynamicCast<FuncTy>(body.GetTy())) {
            if (funcTy->retTy->IsUnit() || (body.funcDecl && body.funcDecl->TestAttr(Attribute::CONSTRUCTOR))) {
                // do not collect last expr if it is return type is Unit
                return ret;
            }
        }
        // or push the last expr
        ret.push_back(lastExpr);
        return ret;
    }

    bool IsInExclaveExpr(const ASTContext& ctx, const Node& node)
    {
        auto sym = ScopeManager::GetCurSatisfiedSymbolUntilTopLevel(
            ctx, node.scopeName, [](Symbol& sym) { return sym.node->astKind == ASTKind::EXCLAVE_EXPR; });
        if (sym && sym->node) {
            return true;
        }
        if (auto fun = ScopeManager::GetCurSymbolByKind(SymbolKind::FUNC_LIKE, ctx, node.scopeName)) {
            if (auto fd = DynamicCast<FuncDecl>(fun->node)) {
                if (std::any_of(fd->modifiers.begin(), fd->modifiers.end(),
                        [](const Modifier& m) { return m.modifier == TokenKind::EXCLAVE; })) {
                    return true;
                }
            }
        }
        return false;
    }

    void DiagBadExternalLocalArg(const FuncParamInfo& param, const Expr& arg)
    {
        d.DiagnoseRefactor(DiagKindRefactor::sema_bad_external_local_arg, arg, arg.GetTy()->Modal().ToString(),
            std::string{param.name});
    }

    void DiagBadInternalLocalReturn(const Expr& expr)
    {
        d.DiagnoseRefactor(DiagKindRefactor::sema_bad_internal_local_return, expr);
    }

    /// Check call expr args cannot be external @local! type if the param is @local! nor Copy
    void CheckCallExpr(const ASTContext& ctx, const CallExpr& call)
    {
        size_t paramNum = GetParamNum(call);
        if (paramNum == -1UL) {
            // invalid call node, skip
            return;
        }
        // ctor is always considered exclave call, so we don't check arg pass
        if (call.callKind == CallKind::CALL_OBJECT_CREATION || call.callKind == CallKind::CALL_STRUCT_CREATION) {
            return;
        }
        for (size_t i = 0; i < paramNum; ++i) {
            auto arg = GetFuncArg(call, i);
            if (!arg) {
                continue;
            }
            auto param = GetFuncParam(call, i);
            if (param.modal.local == LocalModal::FULL) {
                if (IsExternalLocal(ctx, *arg) && !type.ImplementsCopyInterface(arg->GetTy())) {
                    DiagBadExternalLocalArg(param, *arg);
                }
            }
        }
    }

    void DiagGlobalVarLocalModalType(const VarDecl& var)
    {
        // Use written type's position if available, otherwise use variable location
        if (var.type) {
            DiagExpectedDataType(*var.type, var.GetTy()->String());
        } else {
            DiagExpectedDataType(var, var.GetTy()->String());
        }
    }

    void DiagExpectedDataType(const Node& node, const std::string& typeStr)
    {
        d.DiagnoseRefactor(DiagKindRefactor::sema_expected_data_type, node, typeStr);
    }

    /// Check validity of @MakeCopy
    void CheckCopyType(StructDecl& decl)
    {
        for (auto member : decl.GetMemberDeclPtrs()) {
            if (auto var = DynamicCast<VarDecl>(member)) {
                if (type.ImplementsCopyInterface(var->GetTy())) {
                    continue;
                }
                DiagCopyStructBadField(decl, *var);
            }
        }
    }

    void DiagCopyStructBadField(const StructDecl& decl, const VarDecl& var)
    {
        d.DiagnoseRefactor(DiagKindRefactor::sema_copy_struct_bad_field, var, var.identifier.Val(), decl.identifier.Val());
    }

    DiagnosticEngine& d;
    TypeManager& type;
    std::vector<FuncContext> funcStack;
    Node* currentSpawnExpr = nullptr;       // Cache for tracking if we're inside a spawn block
    Node* currentGlobalStaticVar = nullptr; // Cache for tracking if we're inside a global/static var initializer
};

ModalTypeChecker* TypeChecker::TypeCheckerImpl::NewModalTypeChecker()
{
    return new ModalTypeChecker(diag, typeManager);
}

void TypeChecker::TypeCheckerImpl::DeleteModalTypeChecker()
{
    delete modalTypeChecker;
}

bool TypeChecker::TypeCheckerImpl::IsExternalLocal(const ASTContext& ctx, const Expr& expr)
{
    return modalTypeChecker->IsExternalLocal(ctx, expr);
}

void TypeChecker::TypeCheckerImpl::CheckModalType(const ASTContext& ctx, Package& pkg)
{
    modalTypeChecker->Check(ctx, pkg);
}

void TypeChecker::TypeCheckerImpl::ExpectSubtypeOf(
    Ptr<AST::Node> node, Ptr<AST::Ty> expect, Ptr<AST::Ty> actual, ModalMatchMode modal)
{
    if (!typeManager.IsSubtype(expect, actual, true, true, modal)) {
        Sema::DiagMismatchedTypesWithFoundTy(diag, *node, *expect, *actual);
        node->SetTy(TypeManager::GetInvalidTy());
    }
}

using namespace Sema;
bool TypeChecker::TypeCheckerImpl::ChkExclaveExpr(ASTContext& ctx, Ty& target, ExclaveExpr& expr)
{
    auto type = SynExclaveExpr(ctx, expr);
    if (!Ty::IsTyCorrect(type)) {
        return false;
    }
    // syn exclave either returns invalid, or returns Nothing, so subtype always holds
    // but in our design, InvalidTy is not a subtype of anything, so we still need to check
    if (!typeManager.IsSubtype(type, &target)) {
        DiagMismatchedTypes(diag, expr, target);
        expr.SetTy(TypeManager::GetInvalidTy());
        return false;
    }
    return true;
}

void TypeChecker::TypeCheckerImpl::DiagNestedExclave(const ExclaveExpr& expr, const AST::Node& outerNode)
{
    auto db = diag.DiagnoseRefactor(DiagKindRefactor::sema_nested_exclave, expr);
    db.AddHint(MakeRange(outerNode.begin, outerNode.end));
}

void TypeChecker::TypeCheckerImpl::DiagExpectedDataType(const AST::Node& node)
{
    diag.DiagnoseRefactor(DiagKindRefactor::sema_expected_data_type, node, node.GetTy()->String());
}

DiagnosticBuilder TypeChecker::TypeCheckerImpl::DiagUndeclaredIdentifier(Range range, const std::string& identifier)
{
    return diag.DiagnoseRefactor(DiagKindRefactor::sema_undeclared_identifier, range, identifier);
}

Ptr<Ty> TypeChecker::TypeCheckerImpl::SynExclaveExpr(ASTContext& ctx, ExclaveExpr& expr)
{
    // DiagExclaveOutsideFunc is only reported in ModalCheckExclaveInsideFunction.

    // Check for nested exclave (prefer outer exclave expr over exclave-modified function as outer)
    if (auto outerExclave = ScopeManager::GetCurSatisfiedSymbolUntilTopLevel(ctx, expr.scopeName,
            [e = &expr](Symbol& sym) { return sym.node != e && sym.node->astKind == ASTKind::EXCLAVE_EXPR; })) {
        DiagNestedExclave(expr, *outerExclave->node);
        // but we can still synthesize the type, so do not return invalid ty here
    } else if (auto outFun = ScopeManager::GetOutMostSymbol(ctx, SymbolKind::FUNC_LIKE, expr.scopeName)) {
        if (auto fd = DynamicCast<FuncDecl>(outFun->node)) {
            if (fd->IsMemberDecl() && TypeCheckUtil::HasModifier(fd->modifiers, TokenKind::EXCLAVE)) {
                DiagNestedExclave(expr, *fd);
            }
        }
    }

    // Target from global/static var initializer (use var's type for last expr / exclave type)
    VarDecl* gv = topLevelVar.get();
    if (gv && gv->initializer) {
        if (!Ty::IsTyCorrect(gv->GetTy()) || gv->GetTy()->kind == TypeKind::TYPE_QUEST) {
            SynBlock({ctx, SynPos::EXPR_ARG}, *expr.body);
            expr.SetTy(TypeManager::GetNothingTy());
            return expr.GetTy();
        }
        if (!ChkBlock(ctx, *gv->GetTy(), *expr.body)) {
            expr.SetTy(TypeManager::GetInvalidTy());
            return expr.GetTy();
        }
        expr.SetTy(TypeManager::GetNothingTy());
        return expr.GetTy();
    }

    // Target from enclosing function return type
    auto fun = ScopeManager::GetCurSymbolByKind(SymbolKind::FUNC_LIKE, ctx, expr.scopeName);
    if (fun && fun->node) {
        auto fb = GetFuncBody(*fun->node);
        if (auto funcBodyTy = DynamicCast<FuncTy>(fb->GetTy())) {
            auto target = funcBodyTy->retTy;
            if (target->kind == TypeKind::TYPE_QUEST) {
                SynBlock({ctx, SynPos::EXPR_ARG}, *expr.body);
                expr.SetTy(TypeManager::GetNothingTy());
                return expr.GetTy();
            }
            if (auto func = DynamicCast<FuncDecl>(fun->node)) {
                if (func->TestAttr(Attribute::CONSTRUCTOR) || func->IsFinalizer()) {
                    SynBlock({ctx, SynPos::EXPR_ARG}, *expr.body);
                    expr.SetTy(TypeManager::GetNothingTy());
                    return expr.GetTy();
                }
            }
            if (!ChkBlock(ctx, *target, *expr.body)) {
                expr.SetTy(TypeManager::GetInvalidTy());
                return expr.GetTy();
            }
            expr.SetTy(TypeManager::GetNothingTy());
            return expr.GetTy();
        }
    }

    // No target: do not check exclave return type; any type inside body is correct
    SynBlock({ctx, SynPos::EXPR_ARG}, *expr.body);
    expr.SetTy(TypeManager::GetNothingTy());
    return expr.GetTy();
}

void TypeChecker::TypeCheckerImpl::CheckHasLocalMod(const AST::Expr& node, AST::LocalModal local)
{
    if (!Ty::IsTyCorrect(node.GetTy())) {
        return;
    }
    if (node.GetTy()->Modal().local != local) {
        DiagLocalModNotSatisfied(node, local);
    }
}

void TypeChecker::TypeCheckerImpl::DiagLocalModNotSatisfied(const AST::Expr& node, AST::LocalModal local)
{
    diag.DiagnoseRefactor(DiagKindRefactor::sema_local_modal_not_satisfied, node, std::string{ToString(local)}, node.GetTy()->String());
}
} // namespace Cangjie
