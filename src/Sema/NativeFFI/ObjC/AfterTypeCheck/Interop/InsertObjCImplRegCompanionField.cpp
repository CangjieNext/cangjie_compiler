// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements inserting `private var $reg: Impl$reg` field for each @ObjCImpl declaration.
 */

#include "Handlers.h"
#include "cangjie/AST/Create.h"
#include "cangjie/AST/Node.h"

namespace Cangjie::Interop::ObjC {

using namespace Cangjie::AST;

void InsertObjCImplRegCompanionField::HandleImpl(InteropContext& ctx)
{
    for (auto& impl : ctx.impls) {
        if (impl->TestAttr(Attribute::IS_BROKEN)) {
            continue;
        }

        auto regCompanion = ctx.implToRegCompanion.at(impl);
        // Transform function will set the identifier
        auto regField = CreateVarDecl("");
        ctx.astTransformer.TransformToObjCImplRegCompanionField(*regField, *regCompanion);
        ctx.astInserter.InsertInto(*impl, std::move(regField));
    }
}

} // namespace Cangjie::Interop::ObjC
