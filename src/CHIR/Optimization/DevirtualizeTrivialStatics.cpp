// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
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
#include "cangjie/CHIR/Optimization/BlockGroupCopyHelper.h"
#include "cangjie/Mangle/CHIRManglingUtils.h"
namespace Cangjie::CHIR {
Block* GetEntryBlock(const BlockGroup& oldBG, std::vector<Block*>& blocks);

DevirtualizeTrivialStatics::DevirtualizeTrivialStatics(
    TypeAnalysisWrapper* typeAnalysisWrapper)
    : analysisWrapper(typeAnalysisWrapper)
{
}

void DevirtualizeTrivialStatics::RunOnFuncs(const std::vector<Function*>& funcs, CHIRBuilder& builder, bool isDebug)
{
    rewriteInfos.clear();
    for (auto func : funcs) {
        RunOnFunc(func, builder);
    }
    RewriteToApply(builder, rewriteInfos, isDebug);
    InstantiateFuncIfPossible(builder, rewriteInfos);
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
    rttiStatic->Dump();
    rttiStatic->GetRTTIType()->Dump();
    clazz->Dump();
    if (rttiStatic->GetRTTIType()->StripAllRefs() == clazz) {
        auto args = invoke->GetArgs();
        std::vector<Value*> applyArgs{};
        applyArgs.insert(applyArgs.end(), args.begin(), args.end());
        auto thisTypeRef = invoke->GetThisType();
        std::vector<Type*> paramTys;
        for (auto&& arg : applyArgs) {
            paramTys.push_back(arg->GetType());
        }

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
    invoke->Dump();
    value->Dump();
    return;
}

void DevirtualizeTrivialStatics::RunOnFunc(const Function* func, CHIRBuilder& builder)
{
    auto result = analysisWrapper->CheckFuncResult(func);
    if (result == nullptr && frozenStates.count(func) != 0) {
        result = frozenStates.at(func).get();
    }
    if (result == nullptr) {
        return;
    }
    const auto actionBeforeVisitExpr =
        [this, &builder](const TypeDomain& analysisResult, Expression* expr, size_t) {
        (void)analysisResult;
        if (expr->GetExprKind() == ExprKind::INVOKESTATIC) {
            RewriteInvokeStatic(expr, builder, rewriteInfos);
            return;
        }
    };

    const auto actionAfterVisitExpr = [](const TypeDomain&, Expression*, size_t) {};
    const auto actionOnTerminator = [](const TypeDomain&, Terminator*, std::optional<Block*>) {};
    result->VisitWith(actionBeforeVisitExpr, actionAfterVisitExpr, actionOnTerminator);
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

        if ((rewriteInfo->thisType->IsStruct() || rewriteInfo->thisType->IsEnum()) &&
            rewriteInfo->realCallee->Get<WrappedRawMethod>()) {
            thisType = rewriteInfo->thisType;
            bool isMutFunc =
                realFunc->TestAttr(Attribute::MUT) || IsConstructor(*realFunc) || IsInstanceVarInit(*realFunc);
            if (isMutFunc) {
                thisType = builder.GetType<RefType>(thisType);
            }
            realFunc = rewriteInfo->realCallee->Get<WrappedRawMethod>();
        }
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

const std::vector<Function*>& DevirtualizeTrivialStatics::GetFrozenInstFuns() const
{
    return frozenInstFuns;
}

void DevirtualizeTrivialStatics::AppendFrozenFuncState(
    const Function* func, std::unique_ptr<Results<TypeDomain>> analysisRes)
{
    frozenStates.emplace(func, std::move(analysisRes));
}

static std::string CreateInstFuncMangleName(const std::string& oriIdentifer, const Apply& apply, CHIRBuilder& builder)
{
    // 1. get type args
    std::vector<Type*> genericTypes;
    auto func = StaticCast<Function*>(apply.GetCallee());
    if (auto customDef = func->GetParentCustomTypeDef(); customDef != nullptr && customDef->IsGenericDef()) {
        auto funcInCustomType = apply.GetInstParentCustomTyOfCallee(builder);
        while (funcInCustomType->IsRef()) {
            funcInCustomType = StaticCast<RefType*>(funcInCustomType)->GetBaseType();
        }
        genericTypes = funcInCustomType->GetTypeArgs();
    }
    auto funcArgs = apply.GetInstantiatedTypeArgs();
    if (!funcArgs.empty()) {
        genericTypes.insert(genericTypes.end(), funcArgs.begin(), funcArgs.end());
    }
    // 2. get mangle
    return CHIRMangling::GenerateInstantiateFuncMangleName(oriIdentifer, genericTypes);
}

void DevirtualizeTrivialStatics::InstantiateFuncIfPossible(CHIRBuilder& builder, std::vector<RewriteInfo>& rewriteInfoList)
{
    for (auto rewriteInfo = rewriteInfoList.rbegin(); rewriteInfo != rewriteInfoList.rend(); ++rewriteInfo) {
        auto callee = rewriteInfo->realCallee;
        if (!callee->IsFuncWithBody() || !callee->IsInGenericContext() || callee->Get<WrappedRawMethod>() != nullptr) {
            continue;
        }
        auto apply = rewriteInfo->newApply;
        std::vector<Type*> parameterType;
        for (auto param : apply->GetArgs()) {
            parameterType.emplace_back(param->GetType());
        }
        auto retType = apply->GetResultType();
        auto instFuncType = builder.GetType<FuncType>(parameterType, retType);

        if (instFuncType->IsGenericRelated()) {
            continue;
        }
        // 2. create new inst func if needed
        auto newId = CreateInstFuncMangleName(callee->GetIdentifierWithoutPrefix(), *apply, builder);
        Function* newFunc;
        if (frozenInstFuncMap.count(newId) != 0) {
            newFunc = frozenInstFuncMap.at(newId);
        } else {
            newFunc = builder.CreateFunction(instFuncType, newId,
                callee->GetSrcCodeIdentifier(), callee->GetRawMangledName(), callee->GetPackageName());
            newFunc->SetDebugLocation(callee->GetDebugLocation());
            newFunc->AppendAttributeInfo(callee->GetAttributeInfo());
            newFunc->DisableAttr(Attribute::GENERIC);
            if (!apply->GetInstantiatedTypeArgs().empty()) {
                newFunc->EnableAttr(Attribute::GENERIC_INSTANTIATED);
            }
            newFunc->Set<LinkTypeInfo>(Linkage::INTERNAL);
            newFunc->SetGenericDecl(*callee);

            auto oriBlockGroup = callee->GetBody();
            BlockGroupCopyHelper helper(builder);
            helper.GetInstMapFromApply(*apply, newFunc);
            auto newBody = builder.CreateBlockGroup(*newFunc);
            newFunc->InitBody(*newBody);
            auto [newBlocks, newBlockGroupRetValue] = helper.CloneBlockGroup(*oriBlockGroup, *newBody);
            auto funcEntry = GetEntryBlock(*oriBlockGroup, newBlocks);
            newBody->SetEntryBlock(funcEntry);
            newFunc->SetReturnValue(*newBlockGroupRetValue);

            std::vector<Value*> args;
            CJC_ASSERT(parameterType.size() == callee->GetParams().size());
            std::unordered_map<Value*, Value*> paramMap;
            for (size_t i = 0; i < parameterType.size(); i++) {
                auto arg = builder.CreateParameter(
                    parameterType[i], callee->GetParam(i)->GetDebugLocation(), *newFunc);
                args.push_back(arg);
                paramMap.emplace(callee->GetParam(i), arg);
            }
            helper.ReplaceExprOperands(newBlocks, paramMap);

            FixCastProblemAfterInst(newBlocks, builder);
            newFunc->SetReturnValue(*newBlockGroupRetValue);
            frozenInstFuns.push_back(newFunc);
            frozenInstFuncMap[newId] = newFunc;
        }
        // replace apply callee with new inst func
        auto applyParent = apply->GetParentBlock();
        auto loc = apply->GetDebugLocation();
        auto instApply = builder.CreateExpression<Apply>(
            loc, retType, newFunc, FuncCallContext{.args = apply->GetArgs()}, applyParent);
        apply->ReplaceWith(*instApply);
    }
}

bool DevirtualizeTrivialStatics::CheckFuncHasInvoke(const BlockGroup& bg)
{
    std::vector<Block*> blocks = bg.GetBlocks();
    for (auto bb : blocks) {
        auto exprs = bb->GetNonTerminatorExpressions();
        for (size_t i = 0; i < exprs.size(); ++i) {
            if (exprs[i]->GetExprKind() == ExprKind::LAMBDA) {
                if (CheckFuncHasInvoke(*StaticCast<Lambda*>(exprs[i])->GetBody())) {
                    return true;
                }
            }
            if (exprs[i]->GetExprKind() == ExprKind::INVOKESTATIC) {
                return true;
            }
        }
    }
    return false;
}

bool DevirtualizeTrivialStatics::CheckAllGenericTypeVisible(
    const Type& type, const std::unordered_set<const GenericType*>& visibleSet)
{
    if (type.IsGeneric() && visibleSet.count(StaticCast<const GenericType*>(&type)) == 0) {
        return false;
    }
    for (auto t : type.GetTypeArgs()) {
        if (!CheckAllGenericTypeVisible(*t, visibleSet)) {
            return false;
        }
    }
    return true;
}

std::vector<Function*> DevirtualizeTrivialStatics::CollectContainInvokeExprFuncs(const Ptr<const Package>& package)
{
    std::vector<Function*> funcs;
    // Collect functions that contain the invoke statement.
    for (auto func : package->GetGlobalFuncsWithBody()) {
        if (CheckFuncHasInvoke(*func->GetBody())) {
            funcs.emplace_back(func);
        }
    }
    return funcs;
}
}
