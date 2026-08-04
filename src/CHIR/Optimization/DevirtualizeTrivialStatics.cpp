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

static void RewriteInvokeStatic(Expression* expr,
    CHIRBuilder& builder,
    std::vector<DevirtualizeTrivialStatics::RewriteInfo>& rewriteInfos)
{
    auto invoke = DynamicCast<InvokeStatic*>(expr);
    if (!invoke) { return; }
    if (!invoke->GetInstantiatedTypeArgs().empty()) { return; }
    auto rttiLocalVar = DynamicCast<LocalVar>(invoke->GetRTTIValue());
    if (!rttiLocalVar) { return; }
    auto value = rttiLocalVar->GetExpr();
    if (!value) { return; }
    auto rttiStatic = DynamicCast<GetRTTIStatic*>(value);
    if (!rttiStatic) { return; }
    auto thisType = invoke->GetThisType()->StripAllRefs();
    if (!thisType->IsClass()) {
        return;
    }
    auto clazz = StaticCast<ClassType*>(thisType);
    if (rttiStatic->GetRTTIType()->StripAllRefs() == clazz) {
        auto thisTypeRef = invoke->GetThisType();
        
        std::vector<Type*> funcInstTypeArgs;
        auto expectedFunc = clazz->GetClassDef()->GetExpectedFunc(
            invoke->GetMethodName(),
            *invoke->GetMethodType(),
            true, {}, funcInstTypeArgs, builder, false);
        if (!expectedFunc) { return; }

        rewriteInfos.push_back({
            .invokeStatic = invoke,
            .realCallee = expectedFunc,
            .thisType = thisTypeRef
        });

        return;
    }
    return;
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
        Type* thisType = builder.GetType<RefType>(rewriteInfo->thisType);
        auto& realFunc = rewriteInfo->realCallee;

        auto args = invoke->GetArgs();
        auto instRetTy = invoke->GetResultType();

        auto loc = invoke->GetDebugLocation();
        auto apply = builder.CreateExpression<Apply>(loc, instRetTy, realFunc, FuncCallContext{
            .args = args,
            .instTypeArgs = rewriteInfo->typeArgs,
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
