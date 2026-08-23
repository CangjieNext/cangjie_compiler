// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "Base/ExprDispatcher/ExprDispatcher.h"

#include <cinttypes>

#include "Base/ArithmeticOpImpl.h"
#include "Base/CHIRExprWrapper.h"
#include "Base/LogicalOpImpl.h"
#include "Base/OverflowDispatcher.h"
#include "CGModule.h"
#include "IRBuilder.h"
#include "cangjie/CHIR/IR/Value/Value.h"

using namespace Cangjie::CHIR;

namespace Cangjie::CodeGen {
llvm::Value* HandleNonOverflowBinaryExpression(IRBuilder2& irBuilder, const CHIR::BinaryExpressionBase& chirExpr)
{
    if (chirExpr.IsMathematicalOperator()) {
        if (chirExpr.GetOpKind() == CHIR::BinaryExprKind::EXP) {
            return GenerateBinaryExpOperation(irBuilder, chirExpr);
        }
        return GenerateArithmeticOperation(irBuilder, chirExpr);
    }
    if (chirExpr.IsBitwiseOperator()) {
        return GenerateBitwiseOperation(irBuilder, chirExpr);
    }
    if (chirExpr.IsComparisonOperator()) {
        return GenerateBooleanOperation(irBuilder, chirExpr);
    }
    // AND and OR are short circuit operators. They are transformed to simple branches by CHIR
    // thus we are free from generating code for these expression kinds.
    auto exprKindStr = std::to_string(static_cast<uint64_t>(chirExpr.GetOpKind()));
    CJC_ASSERT_WITH_MSG(false, std::string("Unexpected CHIRBinaryExprKind: " + exprKindStr + "\n").c_str());
    return nullptr;
}

llvm::Value* HandleBinaryExpression(IRBuilder2& irBuilder, const CHIR::BinaryExpressionBase& chirExpr)
{
    const CHIR::Type* ty = chirExpr.GetResult()->GetType();
    const CHIR::BinaryExprKind opKind = chirExpr.GetOpKind();
    const CHIR::ExprKind kind = CHIR::ExprKindMgr::ToExprKind(opKind);
    OverflowStrategy overflowStrategy = chirExpr.GetOverflowStrategy();
    if (!ty) {
        return nullptr;
    }
    if (OPERATOR_KIND_TO_OP_MAP.find(kind) == OPERATOR_KIND_TO_OP_MAP.end()) {
        return HandleNonOverflowBinaryExpression(irBuilder, chirExpr);
    }
    if ((overflowStrategy == OverflowStrategy::NA || overflowStrategy == OverflowStrategy::WRAPPING) &&
        opKind != CHIR::BinaryExprKind::DIV && opKind != CHIR::BinaryExprKind::MOD) {
        return HandleNonOverflowBinaryExpression(irBuilder, chirExpr);
    }
    // There is a possibility of integer overflow when the result of an arithmetic expression is an integer type.(spec)
    if (!ty->IsInteger()) {
        return HandleNonOverflowBinaryExpression(irBuilder, chirExpr);
    }
    const CHIR::IntType* intTy = StaticCast<const CHIR::IntType*>(ty);
    auto& cgMod = irBuilder.GetCGModule();
    CGValue* valLeft = cgMod | chirExpr.GetLHSOperand();
    CGValue* valRight = cgMod | chirExpr.GetRHSOperand();
    irBuilder.EmitLocation(CHIRExprWrapper(chirExpr));
    return GenerateOverflow(irBuilder, overflowStrategy, kind, std::make_pair(intTy, nullptr), {valLeft, valRight});
}

} // namespace Cangjie::CodeGen
