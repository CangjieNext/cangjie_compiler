// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "LICMOptimizer.h"

#include "CGModule.h"
#include "IRBuilder.h"
#include "Utils/CGCommonDef.h"

#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/LoopUtils.h"

namespace Cangjie::CodeGen {

void LICMOptimizer::Run()
{
    if (cgMod.GetCGContext().GetCompileOptions().enableCompileDebug || cgFunc.GetRawFunction()->isDeclaration()) {
        return;
    }
    MoveLoopInvariant4GetMethodOuterTI();
}

void LICMOptimizer::MoveLoopInvariant4GetMethodOuterTI()
{
    auto virtualCallInfo4LICMMap = cgMod.GetCGContext().GetVirtualCallInfo4LICM(&cgFunc);
    if (virtualCallInfo4LICMMap.empty()) {
        return;
    }

    IRBuilder2 irBuilder(cgMod);
    auto i8PtrTy = irBuilder.getInt8PtrTy();
    irBuilder.SetInsertCGFunction(cgFunc);
    for (auto [prepForVirtualCallBB, virtualCallInfo4LICM] : virtualCallInfo4LICMMap) {
        auto prepForVirtualCallEndBB = prepForVirtualCallBB->getUniqueSuccessor();
        CJC_NULLPTR_CHECK(prepForVirtualCallEndBB);
        if (!prepForVirtualCallEndBB) {
            continue;
        }
        auto lookupVirtualInst = llvm::cast<llvm::Instruction>(virtualCallInfo4LICM.lookupVirtualInst);
        if (auto loop = GetMostInnerLoopFor(*lookupVirtualInst); loop) {
            llvm::BasicBlock* preHdr = loop->getLoopPreheader();
            if (!preHdr) {
                continue;
            }
            llvm::BasicBlock* uniqueSuccessor = preHdr->getUniqueSuccessor();
            if (!uniqueSuccessor) {
                continue;
            }

            auto thisCHIRType = virtualCallInfo4LICM.thisType;
            auto outerCHIRType = virtualCallInfo4LICM.outerType;
            auto methodIndex = virtualCallInfo4LICM.virtualMethodOffset;
            auto getOuterTypeInst = virtualCallInfo4LICM.getOuterTypeInst;

            // create and store cache
            irBuilder.SetInsertPoint(&cgFunc.GetRawFunction()->getEntryBlock());
            auto vmCachePtr = irBuilder.CreateEntryAlloca(i8PtrTy, nullptr, "virtual_method_cache");
            auto outerTICachePtr =
                getOuterTypeInst ? irBuilder.CreateEntryAlloca(i8PtrTy, nullptr, "outer_Type_cache") : nullptr;
            irBuilder.SetInsertPoint(preHdr);
            preHdr->getTerminator()->eraseFromParent();
            auto thisTI = irBuilder.CreateTypeInfo(thisCHIRType);
            auto isRef = irBuilder.CreateTypeInfoIsReferenceCall(thisTI);
            auto [nonRefBB] = Vec2Tuple<1>(irBuilder.CreateAndInsertBasicBlocks({GenNameForBB("thisTI.non.ref")}));
            irBuilder.LLVMIRBuilder2::CreateCondBr(isRef, uniqueSuccessor, nonRefBB);
            irBuilder.SetInsertPoint(nonRefBB);
            auto introType1 = irBuilder.CreateTypeInfo(outerCHIRType);
            auto vmCache = irBuilder.CallIntrinsicMTable({thisTI, introType1, irBuilder.getInt64(methodIndex)});
            irBuilder.CreateStore(vmCache, vmCachePtr);
            if (outerTICachePtr) {
                auto outerTypeCache =
                    irBuilder.CallIntrinsicMethodOuterType({thisTI, introType1, irBuilder.getInt64(methodIndex)});
                irBuilder.CreateStore(outerTypeCache, outerTICachePtr);
            }
            irBuilder.LLVMIRBuilder2::CreateBr(uniqueSuccessor);

            // check cache
            auto oldBB =
                prepForVirtualCallBB->splitBasicBlock(&*prepForVirtualCallBB->begin(), "prepForVirtualCall.bb.old");
            irBuilder.SetInsertPoint(prepForVirtualCallBB);
            prepForVirtualCallBB->getTerminator()->eraseFromParent();
            auto vmCacheLoaded = irBuilder.CreateLoad(i8PtrTy, vmCachePtr);
            auto isNull = irBuilder.CreateICmpEQ(vmCacheLoaded, llvm::ConstantPointerNull::get(i8PtrTy));
            auto [nonNullBB] = Vec2Tuple<1>(irBuilder.CreateAndInsertBasicBlocks({GenNameForBB("cache.non.null")}));
            irBuilder.LLVMIRBuilder2::CreateCondBr(isNull, oldBB, nonNullBB);
            irBuilder.SetInsertPoint(nonNullBB);
            auto outerTICacheLoaded = outerTICachePtr ? irBuilder.CreateLoad(i8PtrTy, outerTICachePtr) : nullptr;
            irBuilder.LLVMIRBuilder2::CreateBr(prepForVirtualCallEndBB);

            // replace uses
            irBuilder.SetInsertPoint(prepForVirtualCallEndBB, prepForVirtualCallEndBB->begin());
            auto vmPHI = irBuilder.CreatePHI(i8PtrTy, 2U);
            lookupVirtualInst->replaceAllUsesWith(vmPHI);
            vmPHI->addIncoming(lookupVirtualInst, oldBB);
            vmPHI->addIncoming(vmCacheLoaded, nonNullBB);
            if (outerTICachePtr) {
                auto outerTIPHI = irBuilder.CreatePHI(i8PtrTy, 2U);
                getOuterTypeInst->replaceAllUsesWith(outerTIPHI);
                outerTIPHI->addIncoming(getOuterTypeInst, oldBB);
                outerTIPHI->addIncoming(outerTICacheLoaded, nonNullBB);
            }
        }
    }
}

llvm::Loop* LICMOptimizer::GetMostInnerLoopFor(const llvm::Instruction& inst)
{
    llvm::LoopInfoBase<llvm::BasicBlock, llvm::Loop>* loopInfo;
    if (dtAndLoopInfo) {
        loopInfo = dtAndLoopInfo->loopInfo.get();
    } else {
        auto dtUPtr = std::make_unique<llvm::DominatorTree>(*cgFunc.GetRawFunction());
        auto loopInfoUPtr = std::make_unique<llvm::LoopInfoBase<llvm::BasicBlock, llvm::Loop>>();
        loopInfoUPtr->analyze(*dtUPtr);
        loopInfo = loopInfoUPtr.get();
        dtAndLoopInfo = std::make_unique<DTAndLoopInfo>(std::move(dtUPtr), std::move(loopInfoUPtr));
    }
    return loopInfo->getLoopFor(inst.getParent());
}
} // namespace Cangjie::CodeGen
