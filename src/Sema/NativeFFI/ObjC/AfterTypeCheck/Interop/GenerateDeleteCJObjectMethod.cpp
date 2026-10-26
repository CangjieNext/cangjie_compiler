// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements generating delete Cangjie object method for Objective-C mirror subtypes.
 */

#include "Handlers.h"
#include "NativeFFI/ObjC/Utils/ASTQuery.h"
#include "cangjie/AST/Match.h"

using namespace Cangjie::AST;
using namespace Cangjie::Interop::ObjC;

void GenerateDeleteCJObjectMethod::HandleImpl(InteropContext& ctx)
{
    for (auto& impl : ctx.impls) {
        if (impl->TestAttr(Attribute::IS_BROKEN)) {
            continue;
        }

        // generate only for root @ObjCImpl classes
        if (HasObjCImplSuperClass(*impl)) {
            continue;
        }
        auto deleteCjObject = ctx.factory.CreateDeleteCjObject(*impl);
        ctx.genDecls.push_back(std::move(deleteCjObject));
    }
}
