// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/IR/Expression/Terminator.h"

#include <iostream>
#include <sstream>
#include <string>

#include "cangjie/CHIR/AST2CHIR/Utils.h"
#include "cangjie/CHIR/IR/Expression/Expression.h"
#include "cangjie/CHIR/Utils/CHIRCasting.h"
#include "cangjie/CHIR/IR/CHIRBuilder.h"
#include "cangjie/CHIR/Utils/ToStringUtils.h"
#include "cangjie/CHIR/IR/Type/Type.h"
#include "cangjie/CHIR/Utils/Utils.h"
#include "cangjie/CHIR/IR/Value/Value.h"
#include "cangjie/Utils/CheckUtils.h"

using namespace Cangjie::CHIR;

// Terminator
Terminator::Terminator(
    ExprKind kind, const std::vector<Value*>& operands, const std::vector<Block*>& successors, Block* parent)
    : Expression(kind, operands, parent)
{
    CJC_NULLPTR_CHECK(parent);
    for (auto succ : successors) {
        AppendOperand(*succ);
    }
}

GoTo::GoTo(Block* destBlock, Block* parent) : Terminator(ExprKind::GOTO, {}, {destBlock}, parent)
{
}

Block* GoTo::GetDestination() const
{
    return GetSuccessor(0);
}

// MultiBranch

MultiBranch::MultiBranch(Value* cond, Block* defaultBlock, const std::vector<uint64_t>& vals,
    const std::vector<Block*>& succs, Block* parent)
    : Terminator(ExprKind::MULTIBRANCH, {cond}, {}, parent), caseVals(vals)
{
    CJC_NULLPTR_CHECK(cond);
    CJC_NULLPTR_CHECK(defaultBlock);
    CJC_NULLPTR_CHECK(parent);
    /* Note that successors[0] is used to store the default basic block */
    AppendOperand(*defaultBlock);
    for (auto b : succs) {
        AppendOperand(*b);
    }
}

Value* MultiBranch::GetCondition() const
{
    return operands[0];
}

const std::vector<uint64_t>& MultiBranch::GetCaseVals() const
{
    return caseVals;
}

uint64_t MultiBranch::GetCaseValByIndex(size_t index) const
{
    return caseVals[index];
}

Block* MultiBranch::GetCaseBlockByIndex(size_t index) const
{
    return GetSuccessor(index + 1);
}

Block* MultiBranch::GetDefaultBlock() const
{
    return GetSuccessor(0);
}

std::vector<Block*> MultiBranch::GetNormalBlocks() const
{
    auto succs = GetSuccessors();
    return {succs.begin() + 1, succs.end()};
}

std::string MultiBranch::OperandsToString() const
{
    std::vector<std::string> res;
    res.emplace_back(GetCondition()->GetIdentifier());
    res.emplace_back(GetDefaultBlock()->GetIdentifier());
    for (size_t i = 1; i < GetNumOfSuccessor(); ++i) {
        auto caseValue = std::to_string(GetCaseValByIndex(i - 1));
        auto caseId = GetSuccessor(i)->GetIdentifier();
        res.emplace_back("[" + caseValue + ", " + caseId + "]");
    }
    return StringJoin(res, ", ");
}

ExpressionWithException::ExpressionWithException(ExprKind kind, Block* parent)
    : Terminator(kind, {}, {}, parent)
{
}

ExpressionWithException::ExpressionWithException(
    ExprKind kind, const std::vector<Value*>& operands, const std::vector<Block*>& successors, Block* parent)
    : Terminator(kind, operands, successors, parent)
{
}

Block* ExpressionWithException::GetSuccessBlock() const
{
    return GetSuccessor(0);
}

Block* ExpressionWithException::GetErrorBlock() const
{
    return GetSuccessor(1);
}

FuncCallWithException::FuncCallWithException(ExprKind kind, const FuncCallContext& funcCallCtx, Block* parent)
    : ExpressionWithException(kind, parent),
      instantiatedTypeArgs(funcCallCtx.instTypeArgs),
      thisType(funcCallCtx.thisType)
{
}

Type* FuncCallWithException::GetThisType() const
{
    return thisType;
}

void FuncCallWithException::SetThisType(Type* type)
{
    thisType = type;
}

const std::vector<Type*>& FuncCallWithException::GetInstantiatedTypeArgs() const
{
    return instantiatedTypeArgs;
}

ApplyWithException::ApplyWithException(
    Value* callee, const FuncCallContext& callContext, Block* sucBlock, Block* errBlock, Block* parent)
    : FuncCallWithException(ExprKind::APPLY_WITH_EXCEPTION, callContext, parent)
{
    CJC_NULLPTR_CHECK(callee);
    CJC_NULLPTR_CHECK(sucBlock);
    CJC_NULLPTR_CHECK(errBlock);
    CJC_NULLPTR_CHECK(parent);
    AppendOperand(*callee);
    for (auto op : callContext.args) {
        AppendOperand(*op);
    }

    AppendOperand(*sucBlock);
    AppendOperand(*errBlock);
}

Value* ApplyWithException::GetCallee() const
{
    return operands[0];
}

/** @brief Get a list of the ApplyWithException operation argument nodes */
std::vector<Value*> ApplyWithException::GetArgs() const
{
    if (GetSuccessorIndex(0) <= 1) {
        return {};
    } else {
        return {operands.begin() + 1, operands.begin() + static_cast<long>(GetSuccessorIndex(0))};
    }
}

Type* ApplyWithException::GetInstParentCustomTyOfCallee(CHIRBuilder& builder) const
{
    return GetInstParentCustomTypeForAweCallee(*this, builder);
}

std::string ApplyWithException::OperandsToString() const
{
    std::vector<std::string> res;
    std::string func;
    if (thisType != nullptr) {
        func += thisType->ToString() + "->";
    }
    func += GetCallee()->GetIdentifier();
    func += TypeVecToString("<", instantiatedTypeArgs, ">");
    res.emplace_back(func);
    auto ops = std::vector<Value*>(operands.begin() + 1, operands.end());
    res.emplace_back(ValueIdVecToString("", ops, "", true));
    return StringJoin(res, ", ");
}

inline static void CheckVirFuncInvokeInfo(const InvokeCallContext& callContext)
{
    CJC_NULLPTR_CHECK(callContext.method);
    CJC_NULLPTR_CHECK(callContext.caller);
    CJC_NULLPTR_CHECK(callContext.funcCallCtx.thisType);
}

DynamicDispatchWithException::DynamicDispatchWithException(
    ExprKind kind, const InvokeCallContext& callContext, Block* sucBlock, Block* errBlock, Block* parent)
    : FuncCallWithException(kind, callContext.funcCallCtx, parent), overflowStrategy(callContext.overflowStrategy)
{
    CJC_NULLPTR_CHECK(sucBlock);
    CJC_NULLPTR_CHECK(errBlock);
    CJC_NULLPTR_CHECK(parent);
    CJC_ASSERT(callContext.overflowStrategy != Cangjie::OverflowStrategy::CHECKED);
    CheckVirFuncInvokeInfo(callContext);
    AppendOperand(*callContext.method);
    AppendOperand(*callContext.caller);
    for (auto op : callContext.funcCallCtx.args) {
        AppendOperand(*op);
    }

    AppendOperand(*sucBlock);
    AppendOperand(*errBlock);
}

Function* DynamicDispatchWithException::GetCallee() const
{
    return StaticCast<Function*>(operands[0]);
}

std::string DynamicDispatchWithException::GetMethodName() const
{
    auto name = GetCallee()->GetSrcCodeIdentifier();
    if (overflowStrategy != Cangjie::OverflowStrategy::NA) {
        return OverflowStrategyPrefix(overflowStrategy) + name;
    }
    return name;
}

FuncType* DynamicDispatchWithException::GetMethodType() const
{
    return GetCallee()->GetFuncType();
}

const std::vector<GenericType*>& DynamicDispatchWithException::GetGenericTypeParams() const
{
    return GetCallee()->GetGenericTypeParams();
}

std::vector<VTableSearchRes> DynamicDispatchWithException::GetVirtualMethodInfo(CHIRBuilder& builder) const
{
    auto thisTypeDeref = thisType->StripAllRefs();
    if (thisTypeDeref->IsThis()) {
        thisTypeDeref = GetTopLevelFunc()->GetParentCustomTypeDef()->GetType();
    }
    std::vector<Type*> instParamTypes;
    for (auto arg : GetArgs()) {
        instParamTypes.emplace_back(arg->GetType());
    }
    if (!IsInvokeStaticBase()) {
        instParamTypes.erase(instParamTypes.begin());
    }
    auto instFuncType = builder.GetType<FuncType>(instParamTypes, builder.GetUnitTy());
    FuncCallType funcCallType{GetMethodName(), instFuncType, instantiatedTypeArgs};
    auto res = GetFuncIndexInVTable(*thisTypeDeref, funcCallType, builder);
    CJC_ASSERT(!res.empty());
    return res;
}

size_t DynamicDispatchWithException::GetVirtualMethodOffset(CHIRBuilder* builder) const
{
    auto offset = Get<VirMethodOffset>();
    if (offset.has_value()) {
        return offset.value();
    } else {
        CJC_NULLPTR_CHECK(builder);
        return GetVirtualMethodInfo(*builder)[0].offset;
    }
}

ClassType* DynamicDispatchWithException::GetInstSrcParentCustomTypeOfMethod(CHIRBuilder& builder) const
{
    ClassType* result = nullptr;
    for (auto& r : GetVirtualMethodInfo(builder)) {
        if (r.offset == GetVirtualMethodOffset()) {
            auto def = r.instSrcParentType->GetClassDef();
            const auto& parentFuncInfo = def->GetDefVTable().GetExpectedTypeVTable(*def->GetType());
            auto originalType = parentFuncInfo.GetVirtualMethods()[r.offset].GetOriginalFuncType();
            if (VirMethodTypeIsMatched(*originalType, *GetMethodType())) {
                CJC_NULLPTR_CHECK(r.instSrcParentType);
                return r.instSrcParentType;
            }
        }
    }
    CJC_NULLPTR_CHECK(result);
    return result;
}

AttributeInfo DynamicDispatchWithException::GetVirtualMethodAttr() const
{
    return GetCallee()->GetAttributeInfo();
}

std::string DynamicDispatchWithException::AddExtraComment() const
{
    if (overflowStrategy != Cangjie::OverflowStrategy::NA) {
        return "methodName: " + GetMethodName();
    }
    return "";
}

InvokeWithException::InvokeWithException(
    const InvokeCallContext& callContext, Block* sucBlock, Block* errBlock, Block* parent)
    : DynamicDispatchWithException(ExprKind::INVOKE_WITH_EXCEPTION, callContext, sucBlock, errBlock, parent)
{
}

Value* InvokeWithException::GetObject() const
{
    return operands[1];
}

/** @brief Get the call args of this InvokeWithException operation */
std::vector<Value*> InvokeWithException::GetArgs() const
{
    return {operands.begin() + 1, operands.begin() + static_cast<long>(GetSuccessorIndex(0))};
}

std::string DynamicDispatchWithException::OperandsToString() const
{
    std::vector<std::string> res;
    std::string func;
    if (thisType != nullptr) {
        func += thisType->ToString() + "->";
    }
    func += GetCallee()->GetIdentifier();
    func += TypeVecToString("<", instantiatedTypeArgs, ">");
    res.emplace_back(func);
    auto ops = std::vector<Value*>(operands.begin() + 1, operands.end());
    res.emplace_back(ValueIdVecToString("", ops, "", true));
    return StringJoin(res, ", ");
}

InvokeStaticWithException::InvokeStaticWithException(
    const InvokeCallContext& callContext, Block* sucBlock, Block* errBlock, Block* parent)
    : DynamicDispatchWithException(ExprKind::INVOKESTATIC_WITH_EXCEPTION, callContext, sucBlock, errBlock, parent)
{
}

Value* InvokeStaticWithException::GetRTTIValue() const
{
    return operands[1];
}

std::vector<Value*> InvokeStaticWithException::GetArgs() const
{
    return {operands.begin() + 2, operands.begin() + static_cast<long>(GetSuccessorIndex(0))};
}

// UnaryExpressionWithException
UnaryExpressionWithException::UnaryExpressionWithException(
    UnaryExprKind unaryKind, Value* operand, Block* normal, Block* exception, Block* parent)
    : UnaryExpressionBase(unaryKind, operand, Cangjie::OverflowStrategy::THROWING, true, {normal, exception}, parent)
{
}

Block* UnaryExpressionWithException::GetSuccessBlock() const
{
    return GetSuccessor(0);
}

Block* UnaryExpressionWithException::GetErrorBlock() const
{
    return GetSuccessor(1);
}

// BinaryExpressionWithException
BinaryExpressionWithException::BinaryExpressionWithException(BinaryExprKind binaryKind, Value* lhs, Value* rhs,
    OverflowStrategy ofs, Block* normal, Block* exception, Block* parent)
    : BinaryExpressionBase(binaryKind, lhs, rhs, ofs, true, {normal, exception}, parent)
{
    // Only DIV may overflow under a non-THROWING strategy (e.g. MIN / -1).
    // Other ops reach here for exceptions like div-by-zero or overshift, so force THROWING.
    if (binaryKind != BinaryExprKind::DIV) {
        overflowStrategy = Cangjie::OverflowStrategy::THROWING;
    }
}

Block* BinaryExpressionWithException::GetSuccessBlock() const
{
    return GetSuccessor(0);
}

Block* BinaryExpressionWithException::GetErrorBlock() const
{
    return GetSuccessor(1);
}

// NumericCastWithException
NumericCastWithException::NumericCastWithException(Value* operand, Block* normal, Block* exception, Block* parent)
    : NumericCastBase(
          ExprKind::NUMERIC_CAST_WITH_EXCEPTION, operand, Cangjie::OverflowStrategy::THROWING, {normal, exception},
          parent)
{
}

Block* NumericCastWithException::GetSuccessBlock() const
{
    return GetSuccessor(0);
}

Block* NumericCastWithException::GetErrorBlock() const
{
    return GetSuccessor(1);
}

// IntrinsicWithException
IntrinsicWithException::IntrinsicWithException(
    const IntrisicCallContext& callContext, Block* normal, Block* exception, Block* parent)
    : ExpressionWithException(ExprKind::INTRINSIC_WITH_EXCEPTION, callContext.args, {normal, exception}, parent),
    intrinsicKind(callContext.kind),
    instantiatedTypeArgs(callContext.instTypeArgs)
{
}

IntrinsicKind IntrinsicWithException::GetIntrinsicKind() const
{
    return intrinsicKind;
}

const std::vector<Type*>& IntrinsicWithException::GetInstantiatedTypeArgs() const
{
    return instantiatedTypeArgs;
}

const std::vector<Value*> IntrinsicWithException::GetArgs() const
{
    return GetNonSuccessorOperands();
}

std::string IntrinsicWithException::OperandsToString() const
{
    std::stringstream ss;
    ss << IntrinsicKindToString(intrinsicKind);
    ss << TypeVecToString("<", instantiatedTypeArgs, ">");
    ss << ", " << ValueIdVecToString("", operands, "");
    return ss.str();
}

// AllocateWithException
AllocateWithException::AllocateWithException(Type* ty, Block* normal, Block* exception, Block* parent)
    : AllocateBase(ExprKind::ALLOCATE_WITH_EXCEPTION, ty, {normal, exception}, parent)
{
}

Block* AllocateWithException::GetSuccessBlock() const
{
    return GetSuccessor(0);
}

Block* AllocateWithException::GetErrorBlock() const
{
    return GetSuccessor(1);
}

// RawArrayAllocateWithException
RawArrayAllocateWithException::RawArrayAllocateWithException(
    Type* eleTy, Value* size, Block* normal, Block* exception, Block* parent)
    : RawArrayAllocateBase(ExprKind::RAW_ARRAY_ALLOCATE_WITH_EXCEPTION, eleTy, size, {normal, exception}, parent)
{
}

Block* RawArrayAllocateWithException::GetSuccessBlock() const
{
    return GetSuccessor(0);
}

Block* RawArrayAllocateWithException::GetErrorBlock() const
{
    return GetSuccessor(1);
}

SpawnWithException::SpawnWithException(
    Value* val, Value* arg, Block* normal, Block* exception, Block* parent)
    : SpawnBase(ExprKind::SPAWN_WITH_EXCEPTION, {val, arg}, {normal, exception}, parent)
{
}

SpawnWithException::SpawnWithException(
    Value* val, Block* normal, Block* exception, Block* parent)
    : SpawnBase(ExprKind::SPAWN_WITH_EXCEPTION, {val}, {normal, exception}, parent)
{
}

Block* SpawnWithException::GetSuccessBlock() const
{
    return GetSuccessor(0);
}

Block* SpawnWithException::GetErrorBlock() const
{
    return GetSuccessor(1);
}

SpawnWithException* SpawnWithException::Clone(CHIRBuilder& builder, Block& parent) const
{
    SpawnWithException* newNode = nullptr;
    auto arg = GetSpawnArg();
    if (arg != nullptr) {
        newNode = builder.CreateExpression<SpawnWithException>(
            result->GetType(), GetObject(), arg, GetSuccessBlock(), GetErrorBlock(), &parent);
    } else {
        newNode = builder.CreateExpression<SpawnWithException>(
            result->GetType(), GetObject(), GetSuccessBlock(), GetErrorBlock(), &parent);
    }
    if (executeClosure) {
        newNode->SetExecuteClosure(*executeClosure);
    }
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

GoTo* GoTo::Clone(CHIRBuilder& builder, Block& parent) const
{
    CJC_ASSERT(result == nullptr);
    auto newNode = builder.CreateTerminator<GoTo>(GetDestination(), &parent);
    parent.AppendExpression(newNode);
    return newNode;
}

Branch::Branch(Value* cond, Block* trueBlock, Block* falseBlock, Block* parent)
    : Terminator(ExprKind::BRANCH, {cond}, {trueBlock, falseBlock}, parent)
{
}

Value* Branch::GetCondition() const
{
    return operands[0];
}

Block* Branch::GetTrueBlock() const
{
    return GetSuccessor(0);
}

Block* Branch::GetFalseBlock() const
{
    return GetSuccessor(1);
}

void Branch::SetSourceExpr(SourceExpr srcExpr)
{
    sourceExpr = srcExpr;
}

SourceExpr Branch::GetSourceExpr() const
{
    return sourceExpr;
}

std::string Branch::AddExtraComment() const
{
    const static std::unordered_map<SourceExpr, std::string> SOURCE_EXPR_MAP = {
        {SourceExpr::IF_EXPR, "IF_EXPR"},
        {SourceExpr::WHILE_EXPR, "WHILE_EXPR"},
        {SourceExpr::DO_WHILE_EXPR, "DO_WHILE_EXPR"},
        {SourceExpr::MATCH_EXPR, "MATCH_EXPR"},
        {SourceExpr::IF_LET_OR_WHILE_LET, "IF_LET_OR_WHILE_LET"},
        {SourceExpr::QUEST, "QUEST"},
        {SourceExpr::BINARY, "BINARY"},
        {SourceExpr::FOR_IN_EXPR, "FOR_IN_EXPR"},
        {SourceExpr::OTHER, "OTHER"},
    };
    return "sourceExpr: " + SOURCE_EXPR_MAP.at(sourceExpr);
}

Exit::Exit(Block* parent) : Terminator(ExprKind::EXIT, {}, {}, parent)
{
}

RaiseException::RaiseException(Value* value, Block* parent)
    : Terminator(ExprKind::RAISE_EXCEPTION, {value}, {}, parent)
{
}

RaiseException::RaiseException(Value* value, Block* successor, Block* parent)
    : Terminator(ExprKind::RAISE_EXCEPTION, {value}, {successor}, parent)
{
}

Value* RaiseException::GetExceptionValue() const
{
    return operands[0];
}

Block* RaiseException::GetExceptionBlock() const
{
    auto succs = GetSuccessors();
    if (!succs.empty()) {
        return succs[0];
    }
    return nullptr;
}

Branch* Branch::Clone(CHIRBuilder& builder, Block& parent) const
{
    CJC_ASSERT(result == nullptr);
    auto newNode = builder.CreateTerminator<Branch>(GetCondition(), GetTrueBlock(), GetFalseBlock(), &parent);
    parent.AppendExpression(newNode);
    return newNode;
}

MultiBranch* MultiBranch::Clone(CHIRBuilder& builder, Block& parent) const
{
    CJC_ASSERT(result == nullptr);
    auto newNode = builder.CreateTerminator<MultiBranch>(
        GetCondition(), GetDefaultBlock(), GetCaseVals(), GetNormalBlocks(), &parent);
    parent.AppendExpression(newNode);
    return newNode;
}

Exit* Exit::Clone(CHIRBuilder& builder, Block& parent) const
{
    CJC_ASSERT(result == nullptr);
    auto newNode = builder.CreateTerminator<Exit>(&parent);
    parent.AppendExpression(newNode);
    return newNode;
}

ApplyWithException* ApplyWithException::Clone(CHIRBuilder& builder, Block& parent) const
{
    CJC_NULLPTR_CHECK(result);
    ApplyWithException* newNode = builder.CreateExpression<ApplyWithException>(
        result->GetType(), GetCallee(), FuncCallContext{
        .args = GetArgs(),
        .instTypeArgs = GetInstantiatedTypeArgs(),
        .thisType = GetThisType()}, GetSuccessBlock(), GetErrorBlock(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

InvokeWithException* InvokeWithException::Clone(CHIRBuilder& builder, Block& parent) const
{
    CJC_NULLPTR_CHECK(result);
    auto args = GetArgs();
    args.erase(args.begin());
    auto invokeInfo = InvokeCallContext {
        .method = GetCallee(),
        .caller = GetObject(),
        .funcCallCtx = FuncCallContext {
            .args = args,
            .instTypeArgs = instantiatedTypeArgs,
            .thisType = thisType
        },
        .overflowStrategy = overflowStrategy
    };
    InvokeWithException* newNode = builder.CreateExpression<InvokeWithException>(
        result->GetType(), invokeInfo, GetSuccessBlock(), GetErrorBlock(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

InvokeStaticWithException* InvokeStaticWithException::Clone(CHIRBuilder& builder, Block& parent) const
{
    CJC_NULLPTR_CHECK(result);
    auto invokeInfo = InvokeCallContext {
        .method = GetCallee(),
        .caller = GetRTTIValue(),
        .funcCallCtx = FuncCallContext {
            .args = GetArgs(),
            .instTypeArgs = instantiatedTypeArgs,
            .thisType = thisType
        },
        .overflowStrategy = overflowStrategy
    };
    InvokeStaticWithException* newNode = builder.CreateExpression<InvokeStaticWithException>(
        result->GetType(), invokeInfo, GetSuccessBlock(), GetErrorBlock(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

UnaryExpressionWithException* UnaryExpressionWithException::Clone(CHIRBuilder& builder, Block& parent) const
{
    CJC_NULLPTR_CHECK(result);
    auto newNode = builder.CreateExpression<UnaryExpressionWithException>(result->GetType(), GetOpKind(), GetOperand(),
        GetSuccessBlock(), GetErrorBlock(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

BinaryExpressionWithException* BinaryExpressionWithException::Clone(CHIRBuilder& builder, Block& parent) const
{
    CJC_NULLPTR_CHECK(result);
    auto newNode = builder.CreateExpression<BinaryExpressionWithException>(result->GetType(), GetOpKind(),
        GetLHSOperand(), GetRHSOperand(), overflowStrategy, GetSuccessBlock(), GetErrorBlock(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

NumericCastWithException* NumericCastWithException::Clone(CHIRBuilder& builder, Block& parent) const
{
    CJC_NULLPTR_CHECK(result);
    auto newNode = builder.CreateExpression<NumericCastWithException>(
        result->GetType(), GetSourceValue(), GetSuccessBlock(), GetErrorBlock(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

IntrinsicWithException* IntrinsicWithException::Clone(CHIRBuilder& builder, Block& parent) const
{
    CJC_NULLPTR_CHECK(result);
    auto callContext = IntrisicCallContext {
        .kind = GetIntrinsicKind(),
        .args = GetArgs(),
        .instTypeArgs = GetInstantiatedTypeArgs()
    };
    auto newNode = builder.CreateExpression<IntrinsicWithException>(
        result->GetType(), callContext, GetSuccessBlock(), GetErrorBlock(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

AllocateWithException* AllocateWithException::Clone(CHIRBuilder& builder, Block& parent) const
{
    CJC_NULLPTR_CHECK(result);
    auto newNode = builder.CreateExpression<AllocateWithException>(
        result->GetType(), GetType(), GetSuccessBlock(), GetErrorBlock(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}

RawArrayAllocateWithException* RawArrayAllocateWithException::Clone(CHIRBuilder& builder, Block& parent) const
{
    CJC_NULLPTR_CHECK(result);
    auto newNode = builder.CreateExpression<RawArrayAllocateWithException>(
        result->GetType(), GetElementType(), GetSize(), GetSuccessBlock(), GetErrorBlock(), &parent);
    parent.AppendExpression(newNode);
    newNode->GetResult()->AppendAttributeInfo(result->GetAttributeInfo());
    return newNode;
}
