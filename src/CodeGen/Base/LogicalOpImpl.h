// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_LOGICALOP_IMPL_H
#define CANGJIE_LOGICALOP_IMPL_H

#include "llvm/IR/Value.h"

#include "cangjie/CHIR/IR/Expression/Terminator.h"

namespace Cangjie {
namespace CodeGen {
class IRBuilder2;
llvm::Value* GenerateBooleanOperation(IRBuilder2& irBuilder, const CHIR::BinaryExpressionBase& binOp);
} // namespace CodeGen
} // namespace Cangjie

#endif // CANGJIE_LOGICALOP_IMPL_H
