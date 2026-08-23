// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_ALLOCATEIMPL_H
#define CANGJIE_ALLOCATEIMPL_H
#include "llvm/IR/Value.h"

namespace Cangjie {
namespace CHIR {
class AllocateBase;
} // namespace CHIR
namespace CodeGen {
class IRBuilder2;
llvm::Value* GenerateAllocate(IRBuilder2& irBuilder, const CHIR::AllocateBase& alloca);
} // namespace CodeGen
} // namespace Cangjie
#endif // CANGJIE_ALLOCATEIMPL_H
