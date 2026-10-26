// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements generating and inserting a constructor declaration of native handle
 */

#include "Handlers.h"
#include "NativeFFI/Utils.h"
#include "cangjie/AST/Create.h"

namespace Cangjie::Interop::ObjC {
using namespace Cangjie::AST;
using namespace Cangjie::Native::FFI;

void InsertBaseCtorDecl::HandleImpl(InteropContext& ctx)
{
    for (auto& mirror : ctx.mirrors) {
        if (mirror->TestAttr(Attribute::IS_BROKEN)) {
            continue;
        }

        auto mirrorClass = As<ASTKind::CLASS_DECL>(mirror);
        if (!mirrorClass) {
            continue;
        }

        auto ctor = ctx.factory.CreateObjCMirrorBaseCtorDecl(*mirrorClass);
        mirrorClass->body->decls.emplace_back(std::move(ctor));
    }

    for (auto& wrapper : ctx.mirrorInterfaceHandleWrappers) {
        auto ctor = ctx.factory.CreateBaseCtorDecl(*wrapper);
        wrapper->body->decls.emplace_back(std::move(ctor));
    }

    for (auto& impl : ctx.impls) {
        if (impl->TestAttr(Attribute::IS_BROKEN)) {
            continue;
        }

        auto paramLists = Nodes<FuncParamList>(CreateFuncParamList(std::vector<OwnedPtr<FuncParam>>{}));
        // Transform function will set the identifier
        auto ctor = CreateFuncDecl("", CreateFuncBody(std::move(paramLists), nullptr, CreateBlock({})));
        ctx.astTransformer.TransformToObjCImplBaseCtorDecl(*ctor, *impl);
        ctx.astInserter.InsertInto(*impl, std::move(ctor));
    }

    for (auto& regComp : ctx.regCompanions) {
        // no need to check if broken, because class is just generated
        auto ctor = ctx.factory.CreateBaseCtorDecl(*regComp);
        ctx.astInserter.InsertInto(*regComp, std::move(ctor));
    }
}
} // namespace Cangjie::Interop::ObjC
