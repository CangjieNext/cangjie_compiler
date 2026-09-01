// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements mapping @ObjCImpl classes to their corresponding registry companions
 */

#include "Handlers.h"

using namespace Cangjie::AST;
using namespace Cangjie::Interop::ObjC;

namespace {

Ptr<ClassDecl> GetRegistryCompanionDecl(InteropContext& ctx, const ClassDecl& impl)
{
    // Linear search should be just fine here, as there are not so many impls per package.
    for (auto& regComp : ctx.regCompanions) {
        if (ctx.nameGenerator.GenerateRegistryCompanionName(impl) == regComp->identifier) {
            return regComp;
        }
    }

    return nullptr;
}

} // namespace

void MapImplToRegCompanion::HandleImpl(InteropContext& ctx)
{
    ctx.implToRegCompanion.reserve(ctx.impls.size());
    for (auto& impl : ctx.impls) {
        if (impl->TestAttr(Attribute::IS_BROKEN)) {
            continue;
        }
        auto regCompanion = GetRegistryCompanionDecl(ctx, *impl);
        CJC_NULLPTR_CHECK(regCompanion);

        ctx.implToRegCompanion[impl] = regCompanion;
    }
}
