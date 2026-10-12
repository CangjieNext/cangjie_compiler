// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_CHIR_TRANSFORMATION_REDUNDANT_LOAD_ELIMINATION_H
#define CANGJIE_CHIR_TRANSFORMATION_REDUNDANT_LOAD_ELIMINATION_H

#include "cangjie/CHIR/IR/Package.h"

namespace Cangjie::CHIR {
/**
 * CHIR Opt Pass: replace redundant load expr with exact value itself.
 */
class RedundantLoadElimination {
public:
    /**
     * @brief constructor for redundant load expression elimination.
     */
    explicit RedundantLoadElimination();

    /**
     * @brief Main process to do redundant load elimination.
     * @param package package to do optimization.
     */
    void RunOnPackage(const Package& package) const;

private:
    void RunOnFunc(const Function& func) const;
};
} // namespace Cangjie::CHIR

#endif