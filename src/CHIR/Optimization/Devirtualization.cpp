// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/Optimization/Devirtualization.h"

#include <optional>
#include <unordered_set>

#include "cangjie/CHIR/Analysis/DevirtualizationInfo.h"
#include "cangjie/CHIR/Analysis/Utils.h"
#include "cangjie/CHIR/IR/Type/ExtendDef.h"
#include "cangjie/CHIR/IR/Type/Type.h"
#include "cangjie/CHIR/Utils/UserDefinedType.h"
#include "cangjie/CHIR/Utils/Utils.h"
#include "cangjie/CHIR/Optimization/BlockGroupCopyHelper.h"
#include "cangjie/Mangle/CHIRManglingUtils.h"
namespace Cangjie::CHIR {
namespace {
static const std::unordered_map<BinaryExprKind, FuncInfo> BINARY_FUNC_MAP = {
    {BinaryExprKind::GT, FuncInfo(">", ANY_TYPE, {ANY_TYPE}, ANY_TYPE, CORE_PACKAGE_NAME)},
    {BinaryExprKind::LT, FuncInfo("<", ANY_TYPE, {ANY_TYPE}, ANY_TYPE, CORE_PACKAGE_NAME)},
    {BinaryExprKind::GE, FuncInfo(">=", ANY_TYPE, {ANY_TYPE}, ANY_TYPE, CORE_PACKAGE_NAME)},
    {BinaryExprKind::LE, FuncInfo("<=", ANY_TYPE, {ANY_TYPE}, ANY_TYPE, CORE_PACKAGE_NAME)},
    {BinaryExprKind::EQUAL, FuncInfo("==", ANY_TYPE, {ANY_TYPE}, ANY_TYPE, CORE_PACKAGE_NAME)},
    {BinaryExprKind::NOTEQUAL, FuncInfo("!=", ANY_TYPE, {ANY_TYPE}, ANY_TYPE, CORE_PACKAGE_NAME)},
};

std::vector<CustomTypeDef*> CollectAllDefs(const Type& type, CHIRBuilder& builder)
{
    std::vector<CustomTypeDef*> defs;
    if (auto customType = DynamicCast<const CustomType*>(&type)) {
        auto def = customType->GetCustomTypeDef();
        const auto& extends = def->GetExtends();
        defs.reserve(extends.size() + 1);
        defs.emplace_back(def);
        defs.insert(defs.end(), extends.begin(), extends.end());
    } else {
        auto builtinType = StaticCast<const BuiltinType*>(&type);
        const auto& extends = builtinType->GetExtends(&builder);
        defs.reserve(extends.size());
        defs.insert(defs.end(), extends.begin(), extends.end());
    }
    return defs;
}

Function* SearchRealCalleeInVtable(const Type& type, const InvokeBase& invoke, CHIRBuilder& builder)
{
    std::vector<CustomTypeDef*> allDefs = CollectAllDefs(type, builder);
    auto srcParentTypeInRT = invoke.GetInstSrcParentCustomTypeOfMethod(builder);
    std::unordered_map<const GenericType*, Type*> replaceTable;
    if (auto customType = DynamicCast<CustomType*>(&type)) {
        replaceTable = GetInstMapFromCurDefAndExDefToCurType(*customType);
    }
    for (auto def : allDefs) {
        for (const auto& vtable : def->GetDefVTable().GetTypeVTables()) {
            auto srcParentTypeInVtable = vtable.GetSrcParentType();
            if (srcParentTypeInVtable->GetClassDef() != srcParentTypeInRT->GetClassDef()) {
                continue;
            }
            auto instSrcParentTypeInVtable = ReplaceRawGenericArgType(*srcParentTypeInVtable, replaceTable, builder);
            if (instSrcParentTypeInVtable != srcParentTypeInRT) {
                continue;
            }
            auto methods = vtable.GetVirtualMethods();
            CJC_ASSERT(methods.size() > invoke.GetVirtualMethodOffset());
            auto tempTarget = methods[invoke.GetVirtualMethodOffset()].GetVirtualMethod();
            CJC_NULLPTR_CHECK(tempTarget);
            if (!tempTarget->IsPureAbstract()) {
                if (auto rawMethod = tempTarget->Get<WrappedRawMethod>()) {
                    return rawMethod;
                } else {
                    return tempTarget;
                }
            }
            break;
        }
    }
    return nullptr;
}

bool HasUnknownGenericType(const Type& knownType, const Type& unknownType)
{
    std::unordered_set<const GenericType*> knownGenericTypes;
    knownType.VisitTypeRecursively([&knownGenericTypes](const Type& type) {
        if (type.IsGeneric()) {
            knownGenericTypes.insert(StaticCast<const GenericType*>(&type));
        }
        return true;
    });
    bool hasUnknownGenericType = false;
    unknownType.VisitTypeRecursively([&knownGenericTypes, &hasUnknownGenericType](const Type& type) {
        if (auto genericType = DynamicCast<const GenericType*>(&type)) {
            if (knownGenericTypes.count(genericType) == 0) {
                hasUnknownGenericType = true;
                return false;
            }
        }
        return true;
    });
    return hasUnknownGenericType;
}

std::pair<Value*, std::vector<TypeCast*>> CollectUpstreamTypeCasts(LocalVar& tempThisObj)
{
    std::vector<TypeCast*> castExprs;
    Value* thisValue = &tempThisObj;
    std::function<void(LocalVar&)> findThisValue = [&findThisValue, &castExprs, &thisValue](LocalVar& tempVar) {
        auto expr = tempVar.GetExpr();
        if (Is<Box>(expr) || Is<ClassStaticCast>(expr)) {
            castExprs.emplace_back(StaticCast<TypeCast*>(expr));
            thisValue = StaticCast<TypeCast*>(expr)->GetSourceValue();
            if (auto localVar = DynamicCast<LocalVar*>(thisValue)) {
                findThisValue(*localVar);
            }
        }
    };
    findThisValue(tempThisObj);
    return std::make_pair(thisValue, castExprs);
}

Block* GetEntryBlock(const BlockGroup& oldBG, std::vector<Block*>& blocks)
{
    auto oldBlocks = oldBG.GetBlocks();
    CJC_ASSERT(oldBlocks.size() == blocks.size());
    CJC_ASSERT(!blocks.empty());
    for (size_t i = 0; i < oldBlocks.size(); ++i) {
        if (oldBlocks[i] == oldBG.GetEntryBlock()) {
            return blocks[i];
        }
    }
    CJC_ABORT();
    return blocks.front();
}
}

Devirtualization::Devirtualization(
    TypeAnalysisWrapper* typeAnalysisWrapper, DevirtualizationInfo& devirtFuncInfo, CHIRBuilder& builder)
    : analysisWrapper(typeAnalysisWrapper), devirtFuncInfo(devirtFuncInfo), builder(builder)
{
}

void Devirtualization::RunOnFuncs(const std::vector<Function*>& funcs)
{
    rewriteInfos.clear();
    for (auto func : funcs) {
        RunOnFunc(func);
    }
    RewriteToApply(rewriteInfos);
}

void Devirtualization::RunOnFunc(const Function* func)
{
    auto result = analysisWrapper->CheckFuncResult(func);
    if (result == nullptr && frozenStates.count(func) != 0) {
        result = frozenStates.at(func).get();
    }
    if (result == nullptr) {
        return;
    }
    auto tryCollect = [this](const TypeDomain& state, InvokeBase* invoke) {
        auto object = invoke->GetObject();
        auto invokeAbsObject = state.CheckAbstractObjectRefBy(object);
        // Obtains the state information of the invoke operation object.
        auto resVal = state.CheckAbstractValue(invokeAbsObject);
        if (!resVal) {
            return;
        }
        auto realCallee = FindRealCallee(resVal, *invoke);
        if (realCallee == nullptr) {
            return;
        }
        /*
        * if subtype's generic args are more than its parent, a free generic type will be introduced:
        * inteface I {
        *   func foo()
        * }
        * class CA<T> <: I {
        *   public func foo() {}
        * }
        * func goo(a: I) {
        *   a.foo()
        * }
        * CA<T> is only candicate of foo call, but will introduce a free template args, skip this case.
        */
        auto srcParentType = invoke->GetInstSrcParentCustomTypeOfMethod(builder);
        auto genericSubType = realCallee->GetParentCustomTypeOrExtendedType();
        CJC_NULLPTR_CHECK(genericSubType);
        auto instSubType = GetInstSubType(*genericSubType, *srcParentType, builder);
        if (HasUnknownGenericType(*srcParentType, *instSubType)) {
            return;
        }
        rewriteInfos.emplace_back(RewriteInfo{invoke, realCallee, instSubType});
    };

    const auto actionBeforeVisitExpr = [&tryCollect](const TypeDomain& state, Expression* expr, size_t) {
        if (auto invoke = DynamicCast<Invoke*>(expr)) {
            tryCollect(state, invoke);
        }
    };

    const auto actionAfterVisitExpr = [](const TypeDomain&, Expression*, size_t) {};
    const auto actionOnTerminator = [&tryCollect](const TypeDomain& state, Expression* expr, std::optional<Block*>) {
        if (auto tryInvoke = DynamicCast<TryInvoke*>(expr)) {
            tryCollect(state, tryInvoke);
        }
    };
    result->VisitWith(actionBeforeVisitExpr, actionAfterVisitExpr, actionOnTerminator);
}

bool Devirtualization::RewriteToBuiltinOp(const RewriteInfo& info)
{
    std::optional<BinaryExprKind> targetBinaryKind;
    for (auto& it : BINARY_FUNC_MAP) {
        if (IsExpectedFunction(*info.realCallee, it.second)) {
            targetBinaryKind = it.first;
            break;
        }
    }
    if (!targetBinaryKind.has_value()) {
        return false;
    }
    auto args = info.invoke->GetArgs();
    if (args.size() != 2U) {
        return false;
    }
    Value* leftOp = args[0];
    Value* rightOp = args[1];
    std::vector<TypeCast*> leftCastExprs;
    // after other optimizations, the left operand may be casted to a non-primitive type,
    // we need to find the original one and remove the cast expressions
    if (auto localVar = DynamicCast<LocalVar*>(leftOp)) {
        std::tie(leftOp, leftCastExprs) = CollectUpstreamTypeCasts(*localVar);
    }
    if (!leftOp->GetType()->IsPrimitive() || !rightOp->GetType()->IsPrimitive()) {
        return false;
    }
    Block* nextBlock = nullptr;
    if (auto tryInvoke = DynamicCast<TryInvoke*>(info.invoke)) {
        nextBlock = tryInvoke->GetSuccessBlock();
    }
    auto loc = info.invoke->GetDebugLocation();
    auto retType = info.invoke->GetResultType();
    auto parent = info.invoke->GetParentBlock();
    auto binaryExpr =
        builder.CreateExpression<BinaryExpression>(loc, retType, *targetBinaryKind, leftOp, rightOp, parent);
    info.invoke->ReplaceWith(*binaryExpr);
    for (auto e : leftCastExprs) {
        if (e->GetResult()->GetUsers().empty()) {
            e->RemoveSelfFromBlock();
        }
    }
    if (nextBlock != nullptr) {
        auto goToExpr = builder.CreateTerminator<GoTo>(nextBlock, parent);
        goToExpr->MoveAfter(binaryExpr);
    }
    return true;
}

void Devirtualization::RewriteToApply(std::vector<RewriteInfo>& infos)
{
    auto needThisTypeRef = [](Function& func) {
        return func.TestAttr(Attribute::MUT) || func.IsConstructor() || func.IsInstanceVarInit();
    };
    for (auto rewriteInfo = infos.rbegin(); rewriteInfo != infos.rend(); ++rewriteInfo) {
        if (RewriteToBuiltinOp(*rewriteInfo)) {
            continue;
        }
        auto thisType = rewriteInfo->thisType;
        auto realFunc = rewriteInfo->realCallee;
        if (thisType->IsReferenceType() || (thisType->IsValueType() && needThisTypeRef(*realFunc))) {
            thisType = builder.GetType<RefType>(thisType);
        }

        auto invoke = rewriteInfo->invoke;
        auto args = invoke->GetArgs();
        auto instRetTy = invoke->GetResultType();
        auto parent = invoke->GetParentBlock();
        std::vector<TypeCast*> objCastExprs;
        if (auto localVar = DynamicCast<LocalVar*>(args[0])) {
            std::tie(args[0], objCastExprs) = CollectUpstreamTypeCasts(*localVar);
        }
        auto typecastRes = TypeCastOrBoxIfNeeded(*args[0], *thisType, builder, *parent, INVALID_LOCATION);
        if (typecastRes != args[0]) {
            StaticCast<LocalVar*>(typecastRes)->GetExpr()->MoveBefore(invoke);
            args[0] = typecastRes;
        }
        auto loc = invoke->GetDebugLocation();
        auto context = FuncCallContext {
            .args = args,
            .instTypeArgs = rewriteInfo->invoke->GetInstantiatedTypeArgs(),
            .thisType = thisType
        };
        realFunc = CreateInstFuncIfPossible(realFunc, context, *instRetTy);
        Expression* newCall = nullptr;
        if (auto tryInvoke = DynamicCast<TryInvoke*>(invoke)) {
            newCall = builder.CreateExpression<TryApply>(
                loc, instRetTy, realFunc, context, tryInvoke->GetSuccessBlock(), tryInvoke->GetErrorBlock(), parent);
        } else {
            newCall = builder.CreateExpression<Apply>(loc, instRetTy, realFunc, context, parent);
        }
        invoke->ReplaceWith(*newCall);
        for (auto e : objCastExprs) {
            if (e->GetResult()->GetUsers().empty()) {
                e->RemoveSelfFromBlock();
            }
        }
    }
}

const std::vector<Function*>& Devirtualization::GetFrozenInstFuns() const
{
    return frozenInstFuns;
}

void Devirtualization::AppendFrozenFuncState(const Function* func, std::unique_ptr<Results<TypeDomain>> analysisRes)
{
    frozenStates.emplace(func, std::move(analysisRes));
}

static std::string CreateInstFuncMangleName(
    const std::string& oriIdentifer, Function& func, const FuncCallContext& context, CHIRBuilder& builder)
{
    // 1. get type args
    std::vector<Type*> genericTypes;
    if (auto customDef = func.GetParentCustomTypeDef(); customDef != nullptr && customDef->IsGenericDef()) {
        auto funcInCustomType = GetInstParentCustomTyOfCallee(func, context.args, context.thisType, builder);
        CJC_NULLPTR_CHECK(funcInCustomType);
        genericTypes = funcInCustomType->GetTypeArgs();
    }
    if (!context.instTypeArgs.empty()) {
        genericTypes.insert(genericTypes.end(), context.instTypeArgs.begin(), context.instTypeArgs.end());
    }
    // 2. get mangle
    return CHIRMangling::GenerateInstantiateFuncMangleName(oriIdentifer, genericTypes);
}

Function* Devirtualization::CreateInstFuncIfPossible(Function* func, FuncCallContext& context, Type& retType)
{
    auto canBeFullyInstantiated = [](const Type& thisType, const std::vector<Type*>& typeArgs) {
        if (thisType.IsGenericRelated()) {
            return false;
        }
        for (auto typeArg : typeArgs) {
            if (typeArg->IsGenericRelated()) {
                return false;
            }
        }
        return true;
    };
    if (!func->IsInGenericContext() || !canBeFullyInstantiated(*context.thisType, context.instTypeArgs)) {
        return func;
    }
    auto newId = CreateInstFuncMangleName(func->GetIdentifierWithoutPrefix(), *func, context, builder);
    Function* newFunc = nullptr;
    if (frozenInstFuncMap.count(newId) != 0) {
        newFunc = frozenInstFuncMap.at(newId);
    } else {
        BlockGroupCopyHelper helper(builder);
        helper.GetInstMapFromFuncCall(*func, context.thisType, context.instTypeArgs, context.args);
        std::vector<Type*> parameterType;
        for (auto arg : context.args) {
            parameterType.emplace_back(arg->GetType());
        }
        auto instFuncType = builder.GetType<FuncType>(parameterType, &retType);
        newFunc = builder.CreateFunction(
            instFuncType, newId, func->GetSrcCodeIdentifier(), "", func->GetPackageName());
        newFunc->SetDebugLocation(func->GetDebugLocation());
        newFunc->AppendAttributeInfo(func->GetAttributeInfo());
        newFunc->DisableAttr(Attribute::GENERIC);
        if (!context.instTypeArgs.empty()) {
            newFunc->EnableAttr(Attribute::GENERIC_INSTANTIATED);
        }
        newFunc->Set<LinkTypeInfo>(Linkage::INTERNAL);
        newFunc->SetGenericDecl(*func);

        auto oriBlockGroup = func->GetBody();
        auto newBody = builder.CreateBlockGroup(*newFunc);
        newFunc->InitBody(*newBody);
        auto [newBlocks, newBlockGroupRetValue] = helper.CloneBlockGroup(*oriBlockGroup, *newBody);
        auto funcEntry = GetEntryBlock(*oriBlockGroup, newBlocks);
        newBody->SetEntryBlock(funcEntry);
        newFunc->SetReturnValue(*newBlockGroupRetValue);

        CJC_ASSERT(parameterType.size() == func->GetParams().size());
        std::unordered_map<Value*, Value*> paramMap;
        for (size_t i = 0; i < parameterType.size(); i++) {
            auto arg = builder.CreateParameter(parameterType[i], func->GetParam(i)->GetDebugLocation(), *newFunc);
            paramMap.emplace(func->GetParam(i), arg);
        }
        helper.ReplaceExprOperands(newBlocks, paramMap);

        FixCastProblemAfterInst(newBlocks, builder);
        newFunc->SetReturnValue(*newBlockGroupRetValue);
        frozenInstFuns.push_back(newFunc);
        frozenInstFuncMap[newId] = newFunc;
    }
    // Instantiated callee no longer needs call-site type args.
    context.instTypeArgs.clear();
    context.thisType = nullptr;
    return newFunc;
}

std::vector<Type*> Devirtualization::CollectAllSubTypes(ClassType& specific) const
{
    std::vector<Type*> allSubTypes{&specific};
    std::unordered_set<Type*> visited{&specific};
    std::vector<ClassType*> workList{&specific};
    while (!workList.empty()) {
        auto* cur = workList.back();
        workList.pop_back();
        auto it = devirtFuncInfo.subtypeMap.find(cur->GetClassDef());
        if (it == devirtFuncInfo.subtypeMap.end()) {
            continue;
        }
        for (auto& inheritInfo : it->second) {
            auto [matched, replaceTable] = inheritInfo.parentType->CalculateGenericTyMapping(*cur);
            if (!matched) {
                continue;
            }
            auto subtype = ReplaceRawGenericArgType(*inheritInfo.subType, replaceTable, builder);
            if (!visited.insert(subtype).second) {
                continue;
            }
            allSubTypes.emplace_back(subtype);
            if (subtype->CanBeInherited()) {
                workList.emplace_back(StaticCast<ClassType*>(subtype));
            }
        }
    }
    return allSubTypes;
}

Function* Devirtualization::FindRealCallee(const TypeValue* typeState, const InvokeBase& invoke) const
{
    auto typeStateKind = typeState->GetTypeKind();
    auto specificType = typeState->GetSpecificType();
    if (!specificType->CanBeInherited() || typeStateKind == DevirtualTyKind::EXACTLY) {
        // 1. final class, struct, enum or builtin type
        // 2. exact type, even if open class type
        auto target = SearchRealCalleeInVtable(*specificType, invoke, builder);
        CJC_NULLPTR_CHECK(target);
        return target;
    } else {
        // 3. open class type, there may be only one implementation, find it
        auto specificDef = StaticCast<ClassType*>(specificType)->GetClassDef();
        if (specificDef->CanBeInherited() && !devirtFuncInfo.CheckCustomTypeInternal(*specificDef)) {
            // skip open class with external linkage, it might have sub type in another package
            return nullptr;
        }
        auto subTypes = CollectAllSubTypes(*StaticCast<ClassType*>(specificType));
        std::unordered_set<Function*> targets;
        for (auto subtype : subTypes) {
            auto target = SearchRealCalleeInVtable(*subtype, invoke, builder);
            if (target != nullptr) {
                targets.emplace(target);
            }
        }
        if (targets.size() == 1) {
            return *targets.begin();
        }
        return nullptr;
    }
}
}
