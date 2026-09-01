// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements inserting a handle wrapper class declaration for each @ObjCMirror interface.
 * The wrapper is the class an Objective-C object is represented by when it is known to conform to the
 * mirrored protocol only.
 */

#include "Handlers.h"
#include "cangjie/AST/Node.h"

using namespace Cangjie::AST;
using namespace Cangjie::Interop::ObjC;

void InsertHandleWrapperDecl::HandleImpl(PrepareTypeCheckContext& ctx)
{
    for (auto& mirror : ctx.mirrors) {
        if (!mirror->IsInterfaceDecl()) {
            continue;
        }

        auto handleWrapperDecl = MakeOwned<ClassDecl>();
        handleWrapperDecl->body = MakeOwned<ClassBody>();
        handleWrapperDecl->EnableAttr(Attribute::COMPILER_ADD);
        ctx.astTransformer.TransformToHandleWrapper(*handleWrapperDecl, *mirror);

        // Insert requires OwnedPtr<Node>
        auto handleWrapperPtr = handleWrapperDecl.get();
        ctx.astInserter.InsertInto(*mirror->curFile, std::move(handleWrapperDecl));
        ctx.importManager.GetCjoManager()->AddGeneratedDeclToDeclMap(*handleWrapperPtr);
    }
}
