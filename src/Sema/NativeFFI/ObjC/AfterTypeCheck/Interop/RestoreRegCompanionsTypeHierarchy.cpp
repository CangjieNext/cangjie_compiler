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

Ptr<ClassDecl> GetRegistryCompanionClass(InteropContext& ctx, const ClassDecl& impl) noexcept
{
    return ctx.importManager.GetImportedDecl<ClassDecl>(
        impl.fullPackageName, ctx.nameGenerator.GenerateRegistryCompanionName(impl));
}

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
        Ptr<ClassDecl> parentRegCompanionDecl;
        if (implParent->IsSamePackage(*impl)) {
            parentRegCompanionDecl = ctx.implToRegCompanion[implParent];
        } else {
            parentRegCompanionDecl = GetRegistryCompanionClass(ctx, *implParent);
        }

        auto regCompanionDecl = ctx.implToRegCompanion[impl];
        ReplaceSuperClassDecl(*regCompanionDecl, *parentRegCompanionDecl);
        parentRegCompanionDecl->subDecls.insert(regCompanionDecl);
    }
}
