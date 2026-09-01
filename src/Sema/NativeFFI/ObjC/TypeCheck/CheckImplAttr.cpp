// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements the check that a subtype of an Objective-C mirror MUST be annotated either with
 * @ObjCMirror or with @ObjCImpl.
 */

#include "Handlers.h"
#include "NativeFFI/ObjC/Utils/ASTQuery.h"

using namespace Cangjie::AST;
using namespace Cangjie::Interop::ObjC;

void CheckImplAttr::HandleImpl(TypeCheckContext& ctx)
{
    auto& ty = *ctx.target.GetTy();
    if (!IsObjCMirrorSubtype(ty)) {
        return;
    }

    if (IsObjCMirror(ty) || IsObjCImpl(ty)) {
        return;
    }

    ctx.diag.DiagnoseRefactor(DiagKindRefactor::sema_objc_mirror_subtype_must_be_annotated, ctx.target);
    ctx.target.EnableAttr(Attribute::IS_BROKEN);
}
