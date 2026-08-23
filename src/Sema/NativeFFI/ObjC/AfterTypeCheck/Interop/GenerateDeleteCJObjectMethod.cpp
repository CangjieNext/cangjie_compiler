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
#include "NativeFFI/ObjC/Utils/Common.h"
#include "cangjie/AST/Match.h"

using namespace Cangjie::AST;
using namespace Cangjie::Interop::ObjC;

void GenerateDeleteCJObjectMethod::HandleImpl(InteropContext& ctx)
{
    auto genNativeDeleteMethod = [&ctx](Decl& decl) {
        if (decl.TestAttr(Attribute::IS_BROKEN)) {
            return;
        }
        auto deleteCjObject = ctx.factory.CreateDeleteCjObject(decl);
        CJC_ASSERT(deleteCjObject);
        ctx.genDecls.emplace_back(std::move(deleteCjObject));
    };

    for (auto& impl : ctx.impls) {
        // generate only for root @ObjCImpl classes
        if (HasImplSuperClass(*impl)) {
            continue;
        }
        genNativeDeleteMethod(*impl);
    }
}
