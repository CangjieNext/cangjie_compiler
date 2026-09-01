// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements inserting an accessor instance method for `$obj: NativeObjCId` field for each @ObjCMirror
 * interface declaration(according to inheritance).
 */

#include "Handlers.h"
#include "NativeFFI/ObjC/Utils/ASTQuery.h"

using namespace Cangjie::AST;
using namespace Cangjie::Interop::ObjC;

void InsertNativeHandleGetterDecl::HandleImpl(InteropContext& ctx)
{
    for (auto& mirror : ctx.mirrors) {
        if (mirror->TestAttr(Attribute::IS_BROKEN)) {
            continue;
        }

        // The getter is declared once per class hierarchy.
        auto mirrorClass = As<ASTKind::CLASS_DECL>(mirror);
        if (mirrorClass && HasObjCMirrorSuperClass(*mirrorClass)) {
            continue;
        }

        if (GetNativeHandleGetter(*mirror)) {
            continue;
        }

        auto nativeHandleGetterDecl = ctx.factory.CreateNativeHandleGetterDecl(*mirror);
        switch (mirror->astKind) {
            case ASTKind::CLASS_DECL: {
                auto cd = StaticAs<ASTKind::CLASS_DECL>(mirror);
                cd->body->decls.push_back(std::move(nativeHandleGetterDecl));
                break;
            }
            case ASTKind::INTERFACE_DECL: {
                auto id = StaticAs<ASTKind::INTERFACE_DECL>(mirror);
                id->body->decls.push_back(std::move(nativeHandleGetterDecl));
                break;
            }
            default:
                // @ObjCMirror is always either an interface or a class.
                CJC_ABORT();
        }
    }

    for (auto& wrapper : ctx.mirrorInterfaceHandleWrappers) {
        if (wrapper->TestAttr(Attribute::IS_BROKEN)) {
            continue;
        }

        auto nativeHandleGetterDecl = ctx.factory.CreateNativeHandleGetterDecl(*wrapper);
        wrapper->body->decls.push_back(std::move(nativeHandleGetterDecl));
    }
}
