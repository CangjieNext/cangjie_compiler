// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * TBD
 */

#include "Handlers.h"

using namespace Cangjie::AST;
using namespace Cangjie::Interop::ObjC;

void InsertHandleWrapperDecl::HandleImpl(PrepareTypeCheckContext& ctx)
{
    for (auto& mirror : ctx.mirrors) {
        if (!mirror->IsInterfaceDecl()) {
            continue;
        }

        auto handleWrapperDecl = ctx.astFactory.CreateClassDecl();
        ctx.astTransformer.TransformToHandleWrapper(*handleWrapperDecl, *mirror);

        // Insert requires OwnedPtr<Node>
        auto handleWrapperPtr = handleWrapperDecl.get();
        ctx.astInserter.InsertInto(*mirror->curFile, std::move(handleWrapperDecl));
        ctx.importManager.GetCjoManager()->AddGeneratedDeclToDeclMap(*handleWrapperPtr);
    }
}
