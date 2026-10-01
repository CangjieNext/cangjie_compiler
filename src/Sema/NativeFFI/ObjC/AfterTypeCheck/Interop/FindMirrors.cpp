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
                classLikeDecl && classLikeDecl->IsObjCForeign()) {
                ctx.mirrors.push_back(classLikeDecl);
            }

            // Interfaces that inherit an @ObjCMirror interface without carrying the annotation are collected
            // as well, so that a proper diagnostic can be emitted for them.
            if (auto interfaceDecl = As<ASTKind::INTERFACE_DECL>(decl);
                interfaceDecl && !interfaceDecl->IsObjCForeign() && IsObjCForeignSubtype(*interfaceDecl)) {
                ctx.unannotatedMirrorSubtypeInterfaces.push_back(interfaceDecl);
            }

            if (auto classDecl = As<ASTKind::CLASS_DECL>(decl); classDecl) {
                // Mirror interface handle wrappers
                if (IsObjCForeignInterfaceHandleWrapper(*classDecl)) {
                    // The wrapper is generated whole by `InsertHandleWrapperDecl` and carries no user-written
                    // code, so nothing can mark it broken. The handlers below rely on that instead of each
                    // filtering the list on its own.
                    CJC_ASSERT_WITH_MSG(!classDecl->TestAnyAttr(Attribute::IS_BROKEN, Attribute::HAS_BROKEN),
                        "a generated '@ObjC' foreign interface handle wrapper cannot be broken");
                    ctx.mirrorInterfaceHandleWrappers.push_back(classDecl);
                    continue;
                }
                // Classes that inherit an @ObjCMirror decl without carrying any annotation are collected too,
                // so that a proper diagnostic can be emitted for them.
                if (classDecl->IsObjCExport() || (IsObjCForeignSubtype(*classDecl) && !classDecl->IsObjCForeign())) {
                    ctx.impls.push_back(classDecl);
                    continue;
                }

                // @ObjCImpl registry companion class
                if (IsObjCExportRegistryCompanion(*classDecl)) {
                    ctx.regCompanions.push_back(classDecl);
                }
            }

            // @ObjCMirror toplevel func
            if (auto funcDecl = As<ASTKind::FUNC_DECL>(decl); funcDecl && funcDecl->IsObjCForeign()) {
                ctx.mirrorTopLevelFuncs.push_back(funcDecl);
            }
        }
    }
}
