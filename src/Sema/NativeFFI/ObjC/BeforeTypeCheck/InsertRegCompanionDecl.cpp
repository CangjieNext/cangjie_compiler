// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements inserting an empty registry companion class declaration for each @ObjCImpl
 * declaration. The companion holds the state that must outlive the escaping Objective-C counterpart;
 * its members are moved there at the AfterTypeCheck stage, see MoveImplMembersToRegCompanion.
 */

#include "Handlers.h"
#include "cangjie/AST/Node.h"

using namespace Cangjie::AST;
using namespace Cangjie::Interop::ObjC;

void InsertRegCompanionDecl::HandleImpl(PrepareTypeCheckContext& ctx)
{
    for (auto& impl : ctx.impls) {
        auto regCompanionDecl = MakeOwned<ClassDecl>();
        regCompanionDecl->body = MakeOwned<ClassBody>();
        regCompanionDecl->EnableAttr(Attribute::COMPILER_ADD);
        ctx.astTransformer.TransformToRegistryCompanion(*regCompanionDecl, *impl);

        // Insert requires OwnedPtr<Node>
        auto regCompPtr = regCompanionDecl.get();
        ctx.astInserter.InsertInto(*impl->curFile, std::move(regCompanionDecl));
        ctx.importManager.GetCjoManager()->AddGeneratedDeclToDeclMap(*regCompPtr);
    }
}
