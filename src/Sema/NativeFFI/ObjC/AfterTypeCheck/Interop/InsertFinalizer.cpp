// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements generating and inserting a finalizer for each @ObjCMirror and @ObjCImpl class.
 */

#include "Handlers.h"

namespace Cangjie::Interop::ObjC {
using namespace Cangjie::AST;
using namespace Cangjie::Native::FFI;

namespace {
void CreateAndInsertFinalizer(ASTFactory& factory, ClassDecl& cd)
{
    auto finalizer = factory.CreateFinalizer(cd);
    cd.body->decls.push_back(std::move(finalizer));
}
} // namespace

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

        CreateAndInsertFinalizer(ctx.factory, *mirrorClass);
    }

    for (auto& wrapper : ctx.mirrorInterfaceHandleWrappers) {
        CreateAndInsertFinalizer(ctx.factory, *wrapper);
    }

    for (auto& impl : ctx.impls) {
        if (impl->TestAttr(Attribute::IS_BROKEN)) {
            continue;
        }

        CreateAndInsertFinalizer(ctx.factory, *impl);
    }
}
} // namespace Cangjie::Interop::ObjC
