// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements a translation from CHIR to BCHIR.
 */
#include "cangjie/CHIR/Interpreter/CHIR2BCHIR.h"
#include "cangjie/CHIR/Interpreter/Utils.h"

using namespace Cangjie::CHIR;
using namespace Interpreter;

void CHIR2BCHIR::TranslateBinaryExpression(Context& ctx, const BinaryExpressionBase& expr)
{
    CJC_ASSERT(expr.GetNumOfNonSuccessorOperands() == Bchir::FLAG_TWO);
    auto opKind = expr.GetOpKind();
    auto opCode = expr.IsTerminator()
        ? Cangjie::CHIR::Interpreter::BinExprKindWitException2OpCode(opKind)
        : Cangjie::CHIR::Interpreter::BinExprKind2OpCode(opKind);
    auto typeKind = expr.GetLHSOperand()->GetType()->GetTypeKind();
    auto overflowStrat = static_cast<Bchir::ByteCodeContent>(expr.GetOverflowStrategy());
    PushOpCodeWithAnnotations<false, true>(ctx, opCode, expr, typeKind, overflowStrat);
    if (opCode == OpCode::BIN_LSHIFT || opCode == OpCode::BIN_RSHIFT || opCode == OpCode::BIN_LSHIFT_EXC ||
        opCode == OpCode::BIN_RSHIFT_EXC) {
        ctx.def.Push(static_cast<Bchir::ByteCodeContent>(expr.GetRHSOperand()->GetType()->GetTypeKind()));
    }
}