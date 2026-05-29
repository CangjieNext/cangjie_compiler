// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/AST2CHIR/TranslateASTNode/Translator.h"
#include "cangjie/CHIR/IR/Expression/Terminator.h"
#include "cangjie/CHIR/IR/Type/Type.h"

using namespace Cangjie::CHIR;
using namespace Cangjie;
using namespace Cangjie::AST;

constexpr static int ARGS_NUM_TWO = 2;

Ptr<Value> Translator::Visit(const AST::ArrayExpr& array)
{
    if (array.isValueArray) {
        CJC_ASSERT(array.args.size() == 1);
        CJC_ASSERT(!array.GetTy()->typeArgs.empty());

        if (array.args[0]->GetTy()->IsFunc()) {
            return InitVArrayByLambda(array);
        }
        return InitVArrayByItem(array);
    }

    CJC_ASSERT(array.GetTy()->IsArray());
    CJC_NULLPTR_CHECK(array.initFunc);
    // Case B: "RawArray<T>(collection)" which initialize an sized array with the `collection` content.
    if (array.args.size() == 1) {
        return InitArrayByCollection(array);
    }
    CJC_ASSERT(array.args.size() == ARGS_NUM_TWO);
    // Case D: "var x = RawArray<T>(size, initFunc)" which initialize an sized array by specified init func.
    // Only these two cases uses ArrayExpr on AST because they have an additional field 'initFunc' used, which
    // is either arrayInitByCollection and arrayInitByFunction, which are private std.core funcs and called only by
    // compiler. CallExpr does not have a place to store and instantiate them. (they must be instantiated on -O2).
    // other cases use CallExpr (see following funs).
    return InitRawArrayByLambda(array);
}

Ptr<Value> Translator::InitRawArrayEmpty(const AST::CallExpr& expr)
{
    auto loc = TranslateLocation(expr);
    auto arrayTy = chirTy.TranslateType(expr.GetTy());
    CJC_ASSERT(arrayTy->IsRef());
    auto eleTy = StaticCast<RawArrayType>(arrayTy->GetPureType())->GetElementType();
    auto sizeVal = CreateAndAppendConstantExpression<IntLiteral>(builder.GetInt64Ty(), *currentBlock, 0UL)->GetResult();
    auto rawArrayRef = TryCreate<RawArrayAllocate>(currentBlock, loc, arrayTy, eleTy, sizeVal)->GetResult();
    return rawArrayRef;
}

Expression* Translator::CreateAndAppendApplyCallFromArray(
    Value& callee, FuncCallContext& context, const FuncType& instFuncTy, const Expr& array)
{
    CJC_ASSERT(array.astKind == ASTKind::ARRAY_EXPR || array.astKind == ASTKind::ARRAY_LIT);
    auto funcCall = TryCreate<Apply>(currentBlock, instFuncTy.GetReturnType(), &callee, context);
    const auto& loc = TranslateLocation(array);
    funcCall->SetDebugLocation(loc);
    return funcCall;
}

Expression* Translator::CreateAndAppendGVInitFuncCall(Value& callee)
{
    auto instFuncTy = StaticCast<FuncType*>(callee.GetType());
    auto funcCallContext = FuncCallContext {};
    return TryCreate<Apply>(currentBlock, instFuncTy->GetReturnType(), &callee, funcCallContext);
}

static bool IsRawArrayCtorCall(const CallExpr& expr)
{
    if (!expr.resolvedFunction || !expr.resolvedFunction->TestAttr(AST::Attribute::CONSTRUCTOR)) {
        return false;
    }
    auto outer = expr.resolvedFunction->outerDecl;
    if (!outer || !outer->IsBuiltIn()) {
        return false;
    }
    return StaticCast<BuiltInDecl>(outer)->type == BuiltInType::ARRAY;
}

Ptr<Value> Translator::TranslateRawArrayCtorCall(const AST::CallExpr& expr)
{
    if (!IsRawArrayCtorCall(expr)) {
        return nullptr;
    }
    // Case A: "RawArray<T>()" which initialize an empty array.
    if (expr.args.empty()) {
        return InitRawArrayEmpty(expr);
    }
    // Case C: "var x = RawArray<T>(size, val)" which initialize an sized array where the element is with `val`.
    if (expr.args.size() == ARGS_NUM_TWO) {
        return InitRawArrayByItem(expr);
    }
    CJC_ABORT();
    return nullptr;
}

Ptr<Value> Translator::InitRawArrayByLambda(const ArrayExpr& array)
{
    CJC_ASSERT(array.args.size() == ARGS_NUM_TWO && array.initFunc != nullptr);

    auto loc = TranslateLocation(array);
    auto arrayTy = chirTy.TranslateType(array.GetTy());
    CJC_ASSERT(arrayTy->IsRef());
    auto eleTy = StaticCast<RawArrayType>(arrayTy->GetPureType())->GetElementType();
    auto sizeVal = TranslateExprArg(*array.args[0]);
    auto initFn = GetSymbolTable(*array.initFunc);
    auto rawArrayExpr = CreateAndAppendExpression<RawArrayAllocate>(loc, arrayTy, eleTy, sizeVal, currentBlock);
    auto rawArrayRef = rawArrayExpr->GetResult();

    std::vector<Type*> instantiatedTypeArgs;
    // if array init func is generic decl, then we will create `Apply` expr like: `Apply(init<xxx>, args)`
    // if array init func is instantiated decl, then we will create `Apply` expr like: `Apply(init, args)`
    if (array.initFunc->TestAttr(AST::Attribute::GENERIC)) {
        for (auto ty : array.GetTy()->typeArgs) {
            instantiatedTypeArgs.emplace_back(chirTy.TranslateType(ty));
        }
    }
    auto userInitFn = TranslateExprArg(*array.args[1]);
    std::vector<Type*> instParamTys;
    instParamTys.emplace_back(rawArrayRef->GetType());
    instParamTys.emplace_back(userInitFn->GetType());
    auto instFuncTy = builder.GetType<FuncType>(instParamTys, arrayTy);
    auto funcCallContext = FuncCallContext{
        .args = std::vector<Value*>{rawArrayRef, userInitFn},
        .instTypeArgs = instantiatedTypeArgs,
        .thisType = nullptr
    };
    CreateAndAppendApplyCallFromArray(*initFn, funcCallContext, *instFuncTy, array);

    return rawArrayRef;
}

Ptr<Value> Translator::InitRawArrayByItem(const AST::CallExpr& expr)
{
    auto loc = TranslateLocation(expr);
    auto arrayTy = chirTy.TranslateType(expr.GetTy());
    CJC_ASSERT(arrayTy->IsRef());
    auto eleTy = StaticCast<RawArrayType>(arrayTy->GetPureType())->GetElementType();

    auto sizeVal = TranslateExprArg(*expr.args[0]);
    auto initVal = TranslateExprArg(*expr.args[1]);
    auto rawArrayRef =
        CreateAndAppendExpression<RawArrayAllocate>(loc, arrayTy, eleTy, sizeVal, currentBlock)->GetResult();
    CreateAndAppendExpression<RawArrayInitByValue>(
        loc, builder.GetUnitTy(), rawArrayRef, sizeVal, initVal, currentBlock);
    return rawArrayRef;
}

// fuzzyParentType should be data type
// return type should be data type
CHIR::Type* Translator::GetExactParentType(
    Type& fuzzyParentType, const AST::FuncDecl& resolvedFunction, FuncType& funcType,
    std::vector<Type*>& funcInstTypeArgs, bool checkAbstractMethod)
{
    if (fuzzyParentType.IsNothing()) {
        return &fuzzyParentType;
    }
    auto outerDecl = resolvedFunction.outerDecl;
    CJC_NULLPTR_CHECK(outerDecl);
    if (outerDecl->TestAttr(AST::Attribute::GENERIC_INSTANTIATED)) {
        Type* parentTy = nullptr;
        if (outerDecl->astKind == AST::ASTKind::EXTEND_DECL) {
            parentTy = TranslateType(StaticCast<AST::ExtendDecl*>(outerDecl)->extendedType->DataTy());
        } else {
            parentTy = TranslateType(outerDecl->DataTy());
        }
        return parentTy->GetPureType();
    }

    auto funcName = resolvedFunction.identifier.Val();
    auto isStatic = resolvedFunction.TestAttr(AST::Attribute::STATIC);
    CHIR::Type* result = nullptr;
    if (auto genericTy = DynamicCast<GenericType*>(&fuzzyParentType)) {
        auto& upperBounds = genericTy->GetUpperBounds();
        CJC_ASSERT(!upperBounds.empty());
        for (auto upperBound : upperBounds) {
            ClassType* upperClassType = StaticCast<ClassType*>(upperBound->GetPureType());
            result = GetExactParentType(
                *upperClassType, resolvedFunction, funcType, funcInstTypeArgs, checkAbstractMethod);
            if (result != nullptr) {
                break;
            }
        }
    } else if (auto classTy = DynamicCast<CustomType*>(&fuzzyParentType)) {
        result =
            classTy->GetExactParentType(funcName, funcType, isStatic, funcInstTypeArgs, builder, checkAbstractMethod);
    } else {
        std::unordered_map<const GenericType*, Type*> replaceTable;
        auto classInstArgs = fuzzyParentType.GetTypeArgs();
        auto extendDefs = fuzzyParentType.GetExtends(&builder);
        CJC_ASSERT(!extendDefs.empty());
        for (auto ex : extendDefs) {
            auto classGenericArgs = ex->GetExtendedType()->GetTypeArgs();
            CJC_ASSERT(classInstArgs.size() == classGenericArgs.size());
            for (size_t i = 0; i < classInstArgs.size(); ++i) {
                if (auto genericTy1 = DynamicCast<GenericType*>(classGenericArgs[i])) {
                    replaceTable.emplace(genericTy1, classInstArgs[i]);
                }
            }
            auto func = ex->GetExpectedFunc(
                funcName, funcType, isStatic, replaceTable, funcInstTypeArgs, builder, checkAbstractMethod);
            if (func != nullptr && func->Get<WrappedRawMethod>() == nullptr) {
                return ReplaceRawGenericArgType(*ex->GetExtendedType(), replaceTable, builder);
            }
        }
        for (auto ex : extendDefs) {
            for (auto ty : ex->GetImplementedInterfaceTys()) {
                result = ty->GetExactParentType(
                    funcName, funcType, isStatic, funcInstTypeArgs, builder, checkAbstractMethod);
                if (result != nullptr) {
                    return result;
                }
            }
        }
    }
    return result;
}

Ptr<Value> Translator::InitArrayByCollection(const ArrayExpr& array)
{
    auto collection = TranslateExprArg(*array.args[0]);
    auto sizeTy = builder.GetInt64Ty();
    auto invokeInfo = InvokeCallContext{
        .caller = collection,
        .funcCallCtx = FuncCallContext{
            .thisType = collection->GetType()->GetPureType()
        },
        .virMethodCtx = FuncSigInfo{
            .funcName = "$sizeget",
            .funcType = builder.GetType<FuncType>(std::vector<Type*>{collection->GetType()}, sizeTy)
        }
    };
    auto loc = TranslateLocation(array);
    Value* sizeVal = TryCreate<Invoke>(currentBlock, loc, sizeTy, invokeInfo)->GetResult();

    auto arrayTy = chirTy.TranslateType(array.GetTy());
    CJC_ASSERT(arrayTy->IsRef());
    auto eleTy = StaticCast<RawArrayType>(arrayTy->GetPureType())->GetElementType();
    auto rawArrayRef = CreateAndAppendExpression<RawArrayAllocate>(
        loc, arrayTy, eleTy, sizeVal, currentBlock)->GetResult();

    // Call the `Core::arrayInitByCollection` to set the array element value
    CJC_NULLPTR_CHECK(array.initFunc);
    auto initFn = GetSymbolTable(*array.initFunc);
    std::vector<Type*> instParamTys;
    instParamTys.emplace_back(rawArrayRef->GetType());
    instParamTys.emplace_back(collection->GetType());
    auto instFuncTy = builder.GetType<FuncType>(instParamTys, arrayTy);
    // if array init func is generic decl, then we will create `Apply` expr like: `Apply(init<xxx>, args)`
    // if array init func is instantiated decl, then we will create `Apply` expr like: `Apply(init, args)`
    auto funcCallContext = FuncCallContext{
        .args = std::vector<Value*>{rawArrayRef, collection},
        .instTypeArgs =
            array.initFunc->TestAttr(AST::Attribute::GENERIC) ? std::vector<Type*>{eleTy} : std::vector<Type*>{},
        .thisType = nullptr
    };
    CreateAndAppendApplyCallFromArray(*initFn, funcCallContext, *instFuncTy, array);
    return rawArrayRef;
}

Ptr<Value> Translator::InitVArrayByItem(const AST::ArrayExpr& vArray)
{
    auto loc = TranslateLocation(vArray);
    auto vArrayTy = chirTy.TranslateType(vArray.GetTy());
    auto pureVArrayTy = StaticCast<VArrayType*>(vArrayTy->GetPureType());
    auto eleTy = pureVArrayTy->GetElementType();

    auto sizeVal = CreateAndAppendConstantExpression<IntLiteral>(
        builder.GetInt64Ty(), *currentBlock, pureVArrayTy->GetSize())->GetResult();
    auto valArg = vArray.args[0].get();
    auto val = TranslateExprArg(*valArg);

    // todo: optimize if val is constant
    auto fnTy = builder.GetType<FuncType>(std::vector<Type*>({builder.GetInt64Ty()}), eleTy);
    auto nullFn = CreateAndAppendConstantExpression<NullLiteral>(fnTy, *currentBlock)->GetResult();
    return CreateAndAppendExpression<VArrayBuilder>(loc, vArrayTy, sizeVal, val, nullFn, currentBlock)->GetResult();
}

Ptr<Value> Translator::InitVArrayByLambda(const AST::ArrayExpr& vArray)
{
    auto loc = TranslateLocation(vArray);
    auto vArrayTy = chirTy.TranslateType(vArray.GetTy());
    auto pureVArrayTy = StaticCast<VArrayType*>(vArrayTy->GetPureType());
    auto eleTy = pureVArrayTy->GetElementType();
    auto sizeVal = CreateAndAppendConstantExpression<IntLiteral>(
        builder.GetInt64Ty(), *currentBlock, pureVArrayTy->GetSize())->GetResult();
    auto initFn = TranslateExprArg(*vArray.args[0]);
    auto nullItem = CreateAndAppendConstantExpression<NullLiteral>(eleTy, *currentBlock)->GetResult();
    return CreateAndAppendExpression<VArrayBuilder>(loc, vArrayTy, sizeVal, nullItem, initFn, currentBlock)
        ->GetResult();
}
