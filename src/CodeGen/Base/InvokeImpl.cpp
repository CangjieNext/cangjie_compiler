// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements codegen for CHIR Invoke.
 */

#include "Base/InvokeImpl.h"

#include "Base/CHIRExprWrapper.h"
#include "CGModule.h"
#include "IRBuilder.h"
#include "cangjie/CHIR/IR/Expression/Terminator.h"
#include "cangjie/CHIR/IR/Type/ClassDef.h"
#include "cangjie/CHIR/IR/Type/Type.h"

using namespace Cangjie;
using namespace CodeGen;
using namespace CHIR;

namespace {
llvm::MDTuple* GenObjTyMetaForVirtualCall(IRBuilder2& irBuilder, const CHIR::Type& ty)
{
    CJC_ASSERT(ty.GetTypeArgs().size() <= 1);
    llvm::LLVMContext& llvmCtx = irBuilder.GetCGContext().GetLLVMContext();
    std::vector<llvm::Metadata*> objTyMeta{};
    if (ty.GetTypeArgs().size() == 1) {
        objTyMeta = UnwindGenericRelateType(llvmCtx, ty);
    } else {
        std::string tiName = CGType::GetNameOfTypeInfoGV(ty);
        objTyMeta.emplace_back(llvm::MDString::get(llvmCtx, tiName));
    }
    return llvm::MDTuple::get(llvmCtx, objTyMeta);
}

bool VirtualCallCanBeHoisted(IRBuilder2& irBuilder, const CHIRCallExpr& callExprWrapper)
{
    if (irBuilder.GetCGContext().GetCompileOptions().enableCompileDebug) {
        return false;
    }
    return callExprWrapper.GetThisType(false)->IsGeneric();
}

// Compute and cache the outer type info for a hoistable virtual call.
llvm::Value* ComputeHoistedLookUpVirtualMethod(IRBuilder2& irBuilder, const CHIR::Type& thisCHIRType,
    const CHIR::ClassType& outerCHIRType, std::size_t virtualMethodOffset)
{
    auto& cgCtx = irBuilder.GetCGContext();
    auto i8PtrTy = irBuilder.getInt8PtrTy();

    auto backupBB = irBuilder.GetInsertBlock();
    auto backupIt = irBuilder.GetInsertPoint();
    auto vmCachePtr = irBuilder.CreateEntryAlloca(i8PtrTy, nullptr, "virtual_method_cache");

    irBuilder.SetInsertPointForPreparingTypeInfo();
    auto prepTiBB = irBuilder.GetInsertBlock();
    auto successorOfPrepTi = prepTiBB->getUniqueSuccessor();
    auto computeBB = prepTiBB->splitBasicBlock(&*prepTiBB->rbegin(), "compute.virtual.method");
    irBuilder.SetInsertPoint(computeBB);
    computeBB->getTerminator()->eraseFromParent();
    auto thisTI = irBuilder.CreateTypeInfo(thisCHIRType);
    auto isRef = irBuilder.CreateTypeInfoIsReferenceCall(thisTI);
    auto [nonRefBB] = Vec2Tuple<1>(irBuilder.CreateAndInsertBasicBlocks({GenNameForBB("thisTI.non.ref")}));
    irBuilder.LLVMIRBuilder2::CreateCondBr(isRef, successorOfPrepTi, nonRefBB);
    irBuilder.SetInsertPoint(nonRefBB);
    auto introTi1 = irBuilder.CreateTypeInfo(outerCHIRType);
    auto funcPtr1 = irBuilder.CallIntrinsicMTable({thisTI, introTi1, irBuilder.getInt64(virtualMethodOffset)});
    if (outerCHIRType.GetTypeArgs().size() <= 1) {
        auto objTyMeta = GenObjTyMetaForVirtualCall(irBuilder, outerCHIRType);
        llvm::cast<llvm::Instruction>(funcPtr1)->setMetadata("objType", objTyMeta);
    }
    irBuilder.CreateStore(funcPtr1, vmCachePtr);
    irBuilder.LLVMIRBuilder2::CreateBr(successorOfPrepTi);

    irBuilder.SetInsertPoint(backupBB, backupIt);
    auto vmCache = irBuilder.CreateLoad(i8PtrTy, vmCachePtr);
    auto isNull = irBuilder.CreateICmpEQ(vmCache, llvm::ConstantPointerNull::get(i8PtrTy));
    auto [vmCacheEqNull, vmCacheNotNull] = Vec2Tuple<2>(irBuilder.CreateAndInsertBasicBlocks(
        {GenNameForBB("vmCache.eq.null"), GenNameForBB("vmCache.not.null")}));
    irBuilder.CreateCondBr(isNull, vmCacheEqNull, vmCacheNotNull);

    irBuilder.SetInsertPoint(vmCacheEqNull);
    //
    auto introTi2 = irBuilder.CreateTypeInfo(outerCHIRType);
    llvm::Value* funcPtr2 = nullptr;
    if (outerCHIRType.GetClassDef()->IsInterface()) {
        funcPtr2 = irBuilder.CallIntrinsicMTable({thisTI, introTi2, irBuilder.getInt64(virtualMethodOffset)});
    } else {
        auto vtableSizeOfIntroType = cgCtx.GetVTableSizeOf(&outerCHIRType);
        CJC_ASSERT(vtableSizeOfIntroType > 0);
        auto idxOfIntroType = irBuilder.getInt64(vtableSizeOfIntroType - 1U);
        funcPtr2 = irBuilder.CallIntrinsicGetVTableFunc(
            thisTI, idxOfIntroType, irBuilder.getInt64(virtualMethodOffset), introTi2);
        auto meta = llvm::MDTuple::get(irBuilder.GetLLVMContext(),
            llvm::MDString::get(irBuilder.GetLLVMContext(), GetTypeQualifiedName(outerCHIRType)));
        llvm::cast<llvm::Instruction>(funcPtr2)->setMetadata("IntroType", meta);
    }
    if (outerCHIRType.GetTypeArgs().size() <= 1) {
        auto objTyMeta = GenObjTyMetaForVirtualCall(irBuilder, outerCHIRType);
        llvm::cast<llvm::Instruction>(funcPtr2)->setMetadata("objType", objTyMeta);
    }
    //
    irBuilder.CreateBr(vmCacheNotNull);

    irBuilder.SetInsertPoint(vmCacheNotNull);
    auto phi = irBuilder.CreatePHI(i8PtrTy, 2U); // 2U: num of branches
    phi->addIncoming(funcPtr2, vmCacheEqNull);
    phi->addIncoming(vmCache, backupBB);
    return phi;
}
} // namespace
llvm::Value* CodeGen::GenerateInvoke(IRBuilder2& irBuilder, const CHIRInvokeWrapper& invoke)
{
    irBuilder.SetCHIRExpr(&invoke);
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
    auto& cgMod = irBuilder.GetCGModule();
    auto objVal = cgMod | invoke.GetObject();
    std::vector<CGValue*> argsVal;
    for (auto arg : invoke.GetArgs()) {
        (void)argsVal.emplace_back(cgMod | arg);
    }

    auto funcType = invoke.GetMethodType();
    auto& cgCtx = cgMod.GetCGContext();

    auto objType = DeRef(*invoke.GetObject()->GetType());
    llvm::Value* funcPtr = nullptr;
    if (!objType->IsAutoEnv()) {
        auto ti = irBuilder.GetTypeInfoFromObject(objVal->GetRawValue());
        auto vtableOffset = invoke.GetVirtualMethodOffset();
        auto idxOfVFunc = irBuilder.getInt64(vtableOffset);
        auto introType = StaticCast<const CHIR::ClassType*>(invoke.GetOuterType(cgCtx.GetCHIRBuilder()));
        if (VirtualCallCanBeHoisted(irBuilder, invoke)) {
            funcPtr = ComputeHoistedLookUpVirtualMethod(irBuilder, *invoke.GetThisType(false), *introType, vtableOffset);
        } else {
        if (introType->GetClassDef()->IsInterface()) {
            auto introTi = irBuilder.CreateTypeInfo(introType);
            funcPtr = irBuilder.CallIntrinsicMTable({ti, introTi, idxOfVFunc});
        } else {
            auto vtableSizeOfIntroType = cgCtx.GetVTableSizeOf(introType);
            CJC_ASSERT(vtableSizeOfIntroType > 0);
            auto idxOfIntroType = irBuilder.getInt64(vtableSizeOfIntroType - 1U);
            auto introTi = irBuilder.CreateTypeInfo(introType);
            funcPtr = irBuilder.CallIntrinsicGetVTableFunc(ti, idxOfIntroType, idxOfVFunc, introTi);
            auto& llvmCtx = cgCtx.GetLLVMContext();
            auto meta = llvm::MDTuple::get(llvmCtx, llvm::MDString::get(llvmCtx, GetTypeQualifiedName(*introType)));
            llvm::cast<llvm::Instruction>(funcPtr)->setMetadata("IntroType", meta);
        }
        if (introType->GetTypeArgs().size() <= 1) {
            auto objTyMeta = GenObjTyMetaForVirtualCall(irBuilder, *introType);
            llvm::cast<llvm::Instruction>(funcPtr)->setMetadata("objType", objTyMeta);
        }
        }
    } else {
        auto i8PtrTy = irBuilder.getInt8PtrTy();
        auto funcName = invoke.GetMethodName();
        auto methodIdx = CHIR::GetMethodIdxInAutoEnvObject(funcName);
        auto payload = irBuilder.GetPayloadFromObject(**objVal);
        auto vritualPtr = irBuilder.CreateConstGEP1_32(i8PtrTy,
            irBuilder.LLVMIRBuilder2::CreateBitCast(payload, i8PtrTy->getPointerTo(1U)),
            static_cast<unsigned>(methodIdx), "virtualFPtr");
        auto loadInst = irBuilder.LLVMIRBuilder2::CreateLoad(i8PtrTy, vritualPtr);
        loadInst->setMetadata(llvm::LLVMContext::MD_invariant_load, llvm::MDNode::get(cgMod.GetLLVMContext(), {}));
        funcPtr = llvm::cast<llvm::Value>(loadInst);
    }
    auto concreteFuncType = static_cast<CGFunctionType*>(CGType::GetOrCreate(
        cgMod, funcType, CGType::TypeExtraInfo{0, true, false, true, invoke.GetInstantiatedTypeArgs()}));
    funcPtr = irBuilder.CreateBitCast(funcPtr, concreteFuncType->GetLLVMFunctionType()->getPointerTo());
    auto result = irBuilder.CreateCallOrInvoke(*concreteFuncType, funcPtr, argsVal);

    auto thisValue = **(cgMod | invoke.GetThisParam());
    auto originalNonRefVal = cgCtx.GetOriginalNonRefValOfBoxedValue(thisValue);
    if (originalNonRefVal && invoke.TestVritualMethodAttr(cgCtx.GetCHIRBuilder(), CHIR::Attribute::MUT)) {
        auto [handleBoxedBB, handleEndBB] =
            Vec2Tuple<2>(irBuilder.CreateAndInsertBasicBlocks({"handle_boxed", "handle_end"}));
        auto thisValueTI = irBuilder.GetTypeInfoFromObject(thisValue);
        irBuilder.CreateCondBr(irBuilder.CreateTypeInfoIsReferenceCall(thisValueTI), handleEndBB, handleBoxedBB);

        irBuilder.SetInsertPoint(handleBoxedBB);
        auto basePtr = cgCtx.GetBasePtrOf(originalNonRefVal);
        auto sizeOfTI = irBuilder.GetSizeFromTypeInfo(thisValueTI);
        std::vector<llvm::Value*> copyGenericParams{basePtr, originalNonRefVal, thisValue, sizeOfTI};
        irBuilder.CallIntrinsicGCWriteGeneric(copyGenericParams);
        irBuilder.CreateBr(handleEndBB);

        irBuilder.SetInsertPoint(handleEndBB);
    }
    return result;
#endif
}

llvm::Value* CodeGen::GenerateInvokeStatic(IRBuilder2& irBuilder, const CHIRInvokeStaticWrapper& invokeStatic)
{
    irBuilder.SetCHIRExpr(&invokeStatic);
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
    auto& cgMod = irBuilder.GetCGModule();
    std::vector<CGValue*> argsVal{};
    for (auto arg : invokeStatic.GetArgs()) {
        (void)argsVal.emplace_back(cgMod | arg);
    }

    llvm::Value* ti = **(cgMod | invokeStatic.GetRTTIValue());
    llvm::Value* funcPtr = nullptr;
    auto vtableOffset = invokeStatic.GetVirtualMethodOffset();
    auto idxOfVFunc = irBuilder.getInt64(vtableOffset);
    auto& cgCtx = cgMod.GetCGContext();
    auto introType = StaticCast<const CHIR::ClassType*>(invokeStatic.GetOuterType(cgCtx.GetCHIRBuilder()));
    if (introType->GetClassDef()->IsInterface()) {
        auto introTi = irBuilder.CreateTypeInfo(introType);
        funcPtr = irBuilder.CallIntrinsicMTable({ti, introTi, idxOfVFunc});
    } else {
        auto vtableSizeOfIntroType = cgCtx.GetVTableSizeOf(introType);
        CJC_ASSERT(vtableSizeOfIntroType > 0);
        auto idxOfIntroType = irBuilder.getInt64(vtableSizeOfIntroType - 1U);
        auto introTi = irBuilder.CreateTypeInfo(introType);
        funcPtr = irBuilder.CallIntrinsicGetVTableFunc(ti, idxOfIntroType, idxOfVFunc, introTi);
        auto& llvmCtx = cgCtx.GetLLVMContext();
        auto meta = llvm::MDTuple::get(llvmCtx, llvm::MDString::get(llvmCtx, GetTypeQualifiedName(*introType)));
        llvm::cast<llvm::Instruction>(funcPtr)->setMetadata("IntroType", meta);
    }
    if (introType->GetTypeArgs().size() <= 1) {
        auto objTyMeta = GenObjTyMetaForVirtualCall(irBuilder, *introType);
        llvm::cast<llvm::Instruction>(funcPtr)->setMetadata("objType", objTyMeta);
    }

    auto funcType = invokeStatic.GetMethodType();
    auto concreteFuncType = static_cast<CGFunctionType*>(CGType::GetOrCreate(
        cgMod, funcType, CGType::TypeExtraInfo{0, true, true, false, invokeStatic.GetInstantiatedTypeArgs()}));
    funcPtr = irBuilder.CreateBitCast(funcPtr, concreteFuncType->GetLLVMFunctionType()->getPointerTo());
    return irBuilder.CreateCallOrInvoke(*concreteFuncType, funcPtr, argsVal, false, ti);
#endif
}
