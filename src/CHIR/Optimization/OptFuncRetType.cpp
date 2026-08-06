// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/Optimization/OptFuncRetType.h"
#include "cangjie/CHIR/Utils/CHIRCasting.h"
#include "cangjie/CHIR/Utils/Utils.h"
#include "cangjie/Utils/CastingTemplate.h"
#include "cangjie/Utils/CheckUtils.h"

using namespace Cangjie::CHIR;

namespace {
void RemoveOldRetValue(LocalVar& oldRet)
{
    /*  remove this kind of code:
        %1: Unit& = Allocate(Unit)  // old ret value
        ...
        %2: Unit = Constant(Unit)
        %3: Unit& = Store(%2, %1)

        we are not sure if the Store is the only user of the old ret value, if there is Load,
        we can't remove the old ret value. The following condition is a little simple, but it's enough for now.
    */
    for (auto user : oldRet.GetUsers()) {
        if (user->GetExprKind() != ExprKind::STORE) {
            return;
        }
    }
    for (auto user : oldRet.GetUsers()) {
        auto store = Cangjie::StaticCast<Store*>(user);
        if (auto unitVal = Cangjie::DynamicCast<LocalVar*>(store->GetValue())) {
            /*  we only care about the following code:
                %1: Unit& = Allocate(Unit)  // old ret value
                ...
                %2: Unit = Constant(Unit)
                %3: Unit& = Store(%2, %1)

                we can't remove store's operand if in following code:
                %1: Unit& = Allocate(Unit)  // old ret value
                ...
                %2: Unit = Apply(xxx, ...)
                %3: Unit& = Store(%2, %1)

                there may be other safety cases that you can remove the store's operand, you can add them here.
            */
            if (Cangjie::Is<Constant>(unitVal->GetExpr())) {
                unitVal->GetExpr()->RemoveSelfFromBlock();
            }
        }
        store->RemoveSelfFromBlock();
    }
    oldRet.GetExpr()->RemoveSelfFromBlock();
}
}

OptFuncRetType::OptFuncRetType(Package& package, CHIRBuilder& builder) : package(package), builder(builder)
{
}

void OptFuncRetType::Unit2Void()
{
    // 1. collect all global functions that should return Void
    for (auto func : package.GetGlobalFunctions()) {
        if (!ReturnTypeShouldBeVoid(*func)) {
            continue;
        }
        CJC_ASSERT(func->GetReturnType()->IsUnit());
        LocalVar* oldRet = nullptr;
        // 2. change the return type to Void
        if (func->IsFuncWithBody()) {
            oldRet = func->GetReturnValue();
            CJC_NULLPTR_CHECK(oldRet);
        }
        func->ReplaceReturnValue(nullptr, builder);
        // 3. remove the old ret value, just for clean code
        if (oldRet != nullptr) {
            RemoveOldRetValue(*oldRet);
        }
        // 4. replace all call sites with the new return type
        for (auto user : func->GetUsers()) {
            auto apply = StaticCast<ApplyBase*>(user);
            CJC_ASSERT(apply->GetCallee() == func);
            auto funcCallContext = FuncCallContext{
                .args = apply->GetArgs(),
                .instTypeArgs = apply->GetInstantiatedTypeArgs(),
                .thisType = apply->GetThisType()
            };
            Expression* newApply = nullptr;
            if (apply->IsTerminator()) {
                auto& awe = StaticCast<ApplyWithException&>(*apply);
                newApply = builder.CreateExpression<ApplyWithException>(awe.GetDebugLocation(),
                    func->GetReturnType(), awe.GetCallee(), funcCallContext, awe.GetSuccessBlock(),
                    awe.GetErrorBlock(), awe.GetParentBlock());
            } else {
                auto& a = StaticCast<Apply&>(*apply);
                auto created = builder.CreateExpression<Apply>(
                    a.GetDebugLocation(), func->GetReturnType(), a.GetCallee(), funcCallContext, a.GetParentBlock());
                if (a.IsSuperCall()) {
                    created->SetSuperCall();
                }
                newApply = created;
            }
            apply->ReplaceWith(*newApply);
        }
    }
}