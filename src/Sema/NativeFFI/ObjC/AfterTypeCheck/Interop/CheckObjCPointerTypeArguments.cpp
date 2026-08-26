// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements checks of types used with ObjCPointer
 */

#include "NativeFFI/Utils.h"
#include "NativeFFI/ObjC/Utils/Common.h"
#include "cangjie/AST/Walker.h"
#include "Handlers.h"

using namespace Cangjie;
using namespace Cangjie::AST;
using namespace Cangjie::Interop::ObjC;

namespace {

void ReportObjCIncompatibleTypeUsage(const InteropContext& ctx, Ptr<Type> typeUsage)
{
    CJC_NULLPTR_CHECK(typeUsage);
    Ptr<Node> errorRef = typeUsage;
    auto typeArgs = typeUsage->GetTypeArgs();
    if (typeArgs.size() > 0) {
        errorRef = typeArgs.front();
        CJC_NULLPTR_CHECK(errorRef);
    }
    ctx.diag.DiagnoseRefactor(DiagKindRefactor::sema_objc_pointer_argument_must_be_objc_compatible, *errorRef);
    typeUsage->EnableAttr(Attribute::IS_BROKEN);
}

void ReportObjCIncompatibleConstructorCall(const InteropContext& ctx, Ptr<CallExpr> callExpr)
{
    CJC_NULLPTR_CHECK(callExpr);
    Ptr<Expr> refExpr = callExpr->baseFunc;
    CJC_NULLPTR_CHECK(refExpr);
    Ptr<Node> errorRef = refExpr;
    auto typeArgs = refExpr->GetTypeArgs();
    // ObjCPointer<T>(x) ==> report error on "T"
    if (typeArgs.size() > 0) {
        errorRef = typeArgs.front();
        CJC_NULLPTR_CHECK(errorRef);
    }
    // else report error on the type name
    ctx.diag.DiagnoseRefactor(DiagKindRefactor::sema_objc_pointer_argument_must_be_objc_compatible, *errorRef);
    callExpr->EnableAttr(Attribute::IS_BROKEN);
}

} // namespace

void CheckObjCPointerTypeArguments::HandleImpl(InteropContext& ctx)
{
    for (auto& file : ctx.pkg.files) {
        Walker(file, Walker::GetNextWalkerID(), [&file, &ctx, this](auto node) {
            if (!node->IsSamePackage(*file->curPackage)) {
                return VisitAction::WALK_CHILDREN;
            }
            if (Ptr<Type> typeUsage = As<ASTKind::TYPE>(node)) {
                CheckTypeUsage(ctx, *typeUsage);
            }
            if (Ptr<CallExpr> constructorCall = As<ASTKind::CALL_EXPR>(node)) {
                CheckConstructorCall(ctx, *constructorCall);
            }
            return VisitAction::WALK_CHILDREN;
        }).Walk();
    }
}


void CheckObjCPointerTypeArguments::CheckTypeUsage(InteropContext& ctx, Type& typeUsage)
{
    if (typeUsage.TestAttr(Attribute::COMPILER_ADD)) {
        return;
    }
    auto ty = typeUsage.GetTy();
    CJC_NULLPTR_CHECK(ty);
    if (ty->typeArgs.size() != 1 || !ctx.typeMapper.IsObjCPointer(*ty)) {
        return;
    }
    auto tyArg = ty->typeArgs.front();
    CJC_NULLPTR_CHECK(tyArg);
    if (ctx.typeMapper.IsObjCCompatible(*tyArg)) {
        // everything is fine
        return;
    }
    ReportObjCIncompatibleTypeUsage(ctx, &typeUsage);
}

void CheckObjCPointerTypeArguments::CheckConstructorCall(InteropContext& ctx, CallExpr& call)
{
    if (call.TestAttr(Attribute::COMPILER_ADD)) {
        return;
    }
    if (call.callKind != CallKind::CALL_OBJECT_CREATION && call.callKind != CallKind::CALL_STRUCT_CREATION) {
        return;
    }
    if (call.resolvedFunction == nullptr) {
        return;
    }
    if (!call.resolvedFunction->TestAttr(Attribute::CONSTRUCTOR)) {
        return;
    }
    auto ty = call.GetTy();
    CJC_NULLPTR_CHECK(ty);
    if (ty->typeArgs.size() != 1 || !ctx.typeMapper.IsObjCPointer(*ty)) {
        return;
    }
    auto tyArg = ty->typeArgs.front();
    CJC_NULLPTR_CHECK(tyArg);
    if (ctx.typeMapper.IsObjCCompatible(*tyArg)) {
        // everything is fine
        return;
    }
    ReportObjCIncompatibleConstructorCall(ctx, &call);
}
