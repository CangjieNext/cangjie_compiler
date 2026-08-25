// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements generating and inserting a finalizer for each @ObjCMirror class:
 */

#include "Handlers.h"
#include "NativeFFI/ObjC/Utils/ASTQuery.h"
#include "NativeFFI/Utils.h"

using namespace Cangjie::AST;
using namespace Cangjie::Interop::ObjC;
using namespace Cangjie::Native::FFI;

void InsertFinalizer::HandleImpl(InteropContext& ctx)
{
    for (auto& mirror : ctx.mirrors) {
        if (mirror->TestAttr(Attribute::IS_BROKEN)) {
            continue;
        }
        auto mirrorClass = As<ASTKind::CLASS_DECL>(mirror);
        if (!mirrorClass) {
            continue;
        }

        auto hasInitedField = ctx.deprecatedFactory.CreateHasInitedField(*mirrorClass);
        CJC_NULLPTR_CHECK(hasInitedField);
        mirrorClass->body->decls.emplace_back(std::move(hasInitedField));

        auto finalizer = ctx.deprecatedFactory.CreateFinalizer(*mirrorClass);
        CJC_NULLPTR_CHECK(finalizer);
        mirrorClass->body->decls.emplace_back(std::move(finalizer));
    }

    for (auto& wrapper : ctx.mirrorInterfaceHandleWrappers) {
        if (wrapper->TestAttr(Attribute::IS_BROKEN)) {
            continue;
        }
        auto hasInitedField = ctx.deprecatedFactory.CreateHasInitedField(*wrapper);
        CJC_NULLPTR_CHECK(hasInitedField);
        wrapper->body->decls.emplace_back(std::move(hasInitedField));

        auto finalizer = ctx.deprecatedFactory.CreateFinalizer(*wrapper);
        CJC_NULLPTR_CHECK(finalizer);
        wrapper->body->decls.emplace_back(std::move(finalizer));
    }

    for (auto& impl : ctx.impls) {
        if (impl->TestAttr(Attribute::IS_BROKEN)) {
            continue;
        }

        auto hasInitedField = ctx.deprecatedFactory.CreateHasInitedField(*impl);
        CJC_NULLPTR_CHECK(hasInitedField);
        impl->body->decls.push_back(std::move(hasInitedField));

        auto finalizer = ctx.deprecatedFactory.CreateFinalizer(*impl);
        CJC_NULLPTR_CHECK(finalizer);
        impl->body->decls.push_back(std::move(finalizer));
    }
}
