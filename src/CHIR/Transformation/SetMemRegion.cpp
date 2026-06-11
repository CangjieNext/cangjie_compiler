// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/Transformation/SetMemRegion.h"

#include "cangjie/CHIR/IR/Expression/Terminator.h"
#include "cangjie/CHIR/Utils/CHIRCasting.h"
#include "cangjie/CHIR/Utils/Utils.h"
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
            Is<Intrinsic>(expr) || Is<IntrinsicWithException>(expr) ||
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
    auto blocks = func.GetBody()->GetBlocks();
    FlattenExclave(blocks, *func.GetBody(), builder, needsRegion);
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