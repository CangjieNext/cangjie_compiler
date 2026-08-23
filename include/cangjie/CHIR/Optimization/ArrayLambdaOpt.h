// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_CHIR_TRANSFORMATION_ARRAY_LAMBDA_OPT_H
#define CANGJIE_CHIR_TRANSFORMATION_ARRAY_LAMBDA_OPT_H

#include "cangjie/CHIR/IR/Expression/Terminator.h"
#include "cangjie/CHIR/IR/Package.h"
#include "cangjie/CHIR/IR/Value/Value.h"

namespace Cangjie::CHIR {
/**
 * CHIR Opt Pass: optimize CHIR IR from lambda function init to value init.
 */
class ArrayLambdaOpt {
public:
    /**
     * @brief constructor of array lambda optimization pass.
     * @param builder CHIR builder for generating IR.
     */
    explicit ArrayLambdaOpt(CHIRBuilder& builder);

    /**
     * @brief run array lambda optimization on a certain package CHIR IR.
     * @param package package to do optimization.
     */
    void RunOnPackage(const Package& package);

private:
    void RunOnFunc(const Function& func);

    Constant* CheckCanRewriteLambda(const Expression& expr) const;

    Constant* CheckIfLambdaReturnConst(const Lambda& lambda) const;

    void RewriteArrayInitFunc(Apply& apply, const Constant& constant);

    Intrinsic* CheckCanRewriteZeroValue(const Expression& expr) const;

    void RewriteZeroValue(RawArrayInitByValue& init, Intrinsic& zeroVal) const;

    CHIRBuilder& builder;

    // `arrayInitByFunction` parameters index
    size_t rawArrayIndex = 0;
    size_t initLambdaIndex = 1;
};
} // namespace Cangjie::CHIR

#endif