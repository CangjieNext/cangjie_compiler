// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements generating init Cangjie object method for @ObjCImpls.
 */

#include "Handlers.h"
#include "NativeFFI/ObjC/Utils/ASTQuery.h"

using namespace Cangjie::AST;
using namespace Cangjie::Interop::ObjC;

void GenerateInitCJObjectMethods::HandleImpl(InteropContext& ctx)
{
    for (auto& impl : ctx.impls) {
        if (impl->TestAttr(Attribute::IS_BROKEN)) {
            continue;
        }
        for (auto& memberDecl : impl->GetMemberDecls()) {
            if (memberDecl->TestAttr(Attribute::IS_BROKEN)) {
                continue;
            }

            if (!memberDecl->TestAttr(Attribute::PUBLIC, Attribute::CONSTRUCTOR)) {
                continue;
            }

            auto ctorDecl = As<ASTKind::FUNC_DECL>(memberDecl.get());
            if (!ctorDecl) {
                continue;
            }

            if (!IsObjCImplRegDataCtor(*ctorDecl)) {
                continue;
            }

            auto initCjObject = ctx.factory.CreateInitCjObjectReturningObjCSelf(
                *impl, *ctx.implToRegCompanion[impl], *ctorDecl);
            CJC_ASSERT(initCjObject);
            ctx.genDecls.push_back(std::move(initCjObject));
        }
    }
}
