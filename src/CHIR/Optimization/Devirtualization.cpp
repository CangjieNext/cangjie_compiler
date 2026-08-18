// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/Optimization/Devirtualization.h"

#include <iostream>
#include <mutex>
#include <optional>
#include <unordered_set>

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

Devirtualization::Devirtualization(
    TypeAnalysisWrapper* typeAnalysisWrapper, DevirtualizationInfo& devirtFuncInfo)
    : analysisWrapper(typeAnalysisWrapper), devirtFuncInfo(devirtFuncInfo)
{
}

namespace {
Function* UnwrapVirtMethod(Function* func)
{
    if (func == nullptr) {
        return nullptr;
    }
    return func->Get<WrappedRawMethod>() != nullptr ? func->Get<WrappedRawMethod>() : func;
}

const CustomTypeDef* CanonicalDef(const CustomTypeDef* def)
{
    if (def == nullptr) {
        return nullptr;
    }
    return def->GetGenericDecl() != nullptr ? def->GetGenericDecl() : def;
}

std::string FormatFuncDesc(Function* func)
{
    if (func == nullptr) {
        return "<unknown>";
    }
    auto* parent = func->GetParentCustomTypeDef();
    std::string desc = func->GetPackageName();
    desc += "::";
    if (parent != nullptr) {
        desc += parent->GetSrcCodeIdentifier();
        desc += ".";
    }
    desc += func->GetSrcCodeIdentifier();
    if (auto* funcTy = func->GetFuncType()) {
        desc += " ";
        desc += funcTy->ToString();
    }
    const auto& loc = func->GetDebugLocation();
    if (!loc.GetAbsPath().empty() && loc.GetBeginPos().line != 0) {
        desc += "  (";
        desc += loc.GetAbsPath();
        desc += ":";
        desc += std::to_string(loc.GetBeginPos().line);
        desc += ")";
    }
    return desc;
}

std::string FormatCallSite(const Invoke& invoke)
{
    const auto& loc = invoke.GetDebugLocation();
    std::string path = loc.GetAbsPath();
    if (path.empty()) {
        path = loc.GetFileName();
    }
    if (path.empty()) {
        path = "<unknown>";
    }
    auto begin = loc.GetBeginPos();
    path += ":" + std::to_string(begin.line);
    if (begin.column != 0) {
        path += ":" + std::to_string(begin.column);
    }
    return path;
}

void PrintIfSplitVTableDevirt(CHIRBuilder& builder, const Invoke& invoke, Function& picked, Type& thisType,
    const Function* hostFunc, const TypeValue* typeState)
{
    auto* pickedRaw = UnwrapVirtMethod(&picked);
    const auto& methodName = invoke.GetMethodName();

    std::string hostDesc = "<unknown-host>";
    std::string hostAttrs;
    if (hostFunc != nullptr) {
        hostDesc = FormatFuncDesc(const_cast<Function*>(hostFunc));
        hostAttrs = " generic=";
        hostAttrs += hostFunc->TestAttr(Attribute::GENERIC) ? "1" : "0";
        hostAttrs += " genericInst=";
        hostAttrs += hostFunc->TestAttr(Attribute::GENERIC_INSTANTIATED) ? "1" : "0";
        hostAttrs += " inGenericCtx=";
        hostAttrs += hostFunc->IsInGenericContext() ? "1" : "0";
        if (auto* parentDef = hostFunc->GetParentCustomTypeDef()) {
            hostAttrs += " hostParent=";
            hostAttrs += parentDef->GetPackageName();
            hostAttrs += "::";
            hostAttrs += parentDef->GetSrcCodeIdentifier();
            if (parentDef->GetGenericDecl() != nullptr || parentDef->TestAttr(Attribute::GENERIC)) {
                hostAttrs += "(genericDef)";
            }
        }
    }
    std::string tyKind = "?";
    if (typeState != nullptr) {
        tyKind = typeState->GetTypeKind() == DevirtualTyKind::EXACTLY ? "EXACTLY" : "SUBTYPE_OF";
    }

    // Always log when FindRealCallee succeeds (stdout may be fully buffered under cjpm pipe).
    std::cout << "[DEVIRT] host={" << hostDesc << hostAttrs << "}"
              << " tyKind=" << tyKind
              << " invoke '" << methodName << "' @ " << FormatCallSite(invoke)
              << " -> " << FormatFuncDesc(pickedRaw) << "  thisType=" << thisType.ToString() << std::endl;

    // Highlight the zip4cj failure pattern: shared parent body specialized to NoCipher/NoEncrypter.
    if (methodName == "write" || methodName == "encryptData") {
        auto thisStr = thisType.ToString();
        bool suspiciousThis =
            thisStr.find("NoCipher") != std::string::npos || thisStr.find("NoEncrypter") != std::string::npos;
        bool suspiciousHost = hostFunc != nullptr &&
            (hostFunc->IsInGenericContext() || hostFunc->GetSrcCodeIdentifier() == "write" ||
                hostFunc->GetSrcCodeIdentifier() == "encryptData");
        if (suspiciousThis && suspiciousHost) {
            std::cout << "[DEVIRT SUSPECT] possible generic/shared-body rewrite: method='" << methodName
                      << "' picked='" << FormatFuncDesc(pickedRaw) << "' thisType=" << thisStr
                      << " host={" << hostDesc << hostAttrs << "}" << std::endl;
        }
    }

    auto* customTy = DynamicCast<CustomType*>(thisType.StripAllRefs());
    if (customTy == nullptr) {
        std::cout.flush();
        return;
    }
    auto* def = customTy->GetCustomTypeDef();
    if (def == nullptr) {
        std::cout.flush();
        return;
    }
    auto* genericDef = CanonicalDef(def);
    auto* callee = invoke.GetCallee();
    const auto* intendedParent = CanonicalDef(callee != nullptr ? callee->GetParentCustomTypeDef() : nullptr);

    std::vector<Type*> paramTys;
    for (auto* arg : invoke.GetArgs()) {
        paramTys.emplace_back(arg->GetType());
    }
    if (!paramTys.empty()) {
        paramTys.erase(paramTys.begin());
    }
    auto* callSigTy = builder.GetType<FuncType>(paramTys, builder.GetUnitTy());
    FuncCallType callType{methodName, callSigTy, invoke.GetInstantiatedTypeArgs()};

    struct SlotHit {
        std::string parentName;
        Function* inst{nullptr};
    };
    std::vector<SlotHit> hits;
    std::unordered_set<Function*> uniq;
    Function* intendedInst = nullptr;
    for (const auto& vtableIt : genericDef->GetDefVTable().GetTypeVTables()) {
        auto* slotParentDef = vtableIt.GetSrcParentType() != nullptr ? vtableIt.GetSrcParentType()->GetClassDef()
                                                                     : nullptr;
        auto* slotParent = CanonicalDef(slotParentDef);
        std::string parentName = "?";
        if (slotParent != nullptr) {
            parentName = slotParent->GetPackageName() + "::" + slotParent->GetSrcCodeIdentifier();
        }
        for (const auto& info : vtableIt.GetVirtualMethods()) {
            std::unordered_map<const GenericType*, Type*> replaceTable;
            if (!info.FuncSigIsMatched(callType, replaceTable, builder)) {
                continue;
            }
            auto* inst = info.GetVirtualMethod();
            if (inst == nullptr) {
                continue;
            }
            auto* raw = UnwrapVirtMethod(inst);
            uniq.insert(raw);
            hits.push_back(SlotHit{parentName, inst});
            if (intendedParent != nullptr && slotParent == intendedParent) {
                intendedInst = inst;
            }
        }
    }

    // Relaxed: any same-signature multi-slot case is suspicious for Devirtualization.
    if (uniq.size() <= 1) {
        std::cout.flush();
        return;
    }

    static std::mutex mu;
    static std::unordered_set<std::string> printed;
    std::string key = FormatCallSite(invoke) + "." + methodName + "." + FormatFuncDesc(pickedRaw);
    std::lock_guard<std::mutex> lock(mu);
    if (!printed.insert(key).second) {
        std::cout.flush();
        return;
    }

    std::cout << "[DEVIRT WARN] 同签名虚函数落在多个父类槽，优化可能选错\n"
              << "  调用位置: " << FormatCallSite(invoke) << "\n"
              << "  虚函数名: " << methodName << "\n"
              << "  thisType: " << thisType.ToString() << "\n"
              << "  优化实际选中: " << FormatFuncDesc(pickedRaw) << "\n";
    if (intendedInst != nullptr) {
        std::cout << "  invoke声明父类槽: " << FormatFuncDesc(intendedInst) << "\n";
    } else {
        std::cout << "  invoke声明父类槽: <未匹配到>\n";
    }
    for (const auto& hit : hits) {
        std::cout << "  slot " << hit.parentName << " -> " << FormatFuncDesc(UnwrapVirtMethod(hit.inst)) << "\n";
    }
    std::cout << std::flush;
}
} // namespace

void Devirtualization::RunOnFuncs(const std::vector<Function*>& funcs, CHIRBuilder& builder, bool isDebug)
{
    rewriteInfos.clear();
    for (auto func : funcs) {
        RunOnFunc(func, builder);
    }
    RewriteToApply(builder, rewriteInfos, isDebug);
    InstantiateFuncIfPossible(builder, rewriteInfos);
}

void Devirtualization::RunOnFunc(const Function* func, CHIRBuilder& builder)
{
    auto result = analysisWrapper->CheckFuncResult(func);
    if (result == nullptr && frozenStates.count(func) != 0) {
        result = frozenStates.at(func).get();
    }
    if (result == nullptr) {
        return;
    }
    std::optional<std::unordered_set<const GenericType*>> visibleGenericTypes = std::nullopt;
    const auto actionBeforeVisitExpr =
        [this, &builder, &visibleGenericTypes, func](const TypeDomain& state, Expression* expr, size_t) {
        if (expr->GetExprKind() != ExprKind::INVOKE) {
            return;
        }
        auto invoke = StaticCast<Invoke*>(expr);
        auto object = invoke->GetObject();
        auto invokeAbsObject = state.CheckAbstractObjectRefBy(object);
        // Obtains the state information of the invoke operation object.
        auto resVal = state.CheckAbstractValue(invokeAbsObject);
        if (!resVal) {
            return;
        }
        std::vector<Type*> paramTys;
        for (auto param : invoke->GetArgs()) {
            paramTys.emplace_back(param->GetType());
        }
        // Grab the function from the classMap.
        const CustomTypeDef* declaringParent = nullptr;
        if (auto* callee = invoke->GetCallee()) {
            declaringParent = callee->GetParentCustomTypeDef();
        }
        auto [realCallee, thisType] = FindRealCallee(builder, resVal,
            {invoke->GetMethodName(), std::move(paramTys), invoke->GetInstantiatedTypeArgs(), declaringParent});
        if (!realCallee) {
            return;
        }
        PrintIfSplitVTableDevirt(builder, *invoke, *realCallee, *thisType, func, resVal);
        if (thisType->IsGenericRelated()) {
            if (visibleGenericTypes == std::nullopt) {
                auto types = GetVisiableGenericTypes(*func);
                visibleGenericTypes = std::unordered_set<const GenericType*>(types.begin(), types.end());
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
            if (!CheckAllGenericTypeVisible(*thisType, visibleGenericTypes.value())) {
                return;
            }
        }
        rewriteInfos.emplace_back(RewriteInfo{invoke, realCallee, thisType, invoke->GetInstantiatedTypeArgs()});
    };

    const auto actionAfterVisitExpr = [](const TypeDomain&, Expression*, size_t) {};
    const auto actionOnTerminator = [](const TypeDomain&, Expression*, std::optional<Block*>) {};
    result->VisitWith(actionBeforeVisitExpr, actionAfterVisitExpr, actionOnTerminator);
}

namespace {
struct BuiltinOpInfo {
    BuiltinOpInfo(FuncInfo info, ExprKind exprKind, size_t operandsNum)
        : funcInfo(std::move(info)), targetExprKind(exprKind), targetBinaryKind(std::nullopt),
          operandsNum(operandsNum) {}
    BuiltinOpInfo(FuncInfo info, BinaryExprKind binaryKind, size_t operandsNum)
        : funcInfo(std::move(info)), targetExprKind(ExprKind::INVALID), targetBinaryKind(binaryKind),
          operandsNum(operandsNum) {}

    FuncInfo funcInfo;
    ExprKind targetExprKind{ExprKind::INVALID};
    std::optional<BinaryExprKind> targetBinaryKind;
    size_t operandsNum{0};
};

static const std::vector<BuiltinOpInfo> COMPARABLE_FUNC_LISTS = {
    {FuncInfo(">", NOT_CARE, {NOT_CARE}, ANY_TYPE, "std.core"),    BinaryExprKind::GT,       2U},
    {FuncInfo("<", NOT_CARE, {NOT_CARE}, ANY_TYPE, "std.core"),    BinaryExprKind::LT,       2U},
    {FuncInfo(">=", NOT_CARE, {NOT_CARE}, ANY_TYPE, "std.core"),   BinaryExprKind::GE,       2U},
    {FuncInfo("<=", NOT_CARE, {NOT_CARE}, ANY_TYPE, "std.core"),   BinaryExprKind::LE,       2U},
    {FuncInfo("==", NOT_CARE, {NOT_CARE}, ANY_TYPE, "std.core"),   BinaryExprKind::EQUAL,    2U},
    {FuncInfo("!=", NOT_CARE, {NOT_CARE}, ANY_TYPE, "std.core"),   BinaryExprKind::NOTEQUAL, 2U},
    {FuncInfo("next", NOT_CARE, {NOT_CARE}, ANY_TYPE, "std.core"), ExprKind::APPLY,    2U}
};

Ptr<Apply> BuiltinOpCreateNewApply(CHIRBuilder& builder, const Invoke& oriInvoke, Ptr<Function> func,
    const Ptr<Value>& thisValue, const std::vector<Value*>& args)
{
    auto instRetTy = oriInvoke.GetResultType();
    std::vector<Value*> applyArgs{thisValue};
    applyArgs.insert(applyArgs.end(), args.begin(), args.end());
    auto thisType = builder.GetType<RefType>(thisValue->GetType());
    return builder.CreateExpression<Apply>(instRetTy, func, FuncCallContext{
        .args = applyArgs,
        .thisType = thisType}, oriInvoke.GetParentBlock());
}

Ptr<BinaryExpression> BuiltinOpCreateNewBinary(CHIRBuilder& builder, const Invoke& oriInvoke, BinaryExprKind kind,
    const Ptr<Value>& thisValue, const std::vector<Value*>& args)
{
    CJC_ASSERT(args.size() == 1U);
    auto instRetTy = oriInvoke.GetResultType();
    auto parent = oriInvoke.GetParentBlock();
    return builder.CreateExpression<BinaryExpression>(
        oriInvoke.GetDebugLocation(), instRetTy, kind, thisValue, args[0], parent);
}
}

bool Devirtualization::RewriteToBuiltinOp(CHIRBuilder& builder, const RewriteInfo& info, bool isDebug)
{
    auto invoke = info.invoke;
    auto func = info.realCallee->Get<WrappedRawMethod>() ? info.realCallee->Get<WrappedRawMethod>() : info.realCallee;

    ExprKind targetExprKind{ExprKind::INVALID};
    std::optional<BinaryExprKind> targetBinaryKind;
    size_t operandsNum = 0;
    for (auto& it : COMPARABLE_FUNC_LISTS) {
        if (IsExpectedFunction(*func, it.funcInfo)) {
            targetExprKind = it.targetExprKind;
            targetBinaryKind = it.targetBinaryKind;
            operandsNum = it.operandsNum;
        }
    }
    auto args = invoke->GetArgs();
    args.erase(args.begin());  // remove `this`
    if ((targetExprKind == ExprKind::INVALID && !targetBinaryKind.has_value()) || args.size() != operandsNum - 1U) {
        return false;
    }
    for (auto& arg : args) {
        if (!arg->GetType()->IsPrimitive()) {
            return false;
        }
    }
    auto thisValue = invoke->GetObject();
    std::vector<Expression*> castExprs;
    std::function<void(LocalVar&)> findThisValue = [&thisValue, &castExprs, &findThisValue](LocalVar& tempVar) {
        auto expr = tempVar.GetExpr();
        if (expr->GetExprKind() == ExprKind::BOX || expr->GetExprKind() == ExprKind::CLASS_STATIC_CAST) {
            castExprs.emplace_back(expr);
            thisValue = expr->GetOperand(0);
            if (auto localVar = DynamicCast<LocalVar*>(thisValue)) {
                findThisValue(*localVar);
            }
        }
    };
    if (thisValue->IsLocalVar() && !thisValue->GetType()->IsPrimitive()) {
        findThisValue(*StaticCast<LocalVar*>(thisValue));
    }
    if (!thisValue->GetType()->IsPrimitive()) {
        return false;
    }
    Ptr<Expression> op;
    if (targetExprKind == ExprKind::APPLY) {
        op = BuiltinOpCreateNewApply(builder, *invoke, func, thisValue, args);
    } else {
        CJC_ASSERT(targetBinaryKind.has_value());
        op = BuiltinOpCreateNewBinary(builder, *invoke, *targetBinaryKind, thisValue, args);
    }
    invoke->ReplaceWith(*op);
    for (auto e : castExprs) {
        if (e->GetResult()->GetUsers().empty()) {
            e->RemoveSelfFromBlock();
        }
    }
    if (isDebug) {
        std::string callName =
            targetExprKind == ExprKind::APPLY ? func->GetSrcCodeIdentifier() : op->GetExprKindName();
        std::string message = "[Devirtualization] The function call to " + invoke->GetMethodName() +
                              ToPosInfo(invoke->GetDebugLocation()) + " was optimized to builtin op " + callName + ".";
        std::cout << message << std::endl;
    }
    return true;
}

void Devirtualization::RewriteToApply(CHIRBuilder& builder, std::vector<RewriteInfo>& rewriteInfos, bool isDebug)
{
    for (auto rewriteInfo = rewriteInfos.rbegin(); rewriteInfo != rewriteInfos.rend(); ++rewriteInfo) {
        if (RewriteToBuiltinOp(builder, *rewriteInfo, isDebug)) {
            continue;
        }
        auto invoke = rewriteInfo->invoke;
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
        auto thisDerefType = thisType->StripAllRefs();
        auto instThisType = GetInstParentType(
            *thisDerefType, *realFunc->GetFuncType()->GetParamTypes()[0]->StripAllRefs(), builder);
        if (thisDerefType->IsClassOrArray() || realFunc->TestAttr(Attribute::MUT)) {
            instThisType = builder.GetType<RefType>(instThisType);
        }
        if (rewriteInfo->thisType->IsBuiltinType()) {
            instThisType = builder.GetType<RefType>(builder.GetAnyTy());
        }
        auto instRetTy = invoke->GetResultType();
        auto typecastRes = TypeCastOrBoxIfNeeded(*args[0], *instThisType, builder, *parent, INVALID_LOCATION);
        if (typecastRes != args[0]) {
            StaticCast<LocalVar*>(typecastRes)->GetExpr()->MoveBefore(invoke);
            args[0] = typecastRes;
        }
        auto loc = invoke->GetDebugLocation();
        auto apply = builder.CreateExpression<Apply>(loc, instRetTy, realFunc, FuncCallContext{
            .args = args,
            .instTypeArgs = rewriteInfo->typeArgs,
            .thisType = thisType}, invoke->GetParentBlock());
        rewriteInfo->newApply = apply;
        invoke->ReplaceWith(*apply);
        invoke->GetResult()->ReplaceWith(*apply->GetResult(), parent->GetParentBlockGroup());
        if (isDebug) {
            std::string message = "[Devirtualization] The function call to " + invoke->GetMethodName() +
                ToPosInfo(invoke->GetDebugLocation()) + " was optimized.";
            std::cout << message << std::endl;
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

void Devirtualization::InstantiateFuncIfPossible(CHIRBuilder& builder, std::vector<RewriteInfo>& rewriteInfoList)
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
                auto arg = builder.CreateParameter(parameterType[i], callee->GetParam(i)->GetDebugLocation(), *newFunc);
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

namespace {
void BuildOrphanTypeReplaceTable(
    const Cangjie::CHIR::Type* mayBeGeneric, const std::unordered_map<const GenericType*, Type*>& replaceTable)
{
    auto genericTypeArgs = mayBeGeneric->GetTypeArgs();
    for (size_t i = 0; i < genericTypeArgs.size(); ++i) {
        BuildOrphanTypeReplaceTable(genericTypeArgs[i], replaceTable);
    }
}

Function* FindFunctionInVtable(const ClassType* parentTy, const std::vector<VirtualMethodInfo>& infos,
    const Devirtualization::FuncSig& method, CHIRBuilder& builder)
{
    std::unordered_map<const GenericType*, Type*> parentReplaceTable;
    auto paramTypes = method.types;
    paramTypes.erase(paramTypes.begin());
    if (!parentTy->GetTypeArgs().empty()) {
        auto instParentTypeArgs = parentTy->GetTypeArgs();
        auto genericParentTypeArgs = parentTy->GetCustomTypeDef()->GetGenericTypeParams();
        for (size_t i = 0; i < genericParentTypeArgs.size(); ++i) {
            parentReplaceTable.emplace(genericParentTypeArgs[i], instParentTypeArgs[i]);
        }
    }

    for (auto& info : infos) {
        if (info.GetMethodName() != method.name) {
            continue;
        }
        auto sigParamTys = info.GetMethodSigType()->GetParamTypes();
        if (sigParamTys.size() != paramTypes.size()) {
            continue;
        }
        if (info.GetGenericTypeParams().size() != method.typeArgs.size()) {
            continue;
        }
        bool isSigSame = true;
        std::unordered_map<const GenericType*, Type*> freeGenericReplaceTable;
        for (size_t i = 0; i < sigParamTys.size(); ++i) {
            BuildOrphanTypeReplaceTable(sigParamTys[i], freeGenericReplaceTable);
            BuildOrphanTypeReplaceTable(paramTypes[i], freeGenericReplaceTable);
        }
        auto methodGenerics = info.GetGenericTypeParams();
        for (size_t i{0}; i < methodGenerics.size(); ++i) {
            freeGenericReplaceTable.emplace(methodGenerics[i], method.typeArgs[i]);
        }
        for (size_t i = 0; i < paramTypes.size(); ++i) {
            if (isSigSame) {
                auto lhs = ReplaceRawGenericArgType(*sigParamTys[i], freeGenericReplaceTable, builder);
                auto rhs = ReplaceRawGenericArgType(*paramTypes[i], parentReplaceTable, builder);
                rhs = ReplaceRawGenericArgType(*rhs, freeGenericReplaceTable, builder);
                isSigSame = lhs == rhs;
            } else {
                break;
            }
        }
        if (!isSigSame) {
            continue;
        }
        return info.GetVirtualMethod()->IsPureAbstract() ? nullptr : info.GetVirtualMethod();
    }
    return nullptr;
}

void PrintDevirtSlotFix(const Devirtualization::FuncSig& method, Function* wrongFirst, Function* fixed,
    const CustomTypeDef* declaringParent, Type* thisType)
{
    if (wrongFirst == nullptr || fixed == nullptr || UnwrapVirtMethod(wrongFirst) == UnwrapVirtMethod(fixed)) {
        return;
    }
    static std::mutex mu;
    static std::unordered_set<std::string> printed;
    std::string parentName = declaringParent != nullptr
        ? (declaringParent->GetPackageName() + "::" + declaringParent->GetSrcCodeIdentifier())
        : "<unknown>";
    std::string key = method.name + "|" + parentName + "|" + FormatFuncDesc(UnwrapVirtMethod(wrongFirst)) + "|" +
        FormatFuncDesc(UnwrapVirtMethod(fixed));
    std::lock_guard<std::mutex> lock(mu);
    if (!printed.insert(key).second) {
        return;
    }
    std::cout << "[DEVIRT FIX] 按 invoke 声明父类槽纠正虚函数目标\n"
              << "  虚函数名: " << method.name << "\n"
              << "  声明父类: " << parentName << "\n"
              << "  thisType: " << (thisType != nullptr ? thisType->ToString() : "<null>") << "\n"
              << "  旧逻辑(取首个同签名槽): " << FormatFuncDesc(UnwrapVirtMethod(wrongFirst)) << "\n"
              << "  纠正后(声明父类槽): " << FormatFuncDesc(UnwrapVirtMethod(fixed)) << "\n"
              << std::flush;
}

/** Pick vtable impl: prefer the slice whose SrcParent matches invoke's declaring parent. */
Function* SelectFuncFromTypeVTables(CustomTypeDef& genericDef, const Devirtualization::FuncSig& method,
    CHIRBuilder& builder, Type* thisTypeForLog)
{
    Function* firstHit = nullptr;
    Function* preferredHit = nullptr;
    auto* preferParent = CanonicalDef(method.declaringParent);
    for (const auto& it : genericDef.GetDefVTable().GetTypeVTables()) {
        auto* target = FindFunctionInVtable(it.GetSrcParentType(), it.GetVirtualMethods(), method, builder);
        if (target == nullptr) {
            continue;
        }
        if (firstHit == nullptr) {
            firstHit = target;
        }
        auto* slotParent = CanonicalDef(
            it.GetSrcParentType() != nullptr ? it.GetSrcParentType()->GetClassDef() : nullptr);
        if (preferParent != nullptr && slotParent == preferParent) {
            preferredHit = target;
            break;
        }
    }
    if (preferredHit != nullptr) {
        PrintDevirtSlotFix(method, firstHit, preferredHit, method.declaringParent, thisTypeForLog);
        return preferredHit;
    }
    return firstHit;
}

Function* SelectFuncFromVTableSearchRes(const std::vector<VTableSearchRes>& res,
    const Devirtualization::FuncSig& method, Type* thisTypeForLog)
{
    Function* firstHit = nullptr;
    Function* preferredHit = nullptr;
    auto* preferParent = CanonicalDef(method.declaringParent);
    for (const auto& item : res) {
        if (item.instance == nullptr || item.instance->IsPureAbstract()) {
            continue;
        }
        if (firstHit == nullptr) {
            firstHit = item.instance;
        }
        auto* slotParent = CanonicalDef(
            item.instSrcParentType != nullptr ? item.instSrcParentType->GetClassDef() : nullptr);
        if (preferParent != nullptr && slotParent == preferParent) {
            preferredHit = item.instance;
            break;
        }
    }
    if (preferredHit != nullptr) {
        PrintDevirtSlotFix(method, firstHit, preferredHit, method.declaringParent, thisTypeForLog);
        return preferredHit;
    }
    return firstHit;
}

void PrintDevirtUnsafeSkip(const Devirtualization::FuncSig& method, Function* legacyTarget, Type* legacyThisType,
    const CustomTypeDef* declaringParent, Type* collectRoot)
{
    if (legacyTarget == nullptr) {
        return;
    }
    static std::mutex mu;
    static std::unordered_set<std::string> printed;
    std::string parentName = declaringParent != nullptr
        ? (declaringParent->GetPackageName() + "::" + declaringParent->GetSrcCodeIdentifier())
        : "<unknown>";
    std::string key = method.name + "|skip|" + parentName + "|" + FormatFuncDesc(UnwrapVirtMethod(legacyTarget));
    std::lock_guard<std::mutex> lock(mu);
    if (!printed.insert(key).second) {
        return;
    }
    std::cout << "[DEVIRT FIX] 拒绝不安全的首槽去虚（声明父类下子类实现不一致，保留虚调用）\n"
              << "  虚函数名: " << method.name << "\n"
              << "  声明父类: " << parentName << "\n"
              << "  收集根类型: " << (collectRoot != nullptr ? collectRoot->ToString() : "<null>") << "\n"
              << "  旧逻辑会错误选中: " << FormatFuncDesc(UnwrapVirtMethod(legacyTarget)) << "\n"
              << "  旧逻辑thisType: " << (legacyThisType != nullptr ? legacyThisType->ToString() : "<null>") << "\n"
              << std::flush;
}

bool VTableSliceMatchesDeclaringParent(const ClassType* srcParentType, const CustomTypeDef* declaringParent,
    ClassType* expected, CHIRBuilder& builder)
{
    if (srcParentType == nullptr) {
        return false;
    }
    auto* preferParent = CanonicalDef(declaringParent);
    if (preferParent != nullptr) {
        return CanonicalDef(srcParentType->GetClassDef()) == preferParent;
    }
    // Fallback to legacy filter when declaring parent is unknown.
    return expected != nullptr && expected->IsEqualOrSubTypeOf(*srcParentType, builder);
}
} // namespace

bool Devirtualization::IsInstantiationOf(
    CHIRBuilder& builder, const GenericType* generic, const Type* instantiated) const
{
    if (generic->GetUpperBounds().empty()) {
        return true;
    }
    std::unordered_set<Type*> possibleParentTys;
    for (auto def : devirtFuncInfo.defsMap[instantiated]) {
        for (auto parentTy : def->GetSuperTypesInCurDef()) {
            auto inheritLists = parentTy->GetSuperTypesRecusively(builder);
            possibleParentTys.insert(inheritLists.begin(), inheritLists.end());
        }
    }
    for (auto upperBound : generic->GetUpperBounds()) {
        if (possibleParentTys.find(upperBound) == possibleParentTys.end()) {
            return false;
        }
    }

    return true;
}

bool Devirtualization::IsValidSubType(CHIRBuilder& builder, const Type* expected, Type* specific,
    std::unordered_map<const GenericType*, Type*>& replaceTable) const
{
    if (expected->GetTypeKind() != specific->GetTypeKind() && !expected->IsGeneric()) {
        return false;
    }
    if (expected->IsGeneric()) {
        auto generic = Cangjie::StaticCast<const GenericType*>(expected);
        if (IsInstantiationOf(builder, generic, specific)) {
            replaceTable.emplace(generic, specific);
            return true;
        }
        return false;
    }
    if (expected->IsNominal()) {
        const CustomType* specificCustomTy = Cangjie::StaticCast<const CustomType*>(specific);
        if (!specificCustomTy->IsEqualOrSubTypeOf(*expected, builder)) {
            return false;
        }
    }
    auto argsOfExpected = expected->GetTypeArgs();
    auto argsOfSpecific = specific->GetTypeArgs();
    if (argsOfExpected.size() != argsOfSpecific.size()) {
        return false;
    }
    for (size_t i = 0; i < argsOfExpected.size(); ++i) {
        if (!IsValidSubType(
            builder, argsOfExpected[i]->StripAllRefs(), argsOfSpecific[i]->StripAllRefs(), replaceTable)) {
            return false;
        }
    }
    return true;
}

std::pair<Function*, Type*> Devirtualization::FindRealCallee(
    CHIRBuilder& builder, const TypeValue* typeState, const FuncSig& method) const
{
    auto typeStateKind = typeState->GetTypeKind();
    auto specificType = typeState->GetSpecificType();
    if (!specificType->IsClass() || typeStateKind == DevirtualTyKind::EXACTLY) {
        std::vector<CustomTypeDef*> extendsOrImplements{};
        if (auto customType = DynamicCast<const CustomType*>(specificType)) {
            extendsOrImplements = devirtFuncInfo.defsMap[customType->GetCustomTypeDef()->GetType()];
        } else {
            extendsOrImplements = devirtFuncInfo.defsMap[specificType];
        }

        Function* target = nullptr;
        for (auto def : extendsOrImplements) {
            auto [typeMatched, replaceTable] = def->GetType()->CalculateGenericTyMapping(*specificType);
            if (!typeMatched) {
                continue;
            }
            auto paramTypes = method.types;
            paramTypes.erase(paramTypes.begin());
            auto funcType = builder.GetType<FuncType>(paramTypes, builder.GetUnitTy());
            FuncCallType funcCallType{method.name, funcType, method.typeArgs};
            auto res = def->GetFuncIndexInVTable(funcCallType, replaceTable, builder);
            target = SelectFuncFromVTableSearchRes(res, method, specificType);
            if (target != nullptr) {
                break;
            }
        }
        CJC_NULLPTR_CHECK(target);
        return {target, specificType};
    } else {
        // The specific type is an interface or a class, and the state kind is SUBCLASS_OF.
        ClassType* specificType1 = StaticCast<ClassType*>(typeState->GetSpecificType());
        ClassType* collectRoot = specificType1;
        // Prefer invoke's declaring parent type as collect root. Type analysis may narrow to one
        // concrete subtype (e.g. NoCipherOutputStream) while the shared call site still serves
        // siblings (e.g. ZipStandardCipherOutputStream); collecting from the declaring parent
        // exposes conflicting overrides and keeps the invoke instead of a wrong Apply.
        if (method.declaringParent != nullptr) {
            if (auto* declTy = DynamicCast<ClassType*>(method.declaringParent->GetType())) {
                collectRoot = declTy;
            }
        }
        std::pair<Function*, Type*> res{nullptr, nullptr};
        CollectCandidates(builder, collectRoot, res, method);
        if (res.first == nullptr && method.declaringParent != nullptr && collectRoot != specificType1) {
            FuncSig legacyMethod = method;
            legacyMethod.declaringParent = nullptr;
            std::pair<Function*, Type*> legacyRes{nullptr, nullptr};
            CollectCandidates(builder, specificType1, legacyRes, legacyMethod);
            if (legacyRes.first != nullptr) {
                PrintDevirtUnsafeSkip(
                    method, legacyRes.first, legacyRes.second, method.declaringParent, collectRoot);
            }
        }
        return res;
    }
}

Function* Devirtualization::GetCandidateFromSpecificType(
    CHIRBuilder& builder, ClassType& specific, const FuncSig& method) const
{
    auto specificDef = specific.GetClassDef();
    if (specificDef->IsAbstract() || specificDef->IsInterface()) {
        return nullptr;
    }
    auto customType = specific.GetClassDef()->GetType();
    auto extendsOrImplements = devirtFuncInfo.defsMap[customType];
    for (auto oriDef : extendsOrImplements) {
        auto genericDef = oriDef->GetGenericDecl() != nullptr ? oriDef->GetGenericDecl() : oriDef;
        if (auto target = SelectFuncFromTypeVTables(*genericDef, method, builder, &specific)) {
            return target;
        }
    }
    return nullptr;
}

void Devirtualization::CollectCandidates(
    CHIRBuilder& builder, ClassType* specific, std::pair<Function*, Type*>& res, const FuncSig& method) const
{
    auto specificDef = specific->GetClassDef();
    // Open/public types normally bail out (incomplete subtype world). When the invoke's declaring
    // parent is known, still walk subtypes and prefer that vtable slice so a unique impl can be
    // selected, or conflicting overrides keep the invoke.
    bool skipOpenExternal =
        specificDef->CanBeInherited() && !devirtFuncInfo.CheckCustomTypeInternal(*specificDef);
    if (skipOpenExternal && method.declaringParent == nullptr) {
        return;
    }
    // 1. Get candidate from this type
    auto targetFromSpecificType = GetCandidateFromSpecificType(builder, *specific, method);
    if (targetFromSpecificType != nullptr) {
        if (res.first == nullptr) {
            res = {targetFromSpecificType, specific};
        } else if (UnwrapVirtMethod(res.first) != UnwrapVirtMethod(targetFromSpecificType)) {
            res = {nullptr, nullptr};
            return;
        }
    }
    if (!specificDef->CanBeInherited()) {
        // non-open class do not need try its subtype
        return;
    }
    auto& subtypeMap = devirtFuncInfo.GetSubtypeMap();
    auto it = subtypeMap.find(specificDef);
    if (it == subtypeMap.end()) {
        // return if has no subtype
        return;
    }
    // 2. Get candidate from subtypes
    for (auto& inheritInfo : it->second) {
        auto expected = inheritInfo.parentType;
        std::unordered_map<const GenericType*, Type*> replaceTable;
        if (!IsValidSubType(builder, expected, specific, replaceTable)) {
            continue;
        }
        auto subtype = ReplaceRawGenericArgType(*(inheritInfo.subType), replaceTable, builder);
        auto subtypeCustom = DynamicCast<CustomType*>(subtype);
        if (!subtypeCustom ||
            (!subtypeCustom->GetCustomTypeDef()->IsInterface() &&
                !subtypeCustom->GetCustomTypeDef()->TestAttr(Attribute::ABSTRACT))) {
            auto extendsOrImplements = devirtFuncInfo.defsMap[subtypeCustom];
            for (auto oriDef : extendsOrImplements) {
                auto def = oriDef->GetGenericDecl() != nullptr ? oriDef->GetGenericDecl() : oriDef;
                Function* firstHit = nullptr;
                Function* preferredHit = nullptr;
                for (const auto& vtableIt : def->GetDefVTable().GetTypeVTables()) {
                    if (!VTableSliceMatchesDeclaringParent(
                        vtableIt.GetSrcParentType(), method.declaringParent, expected, builder)) {
                        continue;
                    }
                    if (auto target = FindFunctionInVtable(
                        vtableIt.GetSrcParentType(), vtableIt.GetVirtualMethods(), method, builder)) {
                        if (firstHit == nullptr) {
                            firstHit = target;
                        }
                        auto* preferParent = CanonicalDef(method.declaringParent);
                        auto* slotParent = CanonicalDef(vtableIt.GetSrcParentType() != nullptr
                                ? vtableIt.GetSrcParentType()->GetClassDef()
                                : nullptr);
                        if (preferParent != nullptr && slotParent == preferParent) {
                            preferredHit = target;
                        }
                    }
                }
                auto target = preferredHit != nullptr ? preferredHit : firstHit;
                if (preferredHit != nullptr) {
                    PrintDevirtSlotFix(method, firstHit, preferredHit, method.declaringParent, subtypeCustom);
                }
                if (target != nullptr) {
                    if (res.first == nullptr) {
                        res = {target, subtypeCustom};
                    } else if (UnwrapVirtMethod(res.first) != UnwrapVirtMethod(target)) {
                        res = {nullptr, nullptr};
                        return;
                    }
                }
            }
        }
        if (subtypeCustom && subtypeCustom->IsClass()) {
            CollectCandidates(builder, StaticCast<ClassType*>(subtypeCustom), res, method);
        }
    }
}

bool Devirtualization::CheckFuncHasInvoke(const BlockGroup& bg)
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
            if (exprs[i]->GetExprKind() == ExprKind::INVOKE) {
                return true;
            }
        }
    }
    return false;
}

bool Devirtualization::CheckAllGenericTypeVisible(
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

std::vector<Function*> Devirtualization::CollectContainInvokeExprFuncs(const Ptr<const Package>& package)
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
