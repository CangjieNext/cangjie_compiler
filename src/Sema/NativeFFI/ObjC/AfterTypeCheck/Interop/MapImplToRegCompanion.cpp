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

#include <string_view>
#include <unordered_map>

#include "Handlers.h"

namespace Cangjie::Interop::ObjC {

using namespace Cangjie::AST;

namespace {

using RegistryCompanionByName = std::unordered_map<std::string_view, Ptr<ClassDecl>>;

Ptr<ClassDecl> GetRegistryCompanionDecl(
    const NameGenerator& nameGenerator, const RegistryCompanionByName& regCompByName, const ClassDecl& impl)
{
    auto found = regCompByName.find(nameGenerator.GenerateRegistryCompanionName(impl));

    return found == regCompByName.end() ? nullptr : found->second;
}

} // namespace

void MapImplToRegCompanion::HandleImpl(InteropContext& ctx)
{
    RegistryCompanionByName regCompanionsByName;
    regCompanionsByName.reserve(ctx.regCompanions.size());
    for (auto& regComp : ctx.regCompanions) {
        regCompanionsByName.emplace(regComp->identifier.Val(), regComp);
    }

    ctx.implToRegCompanion.reserve(ctx.impls.size());
    for (auto& impl : ctx.impls) {
        if (impl->TestAttr(Attribute::IS_BROKEN)) {
            continue;
        }
        auto regCompanion = GetRegistryCompanionDecl(ctx.nameGenerator, regCompanionsByName, *impl);
        CJC_NULLPTR_CHECK(regCompanion);

        ctx.implToRegCompanion[impl] = regCompanion;
    }
}

} // namespace Cangjie::Interop::ObjC
