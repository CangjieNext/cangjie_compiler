// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements restoring curFile field recursively for the package after our desugaring
 */

#include "Handlers.h"
#include "cangjie/AST/Utils.h"

namespace Cangjie::Interop::ObjC {

using namespace Cangjie::AST;
using namespace Cangjie::Native::FFI;

void RestoreCurFile::HandleImpl(InteropContext& ctx)
{
    for (auto& file : ctx.pkg.files) {
        AddCurFile(*file, file.get());
    }
}

} // namespace Cangjie::Interop::ObjC
