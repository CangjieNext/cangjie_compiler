// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements searching for Objective-C mirror declarations and theirs subtypes.
 */

#include "Handlers.h"
#include "NativeFFI/ObjC/Utils/ASTQuery.h"
#include "cangjie/AST/Match.h"
#include "cangjie/AST/Node.h"

using namespace Cangjie::AST;
using namespace Cangjie::Interop::ObjC;

void FindMirrors::HandleImpl(InteropContext& ctx)
{
    for (auto& file : ctx.pkg.files) {
        for (auto& decl : file->decls) {
            // @ObjCMirror interface/class
            if (auto classLikeDecl = As<ASTKind::CLASS_LIKE_DECL>(decl);
                classLikeDecl && IsObjCMirror(*classLikeDecl)) {
                ctx.mirrors.push_back(classLikeDecl);
            }

            if (auto classDecl = As<ASTKind::CLASS_DECL>(decl); classDecl) {
                // Mirror interface handle wrappers
                if (IsObjCMirrorInterfaceHandleWrapper(*classDecl)) {
                    ctx.mirrorInterfaceHandleWrappers.push_back(classDecl);
                    continue;
                }
                // Classes that inherit an @ObjCMirror decl without carrying any annotation are collected too,
                // so that a proper diagnostic can be emitted for them.
                if (IsObjCImpl(*classDecl) || (IsObjCMirrorSubtype(*classDecl) && !IsObjCMirror(*classDecl))) {
                    ctx.impls.push_back(classDecl);
                    continue;
                }

                // @ObjCImpl registry companion class
                if (IsObjCImplRegistryCompanion(*classDecl)) {
                    ctx.regCompanions.push_back(classDecl);
                }
            }

            // @ObjCMirror toplevel func
            if (auto funcDecl = As<ASTKind::FUNC_DECL>(decl); funcDecl && IsObjCMirror(*funcDecl)) {
                ctx.mirrorTopLevelFuncs.push_back(funcDecl);
            }
        }
    }
}