// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements marking the subtypes of a broken @ObjCMirror/@ObjCImpl broken as well.
 */

#include "Handlers.h"
#include "NativeFFI/ObjC/Utils/ASTQuery.h"
#include "cangjie/AST/Match.h"

namespace Cangjie::Interop::ObjC {

using namespace Cangjie::AST;

namespace {

bool IsObjCClass(const ClassDecl& decl) noexcept
{
    return IsObjCImpl(decl) || IsObjCMirror(decl);
}

/**
 * Whether any @ObjCMirror/@ObjCImpl ancestor of \p decl was rejected by the checks that ran before.
 * The whole chain is walked, so the result does not depend on the order the subtypes are visited in.
 */
bool HasBrokenObjCSuperClass(const ClassDecl& decl) noexcept
{
    for (auto parent = decl.GetSuperClassDecl(); parent && IsObjCClass(*parent);
         parent = parent->GetSuperClassDecl()) {
        if (parent->TestAttr(Attribute::IS_BROKEN)) {
            return true;
        }
    }

    return false;
}

void MarkIfParentIsBroken(ClassDecl& decl) noexcept
{
    // A decl the checks already rejected is skipped: it is broken either way, and it may be malformed enough
    // that the queries below have nothing to work with.
    if (decl.TestAttr(Attribute::IS_BROKEN)) {
        return;
    }
    if (HasBrokenObjCSuperClass(decl)) {
        decl.EnableAttr(Attribute::IS_BROKEN);
    }
}

} // namespace

void PropagateBrokenToSubtypes::HandleImpl(InteropContext& ctx)
{
    for (auto& mirror : ctx.mirrors) {
        // Only classes take part in a super class chain; @ObjCMirror interfaces are left alone.
        if (auto mirrorClass = As<ASTKind::CLASS_DECL>(mirror)) {
            MarkIfParentIsBroken(*mirrorClass);
        }
    }

    for (auto& impl : ctx.impls) {
        MarkIfParentIsBroken(*impl);
    }
}

} // namespace Cangjie::Interop::ObjC
