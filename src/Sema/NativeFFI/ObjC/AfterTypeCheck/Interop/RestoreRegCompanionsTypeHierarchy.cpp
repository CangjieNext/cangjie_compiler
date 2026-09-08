// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements restoring type hierarchy for all registry companions in the package.
 */

#include "Context.h"
#include "Handlers.h"
#include "NativeFFI/ObjC/Utils/ASTQuery.h"

using namespace Cangjie::AST;
using namespace Cangjie::Interop::ObjC;

namespace {

void ReplaceSuperClassDecl(ClassDecl& target, ClassDecl& newSuperClass) noexcept
{
    for (auto& it : target.inheritedTypes) {
        if (it->GetTy() && it->GetTy()->kind == TypeKind::TYPE_CLASS) {
            it = CreateRefType(newSuperClass);
            return;
        }
    }
}

} // namespace

void RestoreRegCompanionsTypeHierarchy::HandleImpl(InteropContext& ctx)
{
    for (auto& impl : ctx.impls) {
        if (impl->TestAttr(Attribute::IS_BROKEN)) {
            continue;
        }
        auto implParent = GetObjCImplSuperClass(*impl);
        if (!implParent) {
            continue;
        }
        auto parentRegCompanionDecl = ctx.GetRegCompanion(*implParent);
        auto regCompanionDecl = ctx.GetRegCompanion(*impl);
        // A same-package parent marked `IS_BROKEN` gets no companion in `MapImplToRegCompanion`, and an imported
        // one may be missing from the package it was loaded from. Neither leaves anything to restore, so skip the
        // subtype instead of desugaring it against a companion that does not exist.
        if (!parentRegCompanionDecl || !regCompanionDecl) {
            continue;
        }

        ReplaceSuperClassDecl(*regCompanionDecl, *parentRegCompanionDecl);
        parentRegCompanionDecl->subDecls.insert(regCompanionDecl);
    }
}
