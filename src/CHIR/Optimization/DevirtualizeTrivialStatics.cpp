// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/Optimization/DevirtualizeTrivialStatics.h"

#include "cangjie/CHIR/Optimization/DeadCodeElimination.h"
#include "cangjie/CHIR/Analysis/DevirtualizationInfo.h"
#include "cangjie/CHIR/Analysis/Engine.h"
#include "cangjie/CHIR/Analysis/Utils.h"
#include "cangjie/CHIR/IR/Annotation.h"
#include "cangjie/CHIR/IR/Type/Type.h"
#include "cangjie/CHIR/Utils/UserDefinedType.h"
#include "cangjie/CHIR/Utils/Utils.h"
#include "cangjie/CHIR/Utils/Visitor/Visitor.h"
#include "cangjie/CHIR/Optimization/BlockGroupCopyHelper.h"
#include "cangjie/Mangle/CHIRManglingUtils.h"
namespace Cangjie::CHIR {

void DevirtualizeTrivialStatics::RunOnPackage(const Package& package, CHIRBuilder& builder, bool isDebug)
{
    rewriteInfos.clear();
    for (auto func : package.GetGlobalFuncsWithBody()) {
        RunOnFunc(func, builder);
    }
    RewriteToApply(builder, rewriteInfos, isDebug);
}

static Function* ResolveStaticCallee(InvokeStatic& invoke, ClassType& clazz, CHIRBuilder& builder)
{
    // Resolve the callee via vtable using the actual argument types.
    // GetExpectedFunc(methodName, GetMethodType()) is incorrect when the same
    // generic interface method is instantiated multiple times on one type
    // (e.g. I<Int64>.f1 and I<String>.f1): both share name "f1" and a generic
    // method type, so the first overload is always chosen and can create a
    // self-recursive Apply (infinite loop at -O2).
    //
    // Instantiated type args are intentionally empty: callers with non-empty
    // instTypeArgs are filtered out before this helper is invoked.
    std::vector<Type*> instParamTypes;
    for (auto arg : invoke.GetArgs()) {
        instParamTypes.emplace_back(arg->GetType());
    }
    auto instFuncType = builder.GetType<FuncType>(instParamTypes, builder.GetUnitTy());
    FuncCallType funcCallType{invoke.GetMethodName(), instFuncType, {}};
    auto candidates = GetFuncIndexInVTable(clazz, funcCallType, builder);
    for (auto& cand : candidates) {
        if (cand.instance == nullptr || cand.instance->IsPureAbstract()) {
            continue;
        }
        auto expectedFunc = cand.instance;
        // Prefer the original method over its boxing wrapper so Apply args stay unboxed.
        // ParamTypeIsEquivalent tolerates box-ref differences during matching, but
        // RewriteToApply reuses invoke args without TypeCastOrBoxIfNeeded; only unwrap
        // when every argument type exactly matches the raw parameter type.
        if (auto rawFunc = expectedFunc->Get<WrappedRawMethod>()) {
            auto rawParams = rawFunc->GetFuncType()->GetParamTypes();
            auto args = invoke.GetArgs();
            if (rawParams.size() != args.size()) {
                return nullptr;
            }
            for (size_t i = 0; i < args.size(); ++i) {
                if (args[i]->GetType() != rawParams[i]) {
                    return nullptr;
                }
            }
            expectedFunc = rawFunc;
        }
        return expectedFunc;
    }
    return nullptr;
}

static void RewriteInvokeStatic(Expression* expr,
    CHIRBuilder& builder,
    std::vector<DevirtualizeTrivialStatics::RewriteInfo>& rewriteInfos)
{
    auto invoke = DynamicCast<InvokeStatic*>(expr);
    if (!invoke) { return; }
    // RewriteToApply builds FuncCallContext without instTypeArgs. Generic static
    // calls must be skipped here; otherwise Apply would lose instantiation info
    // and can ICE
    if (!invoke->GetInstantiatedTypeArgs().empty()) { return; }
    auto rttiLocalVar = DynamicCast<LocalVar>(invoke->GetRTTIValue());
    if (!rttiLocalVar) { return; }
    auto value = rttiLocalVar->GetExpr();
    if (!value) { return; }
    auto rttiStatic = DynamicCast<GetRTTIStatic*>(value);
    if (!rttiStatic) { return; }
    auto thisType = invoke->GetThisType()->StripAllRefs();
    if (!thisType->IsClass() || thisType->IsGenericRelated()) {
        return;
    }

    auto clazz = StaticCast<ClassType*>(thisType);
    if (rttiStatic->GetRTTIType()->StripAllRefs() != clazz) {
        return;
    }

    auto expectedFunc = ResolveStaticCallee(*invoke, *clazz, builder);
    if (expectedFunc == nullptr) {
        return;
    }

    rewriteInfos.push_back({
        .invokeStatic = invoke,
        .realCallee = expectedFunc,
        .thisType = invoke->GetThisType()
    });
}

void DevirtualizeTrivialStatics::RunOnFunc(const Function* func, CHIRBuilder& builder)
{
    const auto actionBeforeVisitExpr =
        [this, &builder](Expression& expr) {
        if (expr.GetExprKind() == ExprKind::INVOKESTATIC) {
            RewriteInvokeStatic(&expr, builder, rewriteInfos);
            return VisitResult::CONTINUE;
        }
        return VisitResult::CONTINUE;
    };
    Visitor::Visit(*func, actionBeforeVisitExpr);
}

void DevirtualizeTrivialStatics::RewriteToApply(
    CHIRBuilder& builder, std::vector<RewriteInfo>& rewriteInfos, bool isDebug)
{
    for (auto rewriteInfo = rewriteInfos.rbegin(); rewriteInfo != rewriteInfos.rend(); ++rewriteInfo) {
        auto invoke = rewriteInfo->invokeStatic;
        auto parent = invoke->GetParentBlock();

        // get this type from rewrite info
        Type* thisType = rewriteInfo->thisType;
        auto& realFunc = rewriteInfo->realCallee;

        auto args = invoke->GetArgs();
        auto instRetTy = invoke->GetResultType();

        auto loc = invoke->GetDebugLocation();
        auto apply = builder.CreateExpression<Apply>(loc, instRetTy, realFunc, FuncCallContext{
            .args = args,
            .thisType = thisType}, invoke->GetParentBlock());
        rewriteInfo->newApply = apply;
        invoke->ReplaceWith(*apply);
        invoke->GetResult()->ReplaceWith(*apply->GetResult(), parent->GetParentBlockGroup());
        if (isDebug) {
            std::string message = "[DevirtualizeTrivialStatics] The function call to " + invoke->GetMethodName() +
                ToPosInfo(invoke->GetDebugLocation()) + " was optimized.";
            std::cout << message << std::endl;
        }
    }
}

}
