// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/Optimization/RedundantFutureRemoval.h"

#include "cangjie/CHIR/Analysis/Utils.h"
#include "cangjie/CHIR/Utils/CHIRCasting.h"
#include "cangjie/CHIR/Utils/Utils.h"
#include "cangjie/CHIR/Utils/Visitor/Visitor.h"

namespace Cangjie::CHIR {

RedundantFutureRemoval::RedundantFutureRemoval(CHIRBuilder& builder, const Package& pkg, bool isDebug)
    : builder(builder), package(pkg), isDebug(isDebug)
{
}

void RedundantFutureRemoval::RunOnPackage()
{
    for (auto func : package.GetGlobalFuncsWithBody()) {
        RunOnFunc(*func);
    }
}

void RedundantFutureRemoval::RunOnFunc(const Function& func)
{
    auto visitExitAction = [this](Expression& expr) {
        auto [future, apply] = CheckSpawnWithFuture(expr);
        if (future != nullptr) {
            auto spawnExpr = StaticCast<SpawnBase*>(&expr);
            RewriteSpawnWithOutFuture(*spawnExpr, *future, *apply);
            if (isDebug) {
                std::string message = "[RedundantFutureRemoval] The call to Spawn" +
                    ToPosInfo(expr.GetDebugLocation()) +
                    " has been optimised due to redundant future in spawn.\n";
                std::cout << message;
            }
        }
        return VisitResult::CONTINUE;
    };
    Visitor::Visit(func, visitExitAction);
}

Function* RedundantFutureRemoval::GetExecuteClosureFunc() const
{
    for (auto def : package.GetAllCustomTypeDef()) {
        if (!IsCoreFutureT(*def)) {
            continue;
        }
        for (auto method : def->GetMethods()) {
            if (method->GetSrcCodeIdentifier() == "executeClosure") {
                return method;
            }
        }
        return nullptr;
    }
    return nullptr;
}

void RedundantFutureRemoval::ReplaceTerminatorWithGoTo(Expression& terminator, Block& successBlock) const
{
    auto parentBlock = terminator.GetParentBlock();
    CJC_NULLPTR_CHECK(parentBlock);
    auto loc = terminator.GetDebugLocation();
    // RemoveSelfFromBlock also drops this block from each successor's predecessor
    // list, so the exception edge disappears along with the terminator.
    terminator.RemoveSelfFromBlock();
    parentBlock->AppendExpression(builder.CreateTerminator<GoTo>(loc, &successBlock, parentBlock));
}

void RedundantFutureRemoval::RewriteSpawnWithOutFuture(SpawnBase& spawnExpr, LocalVar& futureValue, ApplyBase& apply)
{
    /* The `Future` object only exists to be handed to `Spawn`, so drop it and
       let `Spawn` take the closure directly.

       change from:
        %1: () -> Int64 = Lambda[...](): Int64
        %2: Class-Future<Int64>& = Allocate(Class-Future<Int64>)
        %3: Void = Apply(Class-Future<Int64>&->@Future<Int64>::<init>, %2, %1)
        %4: Class-Future<Int64>& = Spawn(%2)
        %5: Unit = Debug(%2, f)                 // only when bound to a variable

       change to:
        %1: () -> Int64 = Lambda[...](): Int64
        %4: Class-Future<Int64>& = Spawn(%1)    // executeClosure: @Future<Int64>::executeClosure
        %5: Unit = Debug(%1, f)

       Note that `Allocate` and `Apply` are erased, while any `Debug` is kept
       and retargeted to the lambda so the variable stays observable. `Spawn`
       keeps its own result type and gains an `executeClosure` reference, which
       is what tells CodeGen to pass a closure instead of a `Future`
       (see GenerateSpawn in src/CodeGen/Base/SpawnExprImpl.cpp).

       Inside a `try` block the same three nodes appear as their exception
       throwing counterparts, each ending its own basic block:

        Block #0:  %2: Class-Future<Int64>& = TryAllocate(Class-Future<Int64>)
        Block #8:  %3: Void = TryApply(..., %2, %1, normal: #9, exception: #3)
        Block #9:  %4: Class-Future<Int64>& = TrySpawn(%2, normal: #2, exception: #3)

       `TryAllocate` and `TryApply` are terminators, so they cannot simply be
       erased -- that would leave their blocks without one. They are replaced by
       a `GoTo` to their own success block instead: allocating a `Future` and
       running its constructor are the very things being optimised away, and
       neither can throw once removed, so the exception edge is dropped too.
       `TrySpawn` itself stays put, only swapping its operand and gaining the
       `executeClosure` reference, exactly as in the plain case.
    */
    // 1. Get Lambda from apply expression
    auto lambda = apply.GetOperand(2U);
    CJC_ASSERT(lambda->GetType()->IsFunc());

    // 2. Replace spawn and remove useless node
    auto futureExpression = futureValue.GetExpr();
    if (auto tryApply = DynamicCast<TryApply*>(&apply)) {
        ReplaceTerminatorWithGoTo(*tryApply, *tryApply->GetSuccessBlock());
    } else {
        apply.RemoveSelfFromBlock();
    }
    futureValue.ReplaceWith(*lambda, spawnExpr.GetParentBlock()->GetParentBlockGroup());
    if (auto tryAllocate = DynamicCast<TryAllocate*>(futureExpression)) {
        ReplaceTerminatorWithGoTo(*tryAllocate, *tryAllocate->GetSuccessBlock());
    } else {
        futureExpression->RemoveSelfFromBlock();
    }
    if (executeClosure == nullptr) {
        executeClosure = GetExecuteClosureFunc();
        CJC_NULLPTR_CHECK(executeClosure);
    }
    spawnExpr.SetExecuteClosure(*executeClosure);
}

std::pair<LocalVar*, ApplyBase*> RedundantFutureRemoval::CheckSpawnWithFuture(Expression& expr) const
{
    // Covers both `Spawn` and the `TrySpawn` produced inside a `try` block.
    if (!Is<SpawnBase>(expr)) {
        return {nullptr, nullptr};
    }
    auto spawnExpr = StaticCast<SpawnBase*>(&expr);
    if (spawnExpr->IsExecuteClosure()) {
        return {nullptr, nullptr};
    }
    auto spawnOperand = spawnExpr->GetFuture();
    if (!spawnOperand->IsLocalVar()) {
        return {nullptr, nullptr};
    }
    auto localFuture = StaticCast<LocalVar*>(spawnOperand);
    auto users = localFuture->GetUsers();
    std::unordered_set<Expression*> usersSet(users.begin(), users.end());
    if (usersSet.size() == 3U) {
        // if spawn and future debug is only users of future, then optimize.
        // future would have exactly three users: apply future, as a paramter in spawn and debug
        usersSet.erase(localFuture->GetDebugExpr());
    }
    if (usersSet.size() == 2U) {
        // if spawn is only user of future, then optimize.
        // future would have exactly two users: apply future and use in spawn
        usersSet.erase(spawnExpr);
    }
    if (usersSet.size() != 1) {
        return {nullptr, nullptr};
    }
    // optimize spawn if only apply is left
    auto apply = *usersSet.begin();
    if (!Is<ApplyBase>(*apply)) {
        return {nullptr, nullptr};
    }
    return {localFuture, StaticCast<ApplyBase*>(apply)};
}

} // namespace Cangjie::CHIR
