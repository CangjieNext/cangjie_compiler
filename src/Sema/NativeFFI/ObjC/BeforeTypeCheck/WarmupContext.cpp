// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements collecting the @ObjCMirror and @ObjCImpl declarations of the package into the
 * PrepareTypeCheck context, so that the subsequent handlers don't walk the package on their own.
 */

#include "Handlers.h"
#include "cangjie/AST/Match.h"

using namespace Cangjie::AST;
using namespace Cangjie::Interop::ObjC;

void WarmupContext::HandleImpl(PrepareTypeCheckContext& ctx)
{
    for (auto& file : ctx.pkg.files) {
        for (auto& decl : file->decls) {
            if (decl->TestAnyAttr(Attribute::HAS_BROKEN, Attribute::IS_BROKEN)) {
                continue;
            }

            if (auto classLikeDecl = As<ASTKind::CLASS_LIKE_DECL>(decl);
                classLikeDecl && classLikeDecl->TestAttr(Attribute::OBJ_C_MIRROR)) {
                ctx.mirrors.push_back(classLikeDecl);
            }

            if (auto classDecl = As<ASTKind::CLASS_DECL>(decl);
                classDecl && classDecl->TestAttr(Attribute::OBJ_C_IMPL)) {
                ctx.impls.push_back(classDecl);
            }
        }
    }
}
