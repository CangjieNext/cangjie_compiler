// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/AST/ASTCasting.h"
#include "cangjie/CHIR/AST2CHIR/TranslateASTNode/Translator.h"
#include "cangjie/CHIR/IR/Expression/Terminator.h"
#include "cangjie/CHIR/IR/Type/Type.h"

using namespace Cangjie::CHIR;
using namespace Cangjie;
using namespace Cangjie::AST;

constexpr static int ARGS_NUM_TWO = 2;

Expression* Translator::CreateAndAppendApplyCallFromCallExpr(
    Value& callee, FuncCallContext& context, const FuncType& instFuncTy, const CallExpr& expr)
{
    auto funcCall = TryCreate<Apply>(currentBlock, instFuncTy.GetReturnType(), &callee, context);
    const auto& loc = TranslateLocation(expr);
    funcCall->SetDebugLocation(loc);
    if (expr.callKind == CallKind::CALL_SUPER_FUNCTION) {
        if (auto apply = DynamicCast<Apply*>(funcCall)) {
            apply->SetSuperCall();
        }
    }
    if (HasNothingTypeArg(context.args)) {
        if (expr.baseFunc != nullptr) {
            const auto& warningLoc = TranslateLocation(*expr.baseFunc);
            funcCall->Set<DebugLocationInfoForWarning>(warningLoc);
        } else {
            funcCall->Set<DebugLocationInfoForWarning>(loc);
        }
    }
    return funcCall;
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

Ptr<Value> Translator::InitArrayByLambda(const CallExpr& callExpr)
{
    auto loc = TranslateLocation(callExpr);
    auto arrayTy = chirTy.TranslateType(*callExpr.GetTy());
    CJC_ASSERT(arrayTy->IsRef());
    auto eleTy = StaticCast<RawArrayType*>(StaticCast<CHIR::RefType*>(arrayTy)->GetBaseType())->GetElementType();
    auto sizeVal = TranslateExprArg(*callExpr.args[0]);
    auto initFn = GetSymbolTable(*callExpr.resolvedFunction);
    auto rawArrayExpr = CreateAndAppendExpression<RawArrayAllocate>(loc, arrayTy, eleTy, sizeVal, currentBlock);
    auto rawArrayRef = rawArrayExpr->GetResult();

    std::vector<Type*> instantiatedTypeArgs;
    // if array init func is generic decl, then we will create `Apply` expr like: `Apply(init<xxx>, args)`
    // if array init func is instantiated decl, then we will create `Apply` expr like: `Apply(init, args)`
    if (callExpr.resolvedFunction->TestAttr(AST::Attribute::GENERIC)) {
        for (auto ty : callExpr.GetTy()->typeArgs) {
            instantiatedTypeArgs.emplace_back(chirTy.TranslateType(*ty));
        }
    }
    auto userInitFn = TranslateExprArg(*callExpr.args[1]);
    // what are the initFn here all normal constructor or the arrayInitByFunc/arrayInitByCollection
    // check the thisType and instParentCustomDefTy
    std::vector<Type*> instParamTys;
    instParamTys.emplace_back(rawArrayRef->GetType());
    instParamTys.emplace_back(userInitFn->GetType());
    auto instFuncTy = builder.GetType<FuncType>(instParamTys, arrayTy);
    auto funcCallContext = FuncCallContext {
        .args = std::vector<Value*>{rawArrayRef, userInitFn},
        .instTypeArgs = instantiatedTypeArgs,
        .thisType = nullptr
    };
    CreateAndAppendApplyCallFromCallExpr(*initFn, funcCallContext, *instFuncTy, callExpr);

    return rawArrayRef;
}

Ptr<Value> Translator::InitArrayByItem(const CallExpr& callExpr)
{
    auto loc = TranslateLocation(callExpr);
    auto arrayTy = chirTy.TranslateType(*callExpr.GetTy());
    CJC_ASSERT(arrayTy->IsRef());
    auto eleTy = StaticCast<RawArrayType*>(StaticCast<CHIR::RefType*>(arrayTy)->GetBaseType())->GetElementType();

    auto sizeVal = TranslateExprArg(*callExpr.args[0]);
    auto initVal = TranslateExprArg(*callExpr.args[1]);
    auto rawArrayRef =
        CreateAndAppendExpression<RawArrayAllocate>(loc, arrayTy, eleTy, sizeVal, currentBlock)->GetResult();
    CreateAndAppendExpression<RawArrayInitByValue>(
        loc, builder.GetUnitTy(), rawArrayRef, sizeVal, initVal, currentBlock);
    return rawArrayRef;
}

CHIR::Type* Translator::GetExactParentType(Type& fuzzyParentType, const FuncDecl& resolvedFunction, FuncType& funcType,
    std::vector<Type*>& funcInstTypeArgs, bool checkAbstractMethod)
{
    if (fuzzyParentType.IsNothing()) {
        return &fuzzyParentType;
    }
    auto outerDecl = resolvedFunction.outerDecl;
    CJC_NULLPTR_CHECK(outerDecl);
    if (outerDecl->TestAttr(AST::Attribute::GENERIC_INSTANTIATED)) {
        Type* parentTy = nullptr;
        if (outerDecl->astKind == ASTKind::EXTEND_DECL) {
            parentTy = TranslateType(*StaticCast<ExtendDecl*>(outerDecl)->extendedType->GetTy());
        } else {
            parentTy = TranslateType(*outerDecl->GetTy());
        }
        return builder.SubstituteModal(parentTy->StripAllRefs(), fuzzyParentType.Modal());
    }
    auto dataTy = builder.SubstituteModal(&fuzzyParentType, {});
    auto funcName = resolvedFunction.identifier.Val();
    auto isStatic = resolvedFunction.TestAttr(AST::Attribute::STATIC);
    CHIR::Type* result = nullptr;
    if (auto genericTy = DynamicCast<GenericType*>(dataTy)) {
        auto& upperBounds = genericTy->GetUpperBounds();
        CJC_ASSERT(!upperBounds.empty());
        for (auto upperBound : upperBounds) {
            ClassType* upperClassType = StaticCast<ClassType*>(StaticCast<RefType*>(upperBound)->GetBaseType());
            result = GetExactParentType(
                *upperClassType, resolvedFunction, funcType, funcInstTypeArgs, checkAbstractMethod);
            if (result != nullptr) {
                break;
            }
        }
    } else if (auto classTy = DynamicCast<CustomType*>(dataTy)) {
        result =
            classTy->GetExactParentType(funcName, funcType, isStatic, funcInstTypeArgs, builder, checkAbstractMethod);
    } else {
        std::unordered_map<const GenericType*, Type*> replaceTable;
        auto classInstArgs = fuzzyParentType.GetTypeArgs();
        auto extendDefs = fuzzyParentType.GetExtends(&builder);
        CJC_ASSERT(!extendDefs.empty());
        // extend def
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
                auto ty = ReplaceRawGenericArgType(*ex->GetExtendedType(), replaceTable, builder);
                return builder.SubstituteModal(ty, fuzzyParentType.Modal());
            }
        }
        // extend def's super interface
        for (auto ex : extendDefs) {
            for (auto ty : ex->GetImplementedInterfaceTys()) {
                result = ty->GetExactParentType(
                    funcName, funcType, isStatic, funcInstTypeArgs, builder, checkAbstractMethod);
                if (result != nullptr) {
                    return builder.SubstituteModal(result, fuzzyParentType.Modal());
                }
            }
        }
    }
    if (result) {
        result = builder.SubstituteModal(result, fuzzyParentType.Modal());
    }
    return result;
}

Ptr<Value> Translator::InitArrayByCollection(const CallExpr& callExpr)
{
    auto collection = TranslateExprArg(*callExpr.args[0]);
    auto sizeTy = builder.GetInt64Ty();
    Type* originalObjType =
        StaticCast<CustomType*>(collection->GetType()->StripAllRefs())->GetCustomTypeDef()->GetType();
    originalObjType = builder.GetType<RefType>(originalObjType);
    auto invokeInfo = InvokeCallContext {
        .caller = collection,
        .funcCallCtx = FuncCallContext {
            .thisType = collection->GetType()->StripAllRefs()
        },
        .virMethodCtx = VirMethodContext {
            .srcCodeIdentifier = "$sizeget",
            .originalFuncType = builder.GetType<FuncType>(std::vector<Type*>({originalObjType}), sizeTy)
        }
    };
    auto loc = TranslateLocation(callExpr);
    Value* sizeVal = TryCreate<Invoke>(currentBlock, loc, sizeTy, invokeInfo)->GetResult();

    // Create the array `RawArrayAllocate(eleTy, collection.size)`
    auto arrayTy = chirTy.TranslateType(*callExpr.GetTy());
    CJC_ASSERT(arrayTy->IsRef());
    auto eleTy = StaticCast<RawArrayType*>(arrayTy->StripAllRefs())->GetElementType();
    auto rawArrayRef =
        CreateAndAppendExpression<RawArrayAllocate>(loc, arrayTy, eleTy, sizeVal, currentBlock)->GetResult();

    // Call the `Core::arrayInitByCollection` to set the array element value
    auto initFn = GetSymbolTable(*callExpr.resolvedFunction);
    // what are the initFn here all normal constructor or the arrayInitByFunc/arrayInitByCollection
    // check the thisType and instParentCustomDefTy
    std::vector<Type*> instParamTys;
    instParamTys.emplace_back(rawArrayRef->GetType());
    instParamTys.emplace_back(collection->GetType());
    auto instFuncTy = builder.GetType<FuncType>(instParamTys, arrayTy);
    // if array init func is generic decl, then we will create `Apply` expr like: `Apply(init<xxx>, args)`
    // if array init func is instantiated decl, then we will create `Apply` expr like: `Apply(init, args)`
    auto funcCallContext = FuncCallContext{.args = std::vector<Value*>{rawArrayRef, collection},
        .instTypeArgs = callExpr.resolvedFunction->TestAttr(AST::Attribute::GENERIC) ? std::vector<Type*>{eleTy}
                                                                                     : std::vector<Type*>{},
        .thisType = nullptr};
    CreateAndAppendApplyCallFromCallExpr(*initFn, funcCallContext, *instFuncTy, callExpr);
    return rawArrayRef;
}

Ptr<Value> Translator::InitVArrayByItem(const ArrayExpr& vArray)
{
    auto loc = TranslateLocation(vArray);
    auto vArrayTy = StaticCast<VArrayType*>(chirTy.TranslateType(*vArray.GetTy()));
    auto eleTy = vArrayTy->GetElementType();

    auto sizeVal =
        CreateAndAppendConstantExpression<IntLiteral>(builder.GetInt64Ty(), *currentBlock, vArrayTy->GetSize())
            ->GetResult();
    auto valArg = vArray.args[0].get();
    auto val = TranslateExprArg(*valArg);

    // todo: optimize if val is constant
    auto fnTy = builder.GetType<FuncType>(std::vector<Type*>({builder.GetInt64Ty()}), eleTy);
    auto nullFn = CreateAndAppendConstantExpression<NullLiteral>(fnTy, *currentBlock)->GetResult();
    return CreateAndAppendExpression<VArrayBuilder>(loc, vArrayTy, sizeVal, val, nullFn, currentBlock)->GetResult();
}

Ptr<Value> Translator::InitVArrayByLambda(const ArrayExpr& vArray)
{
    auto loc = TranslateLocation(vArray);
    auto vArrayTy = StaticCast<VArrayType*>(chirTy.TranslateType(*vArray.GetTy()));
    auto eleTy = vArrayTy->GetElementType();
    auto sizeVal =
        CreateAndAppendConstantExpression<IntLiteral>(builder.GetInt64Ty(), *currentBlock, vArrayTy->GetSize())
            ->GetResult();
    auto initFn = TranslateExprArg(*vArray.args[0]);
    auto nullItem = CreateAndAppendConstantExpression<NullLiteral>(eleTy, *currentBlock)->GetResult();
    return CreateAndAppendExpression<VArrayBuilder>(loc, vArrayTy, sizeVal, nullItem, initFn, currentBlock)
        ->GetResult();
}

Ptr<Value> Translator::Visit(const ArrayExpr& arrayExpr)
{
    if (arrayExpr.desugarExpr != nullptr) {
        return TranslateASTNode(*arrayExpr.desugarExpr, *this);
    }
    if (!arrayExpr.isValueArray) {
        InternalError("translating unsupported ArrayExpr");
        return nullptr;
    }
    const auto& loc = TranslateLocation(arrayExpr);
    auto vArrayTy = StaticCast<VArrayType*>(chirTy.TranslateType(*arrayExpr.GetTy()));
    auto eleTy = vArrayTy->GetElementType();
    auto fnTy = builder.GetType<FuncType>(std::vector<Type*>({builder.GetInt64Ty()}), eleTy);
    if (arrayExpr.args.empty()) {
        auto sizeVal =
            CreateAndAppendConstantExpression<IntLiteral>(builder.GetInt64Ty(), *currentBlock, vArrayTy->GetSize())
                ->GetResult();
        auto val = CreateAndAppendConstantExpression<NullLiteral>(eleTy, *currentBlock)->GetResult();
        auto nullFn = CreateAndAppendConstantExpression<NullLiteral>(fnTy, *currentBlock)->GetResult();
        return CreateAndAppendExpression<VArrayBuilder>(loc, vArrayTy, sizeVal, val, nullFn, currentBlock)->GetResult();
    }
    if (arrayExpr.args.size() == 1) {
        if (Ty::IsTyCorrect(arrayExpr.args[0]->GetTy()) && arrayExpr.args[0]->GetTy()->IsFunc()) {
            return InitVArrayByLambda(arrayExpr);
        }
        return InitVArrayByItem(arrayExpr);
    }
    if (arrayExpr.args.size() == ARGS_NUM_TWO) {
        auto sizeVal = TranslateExprArg(*arrayExpr.args[0]);
        if (Ty::IsTyCorrect(arrayExpr.args[1]->GetTy()) && arrayExpr.args[1]->GetTy()->IsFunc()) {
            auto nullItem = CreateAndAppendConstantExpression<NullLiteral>(eleTy, *currentBlock)->GetResult();
            auto initFn = TranslateExprArg(*arrayExpr.args[1]);
            return CreateAndAppendExpression<VArrayBuilder>(loc, vArrayTy, sizeVal, nullItem, initFn, currentBlock)
                ->GetResult();
        }
        auto val = TranslateExprArg(*arrayExpr.args[1]);
        auto nullFn = CreateAndAppendConstantExpression<NullLiteral>(fnTy, *currentBlock)->GetResult();
        return CreateAndAppendExpression<VArrayBuilder>(loc, vArrayTy, sizeVal, val, nullFn, currentBlock)->GetResult();
    }
    InternalError("translating unsupported ArrayExpr");
    return nullptr;
}
