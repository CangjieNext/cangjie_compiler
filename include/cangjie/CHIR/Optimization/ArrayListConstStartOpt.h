// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_CHIR_TRANSFORMATION_ARRAYLIST_CONST_START_OPT_H
#define CANGJIE_CHIR_TRANSFORMATION_ARRAYLIST_CONST_START_OPT_H


#include "cangjie/CHIR/Optimization/FunctionInline.h"


namespace Cangjie::CHIR {
/**
 * CHIR Opt Pass: optimization for cangjie array list loop and start point.
 *     1. inline Array.getUnchecked / setUnchecked into ArrayList related funcs.
 *     2. right after each inline, replace ArrayList.myData.start with const zero.
 */
class ArrayListConstStartOpt {
public:
    /**
     * @brief constructor of optimization for cangjie array list loop and start point.
     * @param builder CHIR builder for generating IR.
     * @param pass inline opt pass.
     */
    explicit ArrayListConstStartOpt(CHIRBuilder& builder, FunctionInline& pass)
        : builder(builder), pass(pass)
    {
    }

    /**
     * @brief run array list start optimization on a certain package CHIR IR.
     * @param package package to do optimization.
     */
    void RunOnPackage(const Package& package);

    /**
     * @brief Get effect map after this pass.
     * @return effect map affected by this pass.
     */
    const OptEffectCHIRMap& GetEffectMap() const;

private:
    bool CheckNeedInline(const Apply& apply) const;
    bool IsStartOfArrayListMyData(const Field& field, const Function& func) const;
    void RewriteStartWithConstZero(Expression& oldExpr) const;
    void RewriteStartAfterInline(Function& func) const;

    CHIRBuilder& builder;
    const std::string optPassName{"ArrayListConstStartOpt Inline"};
    FunctionInline& pass;
    OptEffectCHIRMap effectMap;
};
} // namespace Cangjie::CHIR

#endif
