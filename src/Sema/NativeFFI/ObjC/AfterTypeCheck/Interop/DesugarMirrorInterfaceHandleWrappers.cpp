// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements desugar mirror interface handle wrappers.
 */

#include "Handlers.h"
#include "cangjie/AST/Node.h"

namespace Cangjie::Interop::ObjC {

using namespace Cangjie::AST;
using namespace Cangjie::Native::FFI;

void DesugarMirrorInterfaceHandleWrappers::HandleImpl(InteropContext& ctx)
{
    for (auto& wrapper : ctx.mirrorInterfaceHandleWrappers) {
        if (wrapper->TestAnyAttr(Attribute::IS_BROKEN, Attribute::HAS_BROKEN)) {
            continue;
        }
        wrapper->DisableAttr(Attribute::ABSTRACT);
    }
}
} // namespace Cangjie::Interop::ObjC
