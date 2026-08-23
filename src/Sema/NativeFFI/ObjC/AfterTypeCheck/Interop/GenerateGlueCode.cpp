// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements generating Objective-C glue code.
 */

#include "NativeFFI/ObjC/ObjCCodeTranspiler/Transpiler.h"
#include "Handlers.h"

using namespace Cangjie::AST;
using namespace Cangjie::Interop::ObjC;

void GenerateGlueCode::HandleImpl(InteropContext& ctx)
{
    auto genGlueCode = [&ctx](Decl& decl) {
        if (decl.TestAnyAttr(Attribute::IS_BROKEN, Attribute::HAS_BROKEN)) {
            return;
        }
        auto codegen = Transpiler(ctx, &decl, ctx.outputObjCGenDir, ctx.cjLibOutputPath);
        codegen.Generate();
    };

    for (auto& impl : ctx.impls) {
        genGlueCode(*impl);
    }
}
