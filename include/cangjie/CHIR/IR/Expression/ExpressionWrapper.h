// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * Many Expressions have their xxxWithException version, we wrap them with a new class instead of using a base class
 */

#ifndef CANGJIE_CHIR_EXPRESSION_WRAPPER_H
#define CANGJIE_CHIR_EXPRESSION_WRAPPER_H

#include "cangjie/CHIR/IR/Expression/Terminator.h"

namespace Cangjie {
namespace CHIR {

class ExpressionBase {
public:
    const Expression* GetRawExpr() const;
    LocalVar* GetResult() const;

protected:
    explicit ExpressionBase(const Expression* e);

private:
    const Expression* expr;
};

class IntrinsicBase : public ExpressionBase {
public:
    explicit IntrinsicBase(const Expression* e);
    explicit IntrinsicBase(const Intrinsic* expr);
    explicit IntrinsicBase(const IntrinsicWithException* exprE);

    IntrinsicKind GetIntrinsicKind() const;
    std::vector<Type*> GetInstantiatedTypeArgs() const;
    std::vector<Value*> GetArgs() const;

private:
    const Intrinsic* expr;
    const IntrinsicWithException* exprE;
};

}
}

#endif
