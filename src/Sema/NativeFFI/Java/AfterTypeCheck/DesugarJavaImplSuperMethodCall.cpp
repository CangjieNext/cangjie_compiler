// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "DesugarJavaImplSuperMethodCall.h"
#include "InteropLibBridge.h"

#include "NativeFFI/Java/AfterTypeCheck/ASTFactory.h"
#include "NativeFFI/Java/AfterTypeCheck/AfterTypeCheckContext.h"
#include "cangjie/AST/Match.h"
#include "cangjie/AST/Node.h"
#include "cangjie/AST/Types.h"
#include "cangjie/AST/Walker.h"
#include "cangjie/Utils/SafePointer.h"

namespace Cangjie::Native::FFI::Java {
using namespace Cangjie::Interop::Java;

void DesugarJavaImplSuperMethodCall::DesugarSuperMethodCall(
    AfterTypeCheckContext& ctx, CallExpr& call, ClassDecl& impl) const
{
    call.desugarExpr = factory.CreateSuperJavaMethodCall(ctx, impl, call, factory.ExtractArgExprs(call.args));
}

DesugarJavaImplSuperMethodCall::DesugarJavaImplSuperMethodCall(
    InteropLibBridge& ilib, Native::FFI::Java::Utils& utils, ASTFactory& factory)
    : ilib(ilib), utils(utils), factory(factory)
{}

void DesugarJavaImplSuperMethodCall::Process(AfterTypeCheckContext& ctx)
{
    for (auto& jimpl : ctx.GetJavaImplReferenceWrappers()) {
        Walker(jimpl, [this, &ctx, &jimpl](auto node) {
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

            DesugarSuperMethodCall(ctx, *call, *StaticAs<ASTKind::CLASS_DECL>(jimpl));
            return VisitAction::WALK_CHILDREN;
        }).Walk();
    }
}

} // namespace Cangjie::Native::FFI::Java
