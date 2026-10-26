// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_CHIR_TRANSFORMATION_REDUNDANT_FUTURE_REMOVAL_H
#define CANGJIE_CHIR_TRANSFORMATION_REDUNDANT_FUTURE_REMOVAL_H

#include "cangjie/CHIR/IR/CHIRBuilder.h"
#include "cangjie/CHIR/IR/Expression/Terminator.h"
#include "cangjie/CHIR/IR/Package.h"
#include "cangjie/CHIR/IR/Value/Value.h"

namespace Cangjie::CHIR {
/**
 * CHIR Opt Pass: replace future object with closure call in spawn expression.
 *
 * Handles both the plain form (`Spawn` / `Apply` / `Allocate`) and the form
 * inside a `try` block (`TrySpawn` / `TryApply` / `TryAllocate`).
 */
class RedundantFutureRemoval {
public:
    RedundantFutureRemoval(CHIRBuilder& builder, const Package& pkg, bool isDebug);

    /**
     * @brief Main process to do future remove in spawn expression.
     */
    void RunOnPackage();

private:
    void RunOnFunc(const Function& func);

    Function* GetExecuteClosureFunc() const;

    std::pair<LocalVar*, ApplyBase*> CheckSpawnWithFuture(Expression& expr) const;

    void RewriteSpawnWithOutFuture(SpawnBase& spawnExpr, LocalVar& futureValue, ApplyBase& apply);

    /**
     * @brief Drop a terminator that is no longer needed and fall through to its
     * success block instead, keeping the block well formed.
     */
    void ReplaceTerminatorWithGoTo(Expression& terminator, Block& successBlock) const;

    CHIRBuilder& builder;
    const Package& package;
    bool isDebug{false};

    Function* executeClosure{nullptr};
};

}

#endif