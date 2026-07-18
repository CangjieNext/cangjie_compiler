// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "DesugarJavaImplSuperMethodCall.h"
#include "Utils.h"
#include "InteropLibBridge.h"

#include "cangjie/AST/Match.h"
#include "cangjie/AST/Node.h"
#include "cangjie/AST/Types.h"
#include "cangjie/AST/Walker.h"
#include "cangjie/Utils/CheckUtils.h"
#include "cangjie/Utils/SafePointer.h"
#include <utility>

namespace Cangjie::Native::FFI::Java {
using namespace Cangjie::Interop::Java;

void DesugarJavaImplSuperMethodCall::DesugarSuperMethodCall(
    CallExpr& call, ClassDecl& impl) const
{
    CJC_ASSERT(call.baseFunc && call.baseFunc->astKind == ASTKind::MEMBER_ACCESS);
    auto& ma = *StaticAs<ASTKind::MEMBER_ACCESS>(call.baseFunc.get());
    CJC_ASSERT(ma.baseExpr && ma.baseExpr->astKind == ASTKind::REF_EXPR);
    auto& ref = *StaticAs<ASTKind::REF_EXPR>(ma.baseExpr.get().get());
    CJC_ASSERT(ref.isSuper);
    CJC_ASSERT(call.resolvedFunction && call.resolvedFunction->outerDecl);
    auto& outerDecl = *call.resolvedFunction->outerDecl;
    CJC_ASSERT(IsMirror(outerDecl));
    auto parent = As<ASTKind::CLASS_DECL>(&outerDecl);
    auto curFile = call.curFile;

    std::vector<OwnedPtr<Expr>> jniArgs;

    for (auto& arg : call.args) {
        auto argExpr = ASTCloner::Clone(arg->expr.get());
        CJC_NULLPTR_CHECK(argExpr);
        auto jvalueExpr = ilib.CreateJValueExpr(std::move(argExpr), curFile);
        jniArgs.emplace_back(std::move(jvalueExpr));
    }

    OwnedPtr<VarDecl> argsVar = nullptr;
    OwnedPtr<Expr> argsRef = nullptr;

    if (!jniArgs.empty()) {
        argsVar = ilib.CreateJNIArgsVar(std::move(jniArgs), *curFile);
        argsRef = CreateRefExpr(*argsVar);
        CopyBasicInfo(argsVar.get(), argsRef.get());
    }

    auto env = ilib.CreateGetJniEnvCall(curFile);
    auto javaRef = CreateJavaRefCall(impl, curFile);
    MemberJNISignature signature(utils, *call.resolvedFunction, parent);
    auto desugaredCall = ilib.CreateJNIMethodCall(std::move(env), std::move(javaRef), signature,
        std::move(argsRef), call.GetTy(), *curFile, false);
    auto resultExpr = ilib.ConvertJNIResult(std::move(desugaredCall), call.GetTy(), impl);

    if (call.args.empty()) {
        call.desugarExpr = std::move(resultExpr);
        return;
    }

    std::vector<OwnedPtr<Node>> nodes;
    nodes.emplace_back(std::move(argsVar));
    nodes.emplace_back(std::move(resultExpr));
    auto block = CreateBlock(std::move(nodes), call.GetTy());
    call.desugarExpr = std::move(block);
}

DesugarJavaImplSuperMethodCall::DesugarJavaImplSuperMethodCall(
    InteropLibBridge& ilib, Native::FFI::Java::Utils& utils) : ilib(ilib), utils(utils)
{}

void DesugarJavaImplSuperMethodCall::Process(AfterTypeCheckContext& ctx)
{
    for (auto& jimpl : ctx.GetJavaImplReferenceWrappers()) {
        Walker(jimpl, [this, &jimpl](auto node) {
            if (node->TestAttr(Attribute::IS_BROKEN) || node->astKind == ASTKind::PRIMARY_CTOR_DECL) {
                return VisitAction::SKIP_CHILDREN;
            }
            auto call = As<ASTKind::CALL_EXPR>(node);
            if (!call) {
                return VisitAction::WALK_CHILDREN;
            }
            auto ma = As<ASTKind::MEMBER_ACCESS>(call->baseFunc.get());
            if (!ma || !ma->baseExpr) {
                return VisitAction::WALK_CHILDREN;
            }
            auto ref = As<ASTKind::REF_EXPR>(ma->baseExpr);
            if (!ref || !ref->isSuper) {
                return VisitAction::WALK_CHILDREN;
            }

            DesugarSuperMethodCall(*call, *StaticAs<ASTKind::CLASS_DECL>(jimpl));
            return VisitAction::WALK_CHILDREN;
        }).Walk();
    }
}

} // namespace Cangjie::Native::FFI::Java
