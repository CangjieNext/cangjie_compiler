// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "Base/CGTypes/CGFunctionType.h"

#include "Base/CGTypes/CGGenericType.h"
#include "CGContext.h"
#include "CGModule.h"
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
#include "CJNative/CGCFFI.h"
#endif
#include "Utils/CGUtils.h"
#include "cangjie/CHIR/Utils/CHIRCasting.h"
#include "cangjie/CHIR/IR/Value/Value.h"

namespace Cangjie::CodeGen {
namespace {

llvm::StructType* GetOrCreateFuncLayoutType(llvm::LLVMContext& llvmCtx)
{
    if (auto layoutType = llvm::StructType::getTypeByName(llvmCtx, "Func.Type")) {
        return layoutType;
    }
    return llvm::StructType::create(llvmCtx, {llvm::Type::getInt8PtrTy(llvmCtx)}, "Func.Type");
}

bool IsRefToStructOrTuple(const CHIR::Type& ty)
{
    return ty.IsRef() && (ty.GetTypeArgs()[0]->IsStruct() || ty.GetTypeArgs()[0]->IsTuple());
}

bool NeedsPassByRef(const CHIR::Type& ty, const CGType& cgType)
{
    return ty.IsStruct() || ty.IsTuple() || (!cgType.GetSize() && !ty.IsGeneric());
}
} // namespace

CGFunctionType::CGFunctionType(
    CGModule& cgMod, CGContext& cgCtx, const CHIR::FuncType& chirType, const TypeExtraInfo& extraInfo)
    : CGType(cgMod, cgCtx, chirType, CGTypeKind::CG_FUNCTION)
{
    CJC_ASSERT(chirType.IsFunc());
    isMethod = extraInfo.isMethod;
    isStaticMethod = extraInfo.isStaticMethod;
    instantiatedParamTypes = extraInfo.instantiatedParamTypes;
}

CGFunctionType::CGFunctionType(
    CGModule& cgMod, CGContext& cgCtx, const CHIR::Function& chirFunc, const TypeExtraInfo& extraInfo)
    : CGType(cgMod, cgCtx,
          chirFunc.Get<CHIR::OverrideSrcFuncType>() ? *chirFunc.Get<CHIR::OverrideSrcFuncType>() : *chirFunc.GetType(),
          CGTypeKind::CG_FUNCTION)
{
    this->chirFunc = &chirFunc;
    this->isStaticMethod = chirFunc.TestAttr(CHIR::Attribute::STATIC);
    this->isMethod = chirFunc.GetParentCustomTypeDef();
    this->allowBasePtr = extraInfo.allowBasePtr;
    for (auto gt : chirFunc.GetGenericTypeParams()) {
        this->instantiatedParamTypes.emplace_back(gt);
    }
}

llvm::Type* CGFunctionType::GenLLVMType()
{
    if (llvmType) {
        return llvmType;
    }
    auto& llvmCtx = cgCtx.GetLLVMContext();
    layoutType = GetOrCreateFuncLayoutType(llvmCtx);

    auto& funcTy = StaticCast<const CHIR::FuncType&>(chirType);
    if (funcTy.IsCFunc()) {
        return GenLLVMTypeForCFunc(funcTy);
    }
    llvmType = llvm::Type::getInt8PtrTy(llvmCtx);

    std::vector<llvm::Type*> paramTypes;
    size_t realArgIdx = 0;
    size_t containedCGTypeIndex = 1;
    auto retType = GenReturnLLVMType(funcTy, paramTypes, realArgIdx);
    for (auto paramType : funcTy.GetParamTypes()) {
        (void)realParamIndices.emplace_back(realArgIdx);
        AppendParamLLVMType(*paramType, containedCGTypeIndex++, paramTypes, realArgIdx);
    }
    AppendSupplementaryParamLLVMTypes(paramTypes, realArgIdx);

    llvmFunctionType = llvm::FunctionType::get(retType, paramTypes, funcTy.HasVarArg());
    return llvmType;
}

llvm::Type* CGFunctionType::GenLLVMTypeForCFunc(const CHIR::FuncType& funcTy)
{
    auto& llvmCtx = cgCtx.GetLLVMContext();
    llvmType = llvm::PointerType::get(llvm::StructType::get(llvmCtx), 0);
    auto llvmFuncTy = cgMod.GetCGCFFI().GetCFuncType(funcTy);
    return llvmFuncTy ? llvmFuncTy->getPointerTo() : llvmType;
}

llvm::Type* CGFunctionType::GenReturnLLVMType(
    const CHIR::FuncType& funcTy, std::vector<llvm::Type*>& paramTypes, size_t& realArgIdx)
{
    auto retCGType = CGType::GetOrCreate(cgMod, funcTy.GetReturnType());
    llvm::Type* retType = retCGType->GetLLVMType();
    if (!retType->isStructTy() && !retType->isArrayTy() && retCGType->GetSize()) {
        return retType;
    }

    if (!retCGType->GetSize() && !funcTy.GetReturnType()->IsGeneric()) {
        (void)paramTypes.emplace_back(retType->getPointerTo(1));
    } else {
        (void)paramTypes.emplace_back(retType->getPointerTo());
    }
    ++realArgIdx;
    hasSRet = true;
    return llvm::Type::getVoidTy(cgCtx.GetLLVMContext());
}

void CGFunctionType::AppendParamLLVMType(const CHIR::Type& paramType, size_t containedCGTypeIndex,
    std::vector<llvm::Type*>& paramTypes, size_t& realArgIdx)
{
    auto cgType = CGType::GetOrCreate(cgMod, &paramType);
    if (IsRefToStructOrTuple(paramType)) {
        /// Effect Overview
        // The parameter that requires additional basePtr must meet and the function should not be 'mut'
        // either:
        //  - (1) Is a pointer to llvm::StructType or llvm::ArrayType.
        // or:
        //  - (2) Is llvm::StructType or llvm::ArrayType and is the first parameter of function.
        //    hint: This is designed to make it easier to invoke methods through reflect by runtime.
        //
        // The type of basePtr is determined according to the following rules:
        //  - For above (1), basePtr is i8 addrspace(1)*.
        //  - For above (2), basePtr is i8*.
        auto llvmType = CGType::GetOrCreate(cgMod, DeRef(paramType))->GetLLVMType()->getPointerTo(1U);
        (void)paramTypes.emplace_back(llvmType);
        if (allowBasePtr) {
            (void)paramTypes.emplace_back(llvm::Type::getInt8PtrTy(cgCtx.GetLLVMContext(), 1));
            structParamNeedsBasePtr[realArgIdx] = containedCGTypeIndex;
            ++realArgIdx;
            hasBasePtr = true;
        }
    } else if (NeedsPassByRef(paramType, *cgType)) {
        // If current parameter is the first parameter in a struct method, aka `this`
        llvm::Type* llvmType = nullptr;
        if (containedCGTypeIndex == 1U && chirFunc && !chirFunc->TestAttr(CHIR::Attribute::STATIC) &&
            IsStructOrExtendMethod(*chirFunc)) {
            llvmType = cgType->GetLLVMType()->getPointerTo();
        } else {
            llvmType = cgType->GetLLVMType()->getPointerTo(cgType->GetSize() ? 0U : 1U);
        }
        (void)paramTypes.emplace_back(llvmType);
    } else if (cgType->IsStructType() || cgType->IsVArrayType()) {
        (void)paramTypes.emplace_back(cgType->GetLLVMType()->getPointerTo());
    } else {
        (void)paramTypes.emplace_back(cgType->GetLLVMType());
    }
    ++realArgIdx;
}

void CGFunctionType::AppendSupplementaryParamLLVMTypes(std::vector<llvm::Type*>& paramTypes, size_t& realArgIdx)
{
    // Adding Supplementary parameters
    if (this->chirFunc) { // Generic parameters introduced by a function
        for (auto chirGT : this->chirFunc->GetGenericTypeParams()) {
            (void)paramTypes.emplace_back(CGType::GetOrCreateTypeInfoPtrType(cgCtx.GetLLVMContext()));
            genericParamIndicesMap.emplace(chirGT, realArgIdx++); // map chirGT to ArgIdx
        }
    } else {
        for (std::size_t idx = 0; idx < this->instantiatedParamTypes.size(); ++idx) {
            (void)paramTypes.emplace_back(CGType::GetOrCreateTypeInfoPtrType(cgCtx.GetLLVMContext()));
            realArgIdx++;
        }
    }
    if (this->isMethod) { // For member method, add OuterTypeInfo
        (void)paramTypes.emplace_back(CGType::GetOrCreateTypeInfoPtrType(cgCtx.GetLLVMContext()));
        outerTypeInfoIndex = realArgIdx++;
    }
    if (this->isStaticMethod) { // For static member method, add ThisTypeInfo
        (void)paramTypes.emplace_back(CGType::GetOrCreateTypeInfoPtrType(cgCtx.GetLLVMContext()));
        thisTypeInfoIndex = realArgIdx++;
    }
}

llvm::FunctionType* CGFunctionType::GetLLVMFunctionType() const
{
    if (auto& chirFuncTy = StaticCast<const CHIR::FuncType&>(chirType); chirFuncTy.IsCFunc()) {
        return cgMod.GetCGCFFI().GetCFuncType(chirFuncTy);
    }
    return llvmFunctionType;
}

void CGFunctionType::GenContainedCGTypes()
{
    // The contained types of a function type include:
    auto& funcTy = StaticCast<const CHIR::FuncType&>(chirType);
    size_t containedCGTypeIndex = 0;
    // (1) return type
    (void)containedCGTypes.emplace_back(GetFixedParamCGType(*funcTy.GetReturnType(), containedCGTypeIndex++));
    // (2) every parameter type
    for (auto paramType : funcTy.GetParamTypes()) {
        (void)containedCGTypes.emplace_back(GetFixedParamCGType(*paramType, containedCGTypeIndex++));
    }
    // (3) Supplementary parameters
    if (this->chirFunc) {
        // every generic parameter type
        for ([[maybe_unused]] const auto& it : this->chirFunc->GetGenericTypeParams()) {
            (void)containedCGTypes.emplace_back(CGType::GetCGTI(cgMod));
        }
        // For member method, add OuterTypeInfo
        if (this->chirFunc->GetOuterDeclaredOrExtendedDef()) {
            (void)containedCGTypes.emplace_back(CGType::GetCGTI(cgMod));
        }
        // For static member method, add ThisTypeInfo
        if (this->chirFunc->TestAttr(CHIR::Attribute::STATIC)) {
            (void)containedCGTypes.emplace_back(CGType::GetCGTI(cgMod));
        }
    }
}

CGType* CGFunctionType::GetFixedParamCGType(const CHIR::Type& chirType, size_t containedCGTypeIndex)
{
    // On CJNative backend, the parameters of structure type are passed by reference.
    auto cgType = CGType::GetOrCreate(cgMod, &chirType);
    if (chirType.IsRef() && chirType.GetTypeArgs()[0]->IsStruct()) {
        return CGType::GetOrCreate(cgMod, &chirType, CGType::TypeExtraInfo(1U));
    } else if (NeedsPassByRef(chirType, *cgType)) {
        auto refType = CGType::GetRefTypeOf(cgCtx.GetCHIRBuilder(), chirType);
        // If current parameter is the first parameter in a struct method, aka `this`
        if (containedCGTypeIndex == 1U && chirFunc && IsStructOrExtendMethod(*chirFunc)) {
            return CGType::GetOrCreate(cgMod, refType);
        } else {
            return CGType::GetOrCreate(cgMod, refType, CGType::TypeExtraInfo(cgType->GetSize() ? 0U : 1U));
        }
    } else if (cgType->IsStructType() || cgType->IsVArrayType()) {
        return CGType::GetOrCreate(
            cgMod, CGType::GetRefTypeOf(cgCtx.GetCHIRBuilder(), chirType), CGType::TypeExtraInfo(0U));
    } else if (cgType->IsStructPtrType() || cgType->IsVArrayPtrType()) {
        return CGType::GetOrCreate(cgMod, &chirType, CGType::TypeExtraInfo(1U));
    } else {
        return CGType::GetOrCreate(cgMod, &chirType);
    }
}

llvm::Constant* CGFunctionType::GenSourceGenericOfTypeInfo()
{
    return CGType::GenSourceGenericOfTypeInfo();
}

llvm::Constant* CGFunctionType::GenTypeArgsNumOfTypeInfo()
{
    return llvm::ConstantInt::get(llvm::Type::getInt8Ty(cgMod.GetLLVMContext()),
        static_cast<const CHIR::FuncType&>(chirType).GetParamTypes().size() + 1U); // add one for the return type
}

llvm::Constant* CGFunctionType::GenTypeArgsOfTypeInfo()
{
    auto& funcType = StaticCast<const CHIR::FuncType&>(chirType);
    auto typeInfoPtrTy = CGType::GetOrCreateTypeInfoPtrType(cgMod.GetLLVMContext());

    std::vector<llvm::Constant*> constants;
    auto returnCGType = CGType::GetOrCreate(cgMod, DeRef(*funcType.GetReturnType()));
    (void)constants.emplace_back(returnCGType->GetOrCreateTypeInfo());
    if (returnCGType->IsStaticGI()) {
        cgCtx.AddDependentPartialOrderOfTypes(constants[0], this->typeInfo);
    }
    for (auto paramType : funcType.GetParamTypes()) {
        auto paramCGType = CGType::GetOrCreate(cgMod, DeRef(*paramType));
        auto it =constants.emplace_back(paramCGType->GetOrCreateTypeInfo());
        if (paramCGType->IsStaticGI()) {
            cgCtx.AddDependentPartialOrderOfTypes(it, this->typeInfo);
        }
    }

    auto typeOfGenericArgsGV = llvm::ArrayType::get(typeInfoPtrTy, constants.size());
    auto typeInfoOfGenericArgs = llvm::cast<llvm::GlobalVariable>(cgMod.GetLLVMModule()->getOrInsertGlobal(
        CGType::GetNameOfTypeInfoGV(chirType) + ".typeArgs", typeOfGenericArgsGV));
    typeInfoOfGenericArgs->setInitializer(llvm::ConstantArray::get(typeOfGenericArgsGV, constants));
    typeInfoOfGenericArgs->setLinkage(llvm::GlobalValue::LinkageTypes::PrivateLinkage);
    typeInfoOfGenericArgs->addAttribute(CJTI_TYPE_ARGS_ATTR);
    typeInfoOfGenericArgs->setConstant(true);
    typeInfoOfGenericArgs->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
    return llvm::ConstantExpr::getBitCast(typeInfoOfGenericArgs, llvm::Type::getInt8PtrTy(cgMod.GetLLVMContext()));
}

void CGFunctionType::CalculateSizeAndAlign()
{
    llvm::DataLayout layOut = cgMod.GetLLVMModule()->getDataLayout();
    size = layOut.getTypeAllocSize(llvmType);
    align = layOut.getABITypeAlignment(llvmType);
}
} // namespace Cangjie::CodeGen
