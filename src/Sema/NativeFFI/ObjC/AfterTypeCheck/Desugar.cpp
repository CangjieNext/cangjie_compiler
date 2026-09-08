// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements the entrypoint to handle Objective-C mirrors and theirs subtypes desugaring.
 */

#include "Desugar.h"
#include "Interop/Handlers.h"

void Cangjie::Interop::ObjC::Desugar(InteropContext&& ctx)
{
    if (!ctx.bridge.IsInteropLibAccessible()) {
        return;
    }

    HandlerFactory<InteropContext>::Start<FindMirrors>()
        .Use<CheckMirrorTypes>()
        .Use<CheckImplTypes>()
        .Use<PropagateBrokenToSubtypes>()
        .Use<MapImplToRegCompanion>()
        .Use<RestoreRegCompanionsTypeHierarchy>()
        .Use<InsertNativeHandleField>()
        .Use<InsertObjCImplRegCompanionField>()
        .Use<InsertNativeHandleGetterDecl>()
        .Use<InsertNativeHandleGetterBody>()
        .Use<InsertGetObjCClass>()
        .Use<InsertBaseCtorDecl>()
        .Use<InsertBaseCtorBody>()
        .Use<MoveImplMembersToRegCompanion>()
        .Use<InsertFinalizer>()
        .Use<GenerateInMirrorInterfaceHandleWrappers>()
        .Use<InsertStringConversions>()
        .Use<DesugarMirrors>()
        .Use<DesugarMirrorInterfaceHandleWrappers>()
        .Use<GenerateObjCImplMembers>()
        .Use<GenerateInitCJObjectMethods>()
        .Use<GenerateDeleteCJObjectMethod>()
        .Use<DesugarImpls>()
        .Use<RewriteObjCTypechecks>()
        .Use<GenerateWrappers>()
        .Use<GenerateGlueCode>()
        .Use<CheckObjCPointerTypeArguments>()
        .Use<RewriteObjCPointerAccess>()
        .Use<CheckObjCFuncTypeArguments>()
        .Use<RewriteObjCFuncCall>()
        .Use<RewriteObjCBlockConstruction>()
        .Use<DrainGeneratedDecls>()
        .Use<RestoreCurFile>()
        .Handle(ctx);
}
