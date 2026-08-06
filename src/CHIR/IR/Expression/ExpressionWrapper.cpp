// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/IR/Expression/ExpressionWrapper.h"

#include "cangjie/CHIR/Utils/CHIRCasting.h"
#include "cangjie/Utils/CastingTemplate.h"

using namespace Cangjie::CHIR;

ExpressionBase::ExpressionBase(const Expression* e) : expr(e)
{
    CJC_NULLPTR_CHECK(e);
}

const Expression* ExpressionBase::GetRawExpr() const
{
    return expr;
}

LocalVar* ExpressionBase::GetResult() const
{
    return expr->GetResult();
}

IntrinsicBase::IntrinsicBase(const Expression* e) : ExpressionBase(e)
{
    CJC_NULLPTR_CHECK(e);
    if (e->GetExprKind() == ExprKind::INTRINSIC) {
        expr = Cangjie::StaticCast<const Intrinsic*>(e);
        exprE = nullptr;
    } else {
        expr = nullptr;
        exprE = Cangjie::StaticCast<const IntrinsicWithException*>(e);
    }
}

IntrinsicBase::IntrinsicBase(const Intrinsic* expr) : ExpressionBase(expr), expr(expr), exprE(nullptr)
{
    CJC_NULLPTR_CHECK(expr);
}

IntrinsicBase::IntrinsicBase(const IntrinsicWithException* exprE) : ExpressionBase(exprE), expr(nullptr), exprE(exprE)
{
    CJC_NULLPTR_CHECK(exprE);
}

IntrinsicKind IntrinsicBase::GetIntrinsicKind() const
{
    return expr ? expr->GetIntrinsicKind()
                : exprE->GetIntrinsicKind();
}

std::vector<Type*> IntrinsicBase::GetInstantiatedTypeArgs() const
{
    return expr ? expr->GetInstantiatedTypeArgs()
                : exprE->GetInstantiatedTypeArgs();
}

std::vector<Value*> IntrinsicBase::GetArgs() const
{
    return expr ? expr->GetArgs()
                : exprE->GetArgs();
}
