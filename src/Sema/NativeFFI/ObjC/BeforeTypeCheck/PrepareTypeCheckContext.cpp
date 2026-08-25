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

#include "PrepareTypeCheckContext.h"
#include "Utils/ASTTransformer.h"

using namespace Cangjie::Interop::ObjC;

PrepareTypeCheckContext::PrepareTypeCheckContext(
    AST::Package& pkg, TypeManager& typeManager, ImportManager& importManager) noexcept
    : pkg(pkg),
      importManager(importManager),
      astTransformer{typeManager, astFactory},
      nameGenerator(BaseMangler{}, typeManager)
{
}
