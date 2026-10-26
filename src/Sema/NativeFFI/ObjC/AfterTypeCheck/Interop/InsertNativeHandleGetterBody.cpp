// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements inserting a body of `$getObj(): NativeObjCId` for each @ObjCMirror/@ObjCImpl
 */

#include "Handlers.h"
#include "NativeFFI/ObjC/Utils/ASTQuery.h"
#include "NativeFFI/Utils.h"
#include "cangjie/AST/Create.h"

namespace Cangjie::Interop::ObjC {
using namespace Cangjie::AST;
using namespace Cangjie::Native::FFI;

void InsertNativeHandleGetterBody::HandleImpl(InteropContext& ctx)
{
    for (auto& mirror : ctx.mirrors) {
        if (mirror->TestAttr(Attribute::IS_BROKEN)) {
            continue;
        }

        auto mirrorClass = As<ASTKind::CLASS_DECL>(mirror);
        if (!mirrorClass) {
            continue;
        }

        if (HasObjCMirrorSuperClass(*mirrorClass)) {
            continue;
        }

        auto getterDecl = GetNativeHandleGetter(*mirrorClass);
        auto nativeHandleFieldExpr = ctx.factory.CreateNativeHandleFieldExpr(*mirrorClass);
        getterDecl->funcBody->body->body.emplace_back(std::move(nativeHandleFieldExpr));
    }

    for (auto& wrapper : ctx.mirrorInterfaceHandleWrappers) {
        auto getterDecl = GetNativeHandleGetter(*wrapper);
        auto nativeHandleFieldExpr = ctx.factory.CreateNativeHandleFieldExpr(*wrapper);
        getterDecl->funcBody->body->body.emplace_back(std::move(nativeHandleFieldExpr));
    }
}

} // namespace Cangjie::Interop::ObjC
