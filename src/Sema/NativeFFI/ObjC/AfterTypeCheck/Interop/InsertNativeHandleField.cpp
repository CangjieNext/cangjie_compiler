// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements inserting NativeObjCId field in Objective-C mirrors
 */

#include "NativeFFI/ObjC/Utils/ASTQuery.h"
#include "Handlers.h"

using namespace Cangjie::AST;
using namespace Cangjie::Interop::ObjC;

void InsertNativeHandleField::HandleImpl(InteropContext& ctx)
{
    for (auto& mirror : ctx.mirrors) {
        if (mirror->TestAttr(Attribute::IS_BROKEN)) {
            continue;
        }
        auto mirrorClass = As<ASTKind::CLASS_DECL>(mirror);
        if (!mirrorClass || HasObjCMirrorSuperClass(*mirrorClass)) {
            continue;
        }

        auto nativeObjCIdField = ctx.factory.CreateNativeHandleField(*mirrorClass);
        mirrorClass->body->decls.push_back(std::move(nativeObjCIdField));
    }

    for (auto& wrapper : ctx.mirrorInterfaceHandleWrappers) {
        auto nativeObjCIdField = ctx.factory.CreateNativeHandleField(*wrapper);
        wrapper->body->decls.push_back(std::move(nativeObjCIdField));
    }
}
