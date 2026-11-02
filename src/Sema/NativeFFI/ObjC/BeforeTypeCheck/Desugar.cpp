// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements the entry point of the PrepareTypeCheck stage of Cangjie <-> Objective-C
 * interop
 */

#include "Desugar.h"
#include "Handlers.h"

using namespace Cangjie::AST;

namespace Cangjie::Interop::ObjC {
void PrepareTypeCheck(Package& pkg, ImportManager& importManager, TypeManager& typeManager)
{
    PrepareTypeCheckContext ctx = {pkg, typeManager, importManager};
    HandlerFactory<PrepareTypeCheckContext>::Start<WarmupContext>()
        .Use<ReplaceFieldsWithProps>()
        .Use<InsertToString>()
        .Use<InsertFromStringCtor>()
        .Use<InsertRegCompanionDecl>()
        .Use<InsertHandleWrapperDecl>()
        .Handle(ctx);
}
} // namespace Cangjie::Interop::ObjC
