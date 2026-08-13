// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/Optimization/ArrayLambdaOpt.h"

#include "cangjie/CHIR/Analysis/Utils.h"
#include "cangjie/CHIR/Utils/CHIRCasting.h"
#include "cangjie/CHIR/Utils/Utils.h"
#include "cangjie/CHIR/Utils/Visitor/Visitor.h"

using namespace Cangjie::CHIR;

ArrayLambdaOpt::ArrayLambdaOpt(CHIRBuilder& builder) : builder(builder)
{
}

void ArrayLambdaOpt::RunOnPackage(const Package& package)
{
    for (auto func : package.GetGlobalFuncsWithBody()) {
        RunOnFunc(*func);
    }
}

void ArrayLambdaOpt::RunOnFunc(const Function& func)
{
    auto preAcation = [this](Expression& expr) {
        if (auto constVal = CheckCanRewriteLambda(expr)) {
            RewriteArrayInitFunc(StaticCast<Apply&>(expr), *constVal);
        } else if (auto zeroValue = CheckCanRewriteZeroValue(expr)) {
            RewriteZeroValue(StaticCast<RawArrayInitByValue&>(expr), *zeroValue);
        }
        return VisitResult::CONTINUE;
    };
    Visitor::Visit(func, preAcation);
}

Constant* ArrayLambdaOpt::CheckCanRewriteLambda(const Expression& expr) const
{
    auto apply = DynamicCast<Apply*>(&expr);
    if (!apply) {
        return nullptr;
    }
    auto callee = apply->GetCallee();
    if (!callee->IsFuncWithBody()) {
        return nullptr;
    }
    const FuncInfo ARRAY_INIT_FUNC_INFO{
        "arrayInitByFunction", "", {NOT_CARE}, NOT_CARE, Cangjie::CORE_PACKAGE_NAME};
    if (!IsExpectedFunction(*StaticCast<Function*>(callee), ARRAY_INIT_FUNC_INFO)) {
        return nullptr;
    }

    auto closureVar = DynamicCast<LocalVar*>(apply->GetArg(initLambdaIndex));
    if (closureVar == nullptr) {
        return nullptr;
    }
    auto closure = DynamicCast<Lambda*>(closureVar->GetExpr());
    if (closure == nullptr) {
        return nullptr;
    }
    return CheckIfLambdaReturnConst(*closure);
}

Constant* ArrayLambdaOpt::CheckIfLambdaReturnConst(const Lambda& lambda) const
{
    auto ret = lambda.GetReturnValue();
    CJC_NULLPTR_CHECK(ret);
    CJC_ASSERT(ret->GetExpr()->GetExprKind() == ExprKind::ALLOCATE);
    auto retValUsers = ret->GetUsers();
    if (retValUsers.size() != 1) {
        return nullptr;
    }
    auto store = DynamicCast<Store*>(retValUsers[0]);
    if (store == nullptr) {
        return nullptr;
    }
    auto srcValue = DynamicCast<LocalVar*>(store->GetValue());
    if (srcValue == nullptr) {
        return nullptr;
    }
    auto constRetVal = DynamicCast<Constant*>(srcValue->GetExpr());
    if (constRetVal == nullptr) {
        return nullptr;
    }
    auto blocksInLambda = lambda.GetBody()->GetBlocks();
    if (blocksInLambda.size() > 1) {
        return nullptr;
    }
    // can't have other expressions in the lambda body, in case of side effects
    std::unordered_set<Expression*> validExprs{ret->GetExpr(), store, constRetVal};
    for (auto e : blocksInLambda[0]->GetExpressions()) {
        if (Is<Debug>(e) || e->IsTerminator()) {
            continue;
        }
        if (validExprs.find(e) == validExprs.end()) {
            return nullptr;
        }
    }
    return constRetVal;
}

void ArrayLambdaOpt::RewriteArrayInitFunc(Apply& apply, const Constant& constant)
{
    /** cangjie code:
            ArrayList<Int64>(10000, { i = 2 })
        before optimization:
            %0: (Int64) -> Int64 = Lambda(%27[i]: Int64): Int64
            { Block Group: 0
            Block #0: 
                [ret] %1: Int64& = Allocate(Int64)
                %2: Int64 = Constant(2i)
                %3: Unit = Store(%2, %1)
                Exit()
            }
            %4: Int64 = Constant(100000i)
            %5: RawArray<Int64>& = RawArrayAllocate(Int64, %4)
            %6: RawArray<Int64>& = Apply(@arrayInitByFunction, %5, %0)
        after optimization:
            %4: Int64 = Constant(100000i)
            %5: RawArray<Int64>& = RawArrayAllocate(Int64, %4)
            %6: Int64 = Constant(2i)
            %7: Unit = RawArrayInitByValue(%5, %4, %6)  // address, size, init value
    */
    // 1. create init value, just clone the old one
    auto parent = apply.GetParentBlock();
    auto initVal = builder.CreateExpression<Constant>(constant.GetDebugLocation(), constant.GetResult()->GetType(),
        StaticCast<LiteralValue*>(constant.GetValue()), parent);
    initVal->MoveBefore(&apply);

    // 2. create `RawArrayInitByValue`
    auto lambda = StaticCast<Lambda*>(StaticCast<LocalVar*>(apply.GetArg(initLambdaIndex))->GetExpr());
    auto& loc = apply.GetDebugLocation();
    auto rawArray = apply.GetArg(rawArrayIndex);
    auto size = StaticCast<RawArrayAllocateBase*>(StaticCast<LocalVar*>(rawArray)->GetExpr())->GetSize();
    auto newExpr = builder.CreateExpression<RawArrayInitByValue>(
        loc, builder.GetUnitTy(), rawArray, size, initVal->GetResult(), parent);
    apply.ReplaceWith(*newExpr);
    // Apply's result is replaced with RawArrayInitByValue's result by `apply.ReplaceWith(*newExpr)`, that's illegal,
    // so we need to replace it with the raw array
    newExpr->GetResult()->ReplaceWith(*rawArray, parent->GetParentBlockGroup());

    // 3. remove the old lambda if it has no other users
    auto users = lambda->GetResult()->GetUsers();
    if (users.empty()) {
        lambda->RemoveSelfFromBlock();
    } else if (users.size() == 1 && users[0]->GetExprKind() == ExprKind::DEBUGEXPR) {
        users[0]->RemoveSelfFromBlock();
        lambda->RemoveSelfFromBlock();
    }
}

Intrinsic* ArrayLambdaOpt::CheckCanRewriteZeroValue(const Expression& expr) const
{
    auto init = DynamicCast<RawArrayInitByValue*>(&expr);
    if (init == nullptr) {
        return nullptr;
    }
    auto initValue = DynamicCast<LocalVar*>(init->GetInitValue());
    if (initValue == nullptr) {
        return nullptr;
    }
    auto intrinsic = DynamicCast<Intrinsic*>(initValue->GetExpr());
    if (intrinsic == nullptr) {
        return nullptr;
    }
    if (intrinsic->GetIntrinsicKind() != IntrinsicKind::OBJECT_ZERO_VALUE) {
        return nullptr;
    }

    return intrinsic;
}

void ArrayLambdaOpt::RewriteZeroValue(RawArrayInitByValue& init, Intrinsic& zeroVal) const
{
    /** cangjie code:
            Array<Int64>(10000, repeat : unsafe { zeroValue<Int64>() })
        before optimization:
            %0: Int64 = Constant(10000i)
            %1: RawArray<Int64>& = RawArrayAllocate(Int64, %0)
            %2: Int64 = Intrinsic(zeroValue<Int64>)
            %3: Unit = RawArrayInitByValue(%1, %0, %2)  // address, size, init value
        after optimization:
            %0: Int64 = Constant(10000i)
            %1: RawArray<Int64>& = RawArrayAllocate(Int64, %0)
    */
    CJC_ASSERT(init.GetResult()->GetUsers().empty());
    init.RemoveSelfFromBlock();

    auto users = zeroVal.GetResult()->GetUsers();
    if (users.empty()) {
        zeroVal.RemoveSelfFromBlock();
    } else if (users.size() == 1 && users[0]->GetExprKind() == ExprKind::DEBUGEXPR) {
        users[0]->RemoveSelfFromBlock();
        zeroVal.RemoveSelfFromBlock();
    }
}
