// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements check that @ObjCMirror annotated declaration inherits an @ObjCMirror declaration or none of
 * them.
 */

#include "Handlers.h"
#include "NativeFFI/ObjC/Utils/ASTQuery.h"

namespace Cangjie::Interop::ObjC {

using namespace Cangjie::AST;

namespace {

/**
 * @returns true if @p ty is an @ObjCMirror whose whole super-type hierarchy consists of @ObjCMirror types.
 */
bool IsValidObjCMirror(const Ty& ty) noexcept
{
    const auto classLikeTy = DynamicCast<ClassLikeTy*>(&ty);
    if (!classLikeTy || !classLikeTy->commonDecl || !IsObjCMirror(*classLikeTy->commonDecl)) {
        return false;
    }

    for (auto superInterfaceTy : classLikeTy->GetSuperInterfaceTys()) {
        if (!IsValidObjCMirror(*superInterfaceTy)) {
            return false;
        }
    }

    // The super class of an @ObjCMirror class must be an @ObjCMirror as well.
    if (const auto classTy = DynamicCast<ClassTy*>(&ty)) {
        // Hierarchy root @ObjCMirror class.
        if (!classTy->GetSuperClassTy() || classTy->GetSuperClassTy()->IsObject()) {
            return true;
        }
        return IsValidObjCMirror(*classTy->GetSuperClassTy());
    }

    return true;
}

} // namespace

void CheckMirrorInheritMirror::HandleImpl(TypeCheckContext& ctx)
{
    if (IsValidObjCMirror(*ctx.target.GetTy())) {
        return;
    }

    ctx.diag.DiagnoseRefactor(DiagKindRefactor::sema_objc_mirror_must_inherit_mirror, ctx.target);
    ctx.target.EnableAttr(Attribute::IS_BROKEN);
}

} // namespace Cangjie::Interop::ObjC
