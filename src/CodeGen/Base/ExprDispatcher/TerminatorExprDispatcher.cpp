// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "Base/ExprDispatcher/ExprDispatcher.h"

#include <cinttypes>

#include "Base/AllocateImpl.h"
#include "Base/ApplyImpl.h"
#include "Base/ArrayImpl.h"
#include "Base/CGTypes/CGType.h"
#include "Base/CHIRExprWrapper.h"
#include "Base/IntrinsicsDispatcher.h"
#include "Base/InvokeImpl.h"
#include "Base/SpawnExprImpl.h"
#include "Base/TypeCastImpl.h"
#include "CGModule.h"
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
#include "CJNative/CGCFFI.h"
#endif
#include "Utils/BlockScopeImpl.h"
#include "Utils/CGCommonDef.h"
#include "cangjie/CHIR/IR/Value/Value.h"

namespace Cangjie::CodeGen {
namespace {
void HandleExitDebugInfo(IRBuilder2& irBuilder, const CHIR::Exit& exitExpr)
{
    auto& cgMod = irBuilder.GetCGModule();
    auto& cgCtx = irBuilder.GetCGContext();
    if (cgCtx.debugValue) {
        CJC_ASSERT(cgCtx.GetCompileOptions().enableCompileDebug);
        auto thisValue = irBuilder.GetInsertCGFunction()->GetArgByIndexFromCHIR(0);
        const auto& curCHIRFunc = StaticCast<const CHIR::Function&>(irBuilder.GetInsertCGFunction()->GetOriginal());
        auto thisTy = curCHIRFunc.GetParam(0)->GetType();
        auto cgType = CGType::GetOrCreate(cgMod, thisTy);
        auto thisCGValue = CGValue(thisValue, cgType);
        auto payload = irBuilder.GetPayloadFromObject(cgCtx.debugValue);
        irBuilder.CreateStore(CGValue(payload, cgType), thisCGValue);
    }
    if (auto func = irBuilder.GetInsertFunction(); func && func->hasFnAttribute(HAS_WITH_TI_WRAPPER_ATTR)) {
        cgCtx.AddDebugLocOfRetExpr(func, exitExpr.GetDebugLocation());
    }
}

llvm::Value* CreateRetFromSlot(
    IRBuilder2& irBuilder, const CHIR::Function& parentFunc, llvm::Value* retAddr, llvm::Type* retType)
{
    auto& cgMod = irBuilder.GetCGModule();
    auto llvmRetType = irBuilder.getCurrentFunctionReturnType();
    if (llvmRetType->isVoidTy()) {
        return irBuilder.CreateRetVoid();
    }
    auto curLLVMFunc = irBuilder.GetInsertFunction();
    CJC_NULLPTR_CHECK(curLLVMFunc);
    if (curLLVMFunc->hasFnAttribute(CodeGen::CFUNC_ATTR)) {
        auto retVal = cgMod.GetCGCFFI().ProcessRetValue(*llvmRetType, *retAddr, irBuilder);
        return irBuilder.CreateRet(retVal);
    }
    if (parentFunc.Get<CHIR::OverrideSrcFuncType>() && !curLLVMFunc->hasStructRetAttr()) {
        retType = llvmRetType;
    }
    return irBuilder.CreateRet(irBuilder.CreateLoad(retType, retAddr));
}

bool IsVoidLikeReturn(const CHIR::Type& retTy)
{
    return retTy.IsUnit() || retTy.IsNothing() || retTy.IsVoid();
}

llvm::Value* EmitDirectRet(IRBuilder2& irBuilder, const CHIR::LocalVar& ret)
{
    auto& cgMod = irBuilder.GetCGModule();
    // RawArray returns are lowered as direct values instead of going through a return slot.
    CJC_ASSERT(ret.GetExpr()->GetExprKind() == CHIR::ExprKind::RAW_ARRAY_ALLOCATE);
    return irBuilder.CreateRet(**(cgMod | &ret));
}
} // namespace

llvm::Value* HandleExitExpression(IRBuilder2& irBuilder, const CHIR::Exit& exitExpr)
{
    auto& cgMod = irBuilder.GetCGModule();
    HandleExitDebugInfo(irBuilder, exitExpr);
    auto parentFunc = exitExpr.GetTopLevelFunc();
    CJC_NULLPTR_CHECK(parentFunc);
    auto retTy = parentFunc->GetReturnType();
    if (IsVoidLikeReturn(*retTy)) {
        return irBuilder.CreateRetVoid();
    }
    auto ret = parentFunc->GetReturnValue();
    CJC_ASSERT_WITH_MSG(ret, "An unexpected nullptr is passed by CHIR.");
    if (retTy->IsRawArray()) {
        return EmitDirectRet(irBuilder, *ret);
    }
    CJC_ASSERT(ret->GetExpr()->GetExprKind() == CHIR::ExprKind::ALLOCATE);
    auto retAddr = **(cgMod | ret);
    auto retType = CGType::GetOrCreate(cgMod, ret->GetType()->GetTypeArgs()[0])->GetLLVMType();
    return CreateRetFromSlot(irBuilder, *parentFunc, retAddr, retType);
}

llvm::Value* HandleTerminatorExpression(IRBuilder2& irBuilder, const CHIR::Expression& chirExpr)
{
    CJC_ASSERT(chirExpr.IsTerminator());
    auto& cgMod = irBuilder.GetCGModule();
    switch (chirExpr.GetExprKind()) {
        case CHIR::ExprKind::GOTO: {
            auto& goTo = StaticCast<const CHIR::GoTo&>(chirExpr);
            auto succ = goTo.GetSuccessors()[0];
            return irBuilder.CreateBr(cgMod.GetMappedBB(succ));
        }
        case CHIR::ExprKind::EXIT: {
            auto& exitExpr = StaticCast<const CHIR::Exit&>(chirExpr);
            return HandleExitExpression(irBuilder, exitExpr);
        }
        case CHIR::ExprKind::BRANCH: {
            auto& branch = StaticCast<const CHIR::Branch&>(chirExpr);
            auto cond = **(cgMod | branch.GetCondition());
            auto trueBranch = cgMod.GetMappedBB(branch.GetTrueBlock());
            auto falseBranch = cgMod.GetMappedBB(branch.GetFalseBlock());
            return irBuilder.CreateCondBr(cond, trueBranch, falseBranch);
        }
        case CHIR::ExprKind::MULTIBRANCH: {
            auto& multiBranch = StaticCast<const CHIR::MultiBranch&>(chirExpr);
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
            auto cond = multiBranch.GetCondition();
            CJC_ASSERT(cond->GetType()->IsInteger());
            auto condVal = **(cgMod | cond);
            auto defaultBranch = cgMod.GetMappedBB(multiBranch.GetDefaultBlock());
            auto switchInst = irBuilder.CreateSwitch(condVal, defaultBranch);
            size_t bitness = llvm::cast<llvm::IntegerType>(condVal->getType())->getBitWidth();
            size_t caseIdx = 1; // default block is the first successor, others start from the second.
            for (auto caseVal : multiBranch.GetCaseVals()) {
                auto labelBranch = cgMod.GetMappedBB(multiBranch.GetSuccessor(caseIdx++));
                switchInst->addCase(irBuilder.getIntN(bitness, caseVal), labelBranch);
            }
            return switchInst;
#endif
        }
        case CHIR::ExprKind::RAISE_EXCEPTION: {
            auto& raiseException = StaticCast<const CHIR::RaiseException&>(chirExpr);
            auto exceptionValue = raiseException.GetExceptionValue();
            auto exceptionType = exceptionValue->GetType();
            CJC_ASSERT_WITH_MSG((exceptionType->IsRef() || exceptionType->IsGeneric()),
                "The target of the throw must be a generic or a reference to an object.");
            auto exceptionBlock = raiseException.GetExceptionBlock();
            if (exceptionBlock) {
                CodeGenUnwindBlockScope unwindBlockScope(cgMod, cgMod.GetMappedBB(exceptionBlock));
                irBuilder.CallExceptionIntrinsicThrow(cgMod | exceptionValue);
            } else {
                irBuilder.CallExceptionIntrinsicThrow(cgMod | exceptionValue);
            }
            (void)irBuilder.CreateUnreachable();
            return nullptr;
        }
        case CHIR::ExprKind::TRY_APPLY: {
            auto& tryApply = StaticCast<const CHIR::TryApply&>(chirExpr);
            auto normalDest = tryApply.GetSuccessor(0);
            auto unwindDest = tryApply.GetSuccessor(1);
            CodeGenUnwindBlockScope unwindBlockScope(cgMod, cgMod.GetMappedBB(unwindDest));
            auto resultVal = GenerateApply(irBuilder, CHIRApplyWrapper(tryApply));
            irBuilder.CreateBr(cgMod.GetMappedBB(normalDest));
            return resultVal;
        }
        case CHIR::ExprKind::TRY_INVOKE: {
            auto& tryInvoke = StaticCast<const CHIR::TryInvoke&>(chirExpr);
            auto normalDest = tryInvoke.GetSuccessor(0);
            auto unwindDest = tryInvoke.GetSuccessor(1);
            CodeGenUnwindBlockScope unwindBlockScope(cgMod, cgMod.GetMappedBB(unwindDest));
            auto resultVal = GenerateInvoke(irBuilder, CHIRInvokeWrapper(tryInvoke));
            irBuilder.CreateBr(cgMod.GetMappedBB(normalDest));
            return resultVal;
        }
        case CHIR::ExprKind::TRY_INVOKESTATIC: {
            auto& tryInvokeStatic = StaticCast<const CHIR::TryInvokeStatic&>(chirExpr);
            auto normalDest = tryInvokeStatic.GetSuccessor(0);
            auto unwindDest = tryInvokeStatic.GetSuccessor(1);
            CodeGenUnwindBlockScope unwindBlockScope(cgMod, cgMod.GetMappedBB(unwindDest));
            auto resultVal = GenerateInvokeStatic(irBuilder, CHIRInvokeStaticWrapper(tryInvokeStatic));
            irBuilder.CreateBr(cgMod.GetMappedBB(normalDest));
            return resultVal;
        }
        case CHIR::ExprKind::TRY_NEG: {
            auto& tryUnary = StaticCast<const CHIR::TryUnaryExpression&>(chirExpr);
            auto normalDest = tryUnary.GetSuccessor(0);
            auto unwindDest = tryUnary.GetSuccessor(1);
            CodeGenUnwindBlockScope unwindBlockScope(cgMod, cgMod.GetMappedBB(unwindDest));
            auto resultVal = HandleUnaryExpression(irBuilder, tryUnary);
            irBuilder.CreateBr(cgMod.GetMappedBB(normalDest));
            return resultVal;
        }
        case CHIR::ExprKind::TRY_ADD:
        case CHIR::ExprKind::TRY_SUB:
        case CHIR::ExprKind::TRY_MUL:
        case CHIR::ExprKind::TRY_DIV:
        case CHIR::ExprKind::TRY_MOD:
        case CHIR::ExprKind::TRY_EXP:
        case CHIR::ExprKind::TRY_LSHIFT:
        case CHIR::ExprKind::TRY_RSHIFT: {
            auto& tryBinary = StaticCast<const CHIR::TryBinaryExpression&>(chirExpr);
            auto normalDest = tryBinary.GetSuccessor(0);
            auto unwindDest = tryBinary.GetSuccessor(1);
            CodeGenUnwindBlockScope unwindBlockScope(cgMod, cgMod.GetMappedBB(unwindDest));
            auto resultVal = HandleBinaryExpression(irBuilder, tryBinary);
            irBuilder.CreateBr(cgMod.GetMappedBB(normalDest));
            return resultVal;
        }
        case CHIR::ExprKind::TRY_SPAWN: {
            auto& trySpawn = StaticCast<const CHIR::TrySpawn&>(chirExpr);
            auto normalDest = trySpawn.GetSuccessor(0);
            auto unwindDest = trySpawn.GetSuccessor(1);
            CodeGenUnwindBlockScope unwindBlockScope(cgMod, cgMod.GetMappedBB(unwindDest));
            auto resultVal = GenerateSpawn(irBuilder, trySpawn);
            irBuilder.CreateBr(cgMod.GetMappedBB(normalDest));
            return resultVal;
        }
        case CHIR::ExprKind::TRY_NUMERIC_CAST: {
            auto& tryNumericCast = StaticCast<const CHIR::TryNumericCast&>(chirExpr);
            auto normalDest = tryNumericCast.GetSuccessor(0);
            auto unwindDest = tryNumericCast.GetSuccessor(1);
            CodeGenUnwindBlockScope unwindBlockScope(cgMod, cgMod.GetMappedBB(unwindDest));
            auto resultVal = GenerateTypeCast(irBuilder, tryNumericCast);
            irBuilder.CreateBr(cgMod.GetMappedBB(normalDest));
            return resultVal;
        }
        case CHIR::ExprKind::TRY_INTRINSIC: {
            auto& tryIntrinsic = StaticCast<const CHIR::TryIntrinsic&>(chirExpr);
            auto normalDest = tryIntrinsic.GetSuccessor(0);
            auto unwindDest = tryIntrinsic.GetSuccessor(1);
            CodeGenUnwindBlockScope unwindBlockScope(cgMod, cgMod.GetMappedBB(unwindDest));
            auto resultVal = GenerateIntrinsic(irBuilder, tryIntrinsic);
            irBuilder.CreateBr(cgMod.GetMappedBB(normalDest));
            return resultVal;
        }
        case CHIR::ExprKind::TRY_ALLOCATE: {
            auto& tryAllocate = StaticCast<const CHIR::TryAllocate&>(chirExpr);
            auto normalDest = tryAllocate.GetSuccessor(0);
            auto unwindDest = tryAllocate.GetSuccessor(1);
            CodeGenUnwindBlockScope unwindBlockScope(cgMod, cgMod.GetMappedBB(unwindDest));
            auto resultVal = GenerateAllocate(irBuilder, tryAllocate);
            irBuilder.CreateBr(cgMod.GetMappedBB(normalDest));
            return resultVal;
        }
        case CHIR::ExprKind::TRY_RAW_ARRAY_ALLOCATE: {
            auto& tryRawArrayAllocate = StaticCast<const CHIR::TryRawArrayAllocate&>(chirExpr);
            auto normalDest = tryRawArrayAllocate.GetSuccessor(0);
            auto unwindDest = tryRawArrayAllocate.GetSuccessor(1);
            CodeGenUnwindBlockScope unwindBlockScope(cgMod, cgMod.GetMappedBB(unwindDest));
            auto resultVal = GenerateRawArrayAllocate(irBuilder, tryRawArrayAllocate);
            irBuilder.CreateBr(cgMod.GetMappedBB(normalDest));
            return resultVal;
        }
        default: {
            auto exprKindStr = std::to_string(static_cast<uint64_t>(chirExpr.GetExprKind()));
            CJC_ASSERT_WITH_MSG(false, std::string("Unexpected CHIRExprKind: " + exprKindStr + "\n").c_str());
            return nullptr;
        }
    }
}
} // namespace Cangjie::CodeGen
