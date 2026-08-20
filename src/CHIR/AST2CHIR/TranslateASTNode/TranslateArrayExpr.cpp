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

Ptr<Value> Translator::Visit(const AST::ArrayExpr& array)
{
    if (array.isValueArray) {
        CJC_ASSERT(array.args.size() == 1);
        CJC_ASSERT(!array.GetTy()->typeArgs.empty());

        if (array.args[0]->GetTy()->IsFunc()) {
            // Case A: "VArray<Int64, $5>({i => i})"
            return InitVArrayByLambda(array);
        } else {
            // Case B: "VArray<Int64, $5>(repeat: 0)"
            return InitVArrayByItem(array);
        }
    }
    CJC_ABORT();
    return nullptr;
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
        return builder.WithModal(parentTy->StripAllRefs(), fuzzyParentType.GetModalInfo());
    }

    auto funcName = resolvedFunction.identifier.Val();
    auto isStatic = resolvedFunction.TestAttr(AST::Attribute::STATIC);
    CHIR::Type* result = nullptr;
    if (auto genericTy = DynamicCast<GenericType*>(&fuzzyParentType)) {
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
    } else if (auto classTy = DynamicCast<CustomType*>(&fuzzyParentType)) {
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
                return ReplaceRawGenericArgType(*ex->GetExtendedType(), replaceTable, builder);
            }
        }
        // extend def's super interface
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
<<<<<<< HEAD
    return result;
}

Ptr<Value> Translator::InitArrayByCollection(const AST::ArrayExpr& array)
{
    auto collection = TranslateExprArg(*array.args[0]);
    auto sizeTy = builder.GetInt64Ty();
    Type* originalObjType =
        StaticCast<CustomType*>(collection->GetType()->StripAllRefs())->GetCustomTypeDef()->GetType();
    originalObjType = builder.GetType<RefType>(originalObjType);
    auto funcType = builder.GetType<FuncType>(std::vector<Type*>({originalObjType}), sizeTy);
    auto collectionType = collection->GetType()->StripAllRefs();
    std::vector<Type*> emptyInstTypeArgs;
    auto sizeGetFunc = collectionType->GetExpectedFunc("$sizeget", *funcType, false, emptyInstTypeArgs, builder, true);
    CJC_NULLPTR_CHECK(sizeGetFunc);
    auto invokeInfo = InvokeCallContext {
        .method = sizeGetFunc,
        .caller = collection,
        .funcCallCtx = FuncCallContext {
            .thisType = collection->GetType()->StripAllRefs()
        }
    };
    auto loc = TranslateLocation(array);
    Value* sizeVal = TryCreate<Invoke>(currentBlock, loc, sizeTy, invokeInfo)->GetResult();

    // Create the array `RawArrayAllocate(eleTy, collection.size)`
    auto arrayTy = chirTy.TranslateType(*array.GetTy());
    CJC_ASSERT(arrayTy->IsRef());
    auto eleTy = StaticCast<RawArrayType*>(arrayTy->StripAllRefs())->GetElementType();
    auto rawArrayRef =
        CreateAndAppendExpression<RawArrayAllocate>(loc, arrayTy, eleTy, sizeVal, currentBlock)->GetResult();

    // Call the `Core::arrayInitByCollection` to set the array element value
    CJC_NULLPTR_CHECK(array.initFunc);
    auto initFn = GetSymbolTable(*array.initFunc);
    // what are the initFn here all normal constructor or the arrayInitByFunc/arrayInitByCollection
    // check the thisType and instParentCustomDefTy
    std::vector<Type*> instParamTys;
    instParamTys.emplace_back(rawArrayRef->GetType());
    instParamTys.emplace_back(collection->GetType());
    auto instFuncTy = builder.GetType<FuncType>(instParamTys, arrayTy);
    // if array init func is generic decl, then we will create `Apply` expr like: `Apply(init<xxx>, args)`
    // if array init func is instantiated decl, then we will create `Apply` expr like: `Apply(init, args)`
    auto funcCallContext = FuncCallContext {
        .args = std::vector<Value*>{rawArrayRef, collection},
        .instTypeArgs =
            array.initFunc->TestAttr(AST::Attribute::GENERIC) ? std::vector<Type*>{eleTy} : std::vector<Type*>{},
        .thisType = nullptr
    };
    CreateAndAppendApplyCallFromArray(*initFn, funcCallContext, *instFuncTy, array);
    return rawArrayRef;
=======
    return builder.WithModal(result, fuzzyParentType.GetModalInfo());
>>>>>>> 0c41e6d (feat: add local mode implementation)
}

Ptr<Value> Translator::InitVArrayByItem(const AST::ArrayExpr& vArray)
{
    auto loc = TranslateLocation(vArray);
    auto vArrayTy = chirTy.TranslateType(vArray.GetTy());
    auto pureVArrayTy = StaticCast<VArrayType*>(vArrayTy->StripAllRefs());
    auto eleTy = pureVArrayTy->GetElementType();

<<<<<<< HEAD
    auto size = vArrayTy->GetSize();
    auto sizeVal =
        CreateAndAppendConstantExpression<IntLiteral>(builder.GetInt64Ty(), *currentBlock, static_cast<uint64_t>(size))
            ->GetResult();
=======
    auto sizeVal = CreateAndAppendConstantExpression<IntLiteral>(
        builder.GetInt64Ty(), *currentBlock, pureVArrayTy->GetSize())->GetResult();
>>>>>>> 0c41e6d (feat: add local mode implementation)
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
<<<<<<< HEAD
    auto vArrayTy = StaticCast<VArrayType*>(chirTy.TranslateType(*vArray.GetTy()));
    auto eleTy = vArrayTy->GetElementType();
    auto size = vArrayTy->GetSize();
    auto sizeVal =
        CreateAndAppendConstantExpression<IntLiteral>(builder.GetInt64Ty(), *currentBlock, static_cast<uint64_t>(size))
            ->GetResult();
=======
    auto vArrayTy = chirTy.TranslateType(vArray.GetTy());
    auto pureVArrayTy = StaticCast<VArrayType*>(vArrayTy->StripAllRefs());
    auto eleTy = pureVArrayTy->GetElementType();
    auto sizeVal = CreateAndAppendConstantExpression<IntLiteral>(
        builder.GetInt64Ty(), *currentBlock, pureVArrayTy->GetSize())->GetResult();
>>>>>>> 0c41e6d (feat: add local mode implementation)
    auto initFn = TranslateExprArg(*vArray.args[0]);
    auto nullItem = CreateAndAppendConstantExpression<NullLiteral>(eleTy, *currentBlock)->GetResult();
    return CreateAndAppendExpression<VArrayBuilder>(loc, vArrayTy, sizeVal, nullItem, initFn, currentBlock)
        ->GetResult();
}
