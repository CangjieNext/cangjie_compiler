// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares the entry point of the PrepareTypeCheck stage of Cangjie <-> Objective-C interopability.
 */

#ifndef CANGJIE_SEMA_NATIVEFFI_OBJC_BEFORETYPECHECK_DESUGAR_H
#define CANGJIE_SEMA_NATIVEFFI_OBJC_BEFORETYPECHECK_DESUGAR_H

#include "cangjie/AST/Node.h"
#include "cangjie/Modules/ImportManager.h"
#include "cangjie/Sema/TypeManager.h"

namespace Cangjie::Interop::ObjC {
void PrepareTypeCheck(AST::Package& pkg, ImportManager& importManager, TypeManager& typeManager);
}

#endif // CANGJIE_SEMA_NATIVEFFI_OBJC_BEFORETYPECHECK_DESUGAR_H
