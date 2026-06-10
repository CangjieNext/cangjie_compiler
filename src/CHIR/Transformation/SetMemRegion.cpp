// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/Transformation/SetMemRegion.h"

#include "cangjie/CHIR/IR/Expression/Terminator.h"
#include "cangjie/CHIR/Utils/CHIRCasting.h"
#include "cangjie/CHIR/Utils/Visitor/Visitor.h"

using namespace Cangjie::CHIR;

SetMemRegion::SetMemRegion(CHIRBuilder& builder)
    : builder(builder)
{
}

SetMemRegion::~SetMemRegion()
{
}

void SetMemRegion::RunOnPackage(const Package& package)
{
    for (auto& func : package.GetGlobalFuncsWithBody()) {
        RunOnFunc(*func);
    }
}

bool SetMemRegion::FunctionNeedsRegion(const Function& func) const
{
    bool needsRegion = false;
    Visitor::Visit(*func.GetBody(), [&needsRegion](Expression& expr) -> VisitResult {
        if (Is<Exclave>(expr)) {
            return VisitResult::SKIP;
        }
        if ((Is<Tuple>(expr) && expr.GetResult()->GetType()->IsEnum()) ||
            Is<FuncCall>(expr) || Is<FuncCallWithException>(expr) ||
            Is<Allocate>(expr) || Is<AllocateWithException>(expr) ||
            Is<RawArrayAllocate>(expr) || Is<RawArrayAllocateWithException>(expr)) {
            if (expr.GetResult()->GetType()->StripAllRefs()->IsLocalRegion()) {
                needsRegion = true;
                return VisitResult::STOP;
            }
            return VisitResult::CONTINUE;
        }
        return VisitResult::CONTINUE;
    });
    return needsRegion;
}

void SetMemRegion::RunOnFunc(const Function& func)
{
    auto needsRegion = FunctionNeedsRegion(func);
    if (needsRegion) {
        SetRegions(func);
    }
    FlattenExclave(*func.GetBody(), needsRegion);
}

void SetMemRegion::FlattenExclave(BlockGroup& blockGroup, bool needsEndRegion)
{
    for (auto& block : blockGroup.GetBlocks()) {
        auto terminator = block->GetTerminator();
        if (!Is<Exclave>(terminator)) {
            continue;
        }
        // 1. flatten the innermost exclave
        auto exclave = StaticCast<Exclave*>(terminator);
        FlattenExclave(*exclave->GetBody(), false);
        // 2. move the sub blocks to the parent block group
        auto subEntryBlock = exclave->GetBody()->GetEntryBlock();
        auto subBlocks = exclave->GetBody()->GetBlocks();
        for (auto subBlock : subBlocks) {
            subBlock->MoveTo(blockGroup);
        }
        // 3. replace the exclave terminator with a goto to the sub entry block
        auto parentBlock = exclave->GetParentBlock();
        auto gotoExpr = builder.CreateTerminator<GoTo>(subEntryBlock, parentBlock);
        terminator->ReplaceWith(*gotoExpr);
        // 4. add an end region instruction if needed
        if (needsEndRegion) {
            auto endRegion = builder.CreateExpression<EndRegion>(builder.GetUnitTy(), parentBlock);
            endRegion->MoveBefore(gotoExpr);
        }
    }
}

void SetMemRegion::SetRegions(const Function& func)
{
    auto entryBlock = func.GetBody()->GetEntryBlock();
    auto startRegion = builder.CreateExpression<StartRegion>(builder.GetUnitTy(), entryBlock);
    entryBlock->InsertExprIntoHead(*startRegion);
    for (auto block : func.GetBody()->GetBlocks()) {
        auto terminator = block->GetTerminator();
        if (!Is<Exit>(terminator) && !Is<RaiseException>(terminator)) {
            continue;
        }
        auto endRegion = builder.CreateExpression<EndRegion>(builder.GetUnitTy(), block);
        endRegion->MoveBefore(terminator);
    }
}