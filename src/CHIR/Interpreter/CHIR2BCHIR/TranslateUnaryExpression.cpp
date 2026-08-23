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

void CHIR2BCHIR::TranslateUnaryExpression(Context& ctx, const UnaryExpressionBase& expr)
{
    CJC_ASSERT(expr.GetNumOfNonSuccessorOperands() == Bchir::FLAG_ONE);
    auto opKind = expr.GetOpKind();
    auto opCode = expr.IsTerminator()
        ? Cangjie::CHIR::Interpreter::UnExprKindWitException2OpCode(opKind)
        : Cangjie::CHIR::Interpreter::UnExprKind2OpCode(opKind);
    auto typeKind = expr.GetOperand()->GetType()->GetTypeKind();
    auto overflow = static_cast<Bchir::ByteCodeContent>(expr.GetOverflowStrategy());
    PushOpCodeWithAnnotations<false, true>(ctx, opCode, expr, typeKind, overflow);
}