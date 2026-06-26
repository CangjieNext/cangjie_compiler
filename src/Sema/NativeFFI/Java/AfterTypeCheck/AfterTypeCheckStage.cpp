// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "AfterTypeCheckStage.h"
#include "AfterTypeCheckContext.h"

namespace Cangjie::Native::FFI::Java {

void AfterTypeCheckStage::operator()(AfterTypeCheckContext& ctx)
{
    Process(ctx);
    ctx.FlushGeneratedDecls();
}

} // namespace Cangjie::Native::FFI::Java
