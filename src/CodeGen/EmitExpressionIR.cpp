// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "EmitExpressionIR.h"

#include "Base/CHIRExprWrapper.h"
#include "Base/ExprDispatcher/ExprDispatcher.h"
#include "CGModule.h"
#include "IRBuilder.h"
#include "IRGenerator.h"
#include "cangjie/CHIR/IR/Value/Value.h"
#include "cangjie/CHIR/Utils/ToStringUtils.h"

#include <cstdio>
#include <cstdlib>

using namespace Cangjie::CHIR;

namespace {
using namespace Cangjie;
using namespace CodeGen;

bool IsLLVM21TraceEnabled()
{
    static bool enabled = [] {
        const char* env = std::getenv("CJ_LLVM21_TRACE");
        return env != nullptr && env[0] != '\0' && env[0] != '0';
    }();
    return enabled;
}

void TraceLLVM21(const CGModule& cgMod, const CHIR::Expression& expr, size_t index, const char* stage)
{
    if (!IsLLVM21TraceEnabled()) {
        return;
    }
    std::string detail;
    if (expr.GetExprKind() == CHIR::ExprKind::INTRINSIC) {
        auto intrinsicKind = StaticCast<const CHIR::Intrinsic&>(expr).GetIntrinsicKind();
        auto it = CHIR::INTRINSIC_KIND_TO_STRING_MAP.find(intrinsicKind);
        detail = " detail=" + (it == CHIR::INTRINSIC_KIND_TO_STRING_MAP.end() ? std::to_string(intrinsicKind)
                                                                              : it->second);
    }
    const auto* topLevelFunc = expr.GetTopLevelFunc();
    const auto* parentBlock = expr.GetParentBlock();
    std::fprintf(stderr, "[llvm21-trace] Expr.%s pkg=%s module=%s func=%s bb=%s idx=%zu kind=%s%s\n", stage,
        cgMod.GetCGContext().GetCurrentPkgName().c_str(), cgMod.GetLLVMModule()->getSourceFileName().c_str(),
        topLevelFunc ? topLevelFunc->GetIdentifierWithoutPrefix().c_str() : "<null>",
        parentBlock ? parentBlock->GetIdentifierWithoutPrefix().c_str() : "<null>", index,
        CHIRExprWrapper(expr).GetExprKindName().c_str(), detail.c_str());
    std::fflush(stderr);
}
} // namespace

namespace Cangjie::CodeGen {

class ExpressionGeneratorImpl : public IRGeneratorImpl {
public:
    ExpressionGeneratorImpl(CGModule& cgMod, const std::vector<CHIR::Expression*>& chirExprs)
        : cgMod(cgMod), chirExprs(chirExprs)
    {
        // All chirExprs should be in the same BasicBlock.
        for (auto it : chirExprs) {
            CJC_ASSERT(it->GetParentBlock() == chirExprs[0]->GetParentBlock());
        }
    }

    void EmitIR() override;

private:
    CGModule& cgMod;
    const std::vector<Cangjie::CHIR::Expression*> chirExprs;
};

template <> class IRGenerator<ExpressionGeneratorImpl> : public IRGenerator<> {
public:
    IRGenerator(CGModule& cgMod, const std::vector<CHIR::Expression*>& chirExprs)
        : IRGenerator<>(std::make_unique<ExpressionGeneratorImpl>(cgMod, chirExprs))
    {
    }
};

void ExpressionGeneratorImpl::EmitIR()
{
    CJC_ASSERT(!chirExprs.empty());
    IRBuilder2 irBuilder(cgMod);
    // Since we restricted all the expressions are in the same basic block,
    // we are able to just use the first expression to determine the insert
    // point.
    if (auto parentBB = chirExprs.front()->GetParentBlock()) {
        irBuilder.SetInsertPoint(cgMod.GetMappedBB(parentBB));
    }
    irBuilder.CreateGenericParaDeclare(*cgMod.GetOrInsertCGFunction(chirExprs.front()->GetTopLevelFunc()));
    for (size_t index = 0; index < chirExprs.size(); ++index) {
        auto chirExpr = chirExprs[index];
        TraceLLVM21(cgMod, *chirExpr, index, "begin");
        CHIRExprWrapper chirExprWrapper = CHIRExprWrapper(*chirExpr);
        if (chirExpr->GetParentBlock()) {
            irBuilder.EmitLocation(chirExprWrapper);
        }
        irBuilder.SetCHIRExpr(&chirExprWrapper);
        if (cgMod.GetCGContext().GetCompileOptions().enableCompileDebug) {
            irBuilder.CreateLoadInstForParameter(*chirExpr);
        }

        llvm::Value* rawRet = nullptr;
        switch (chirExpr->GetExprMajorKind()) {
            case ExprMajorKind::TERMINATOR:
                rawRet = HandleTerminatorExpression(irBuilder, *chirExpr);
                break;
            case ExprMajorKind::UNARY_EXPR: {
                auto unaryExpr = StaticCast<const CHIR::UnaryExpression*>(chirExpr);
                irBuilder.EmitLocation(CHIRExprWrapper(*chirExpr));
                rawRet = HandleUnaryExpression(irBuilder, CHIRUnaryExprWrapper(*unaryExpr));
                break;
            }
            case ExprMajorKind::BINARY_EXPR: {
                auto binaryExpr = StaticCast<const CHIR::BinaryExpression*>(chirExpr);
                irBuilder.EmitLocation(CHIRExprWrapper(*chirExpr));
                rawRet = HandleBinaryExpression(irBuilder, CHIRBinaryExprWrapper(*binaryExpr));
                break;
            }
            case ExprMajorKind::MEMORY_EXPR:
                rawRet = HandleMemoryExpression(irBuilder, *chirExpr);
                break;
            case ExprMajorKind::OTHERS:
                if (chirExpr->IsConstant()) {
                    rawRet = HandleConstantExpression(irBuilder, *StaticCast<const CHIR::Constant*>(chirExpr));
                } else {
                    rawRet = HandleOthersExpression(irBuilder, *chirExpr);
                }
                break;
            default: {
                CJC_ASSERT(false && "Should not reach here.");
                break;
            }
        }
        if (auto rst = chirExpr->GetResult(); rst && rawRet) {
            CGType* cgType = CGType::GetOrCreate(cgMod, rst->GetType());
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
            auto& cgCtx = cgMod.GetCGContext();
            // A value that is neither a reference nor a RefType, and if it is passed by a pointer,
            // we need to yield a new CGType for it.
            if (!rst->GetType()->IsRef() && !rst->GetType()->IsGeneric() && rawRet->getType()->isPointerTy() &&
                !cgType->IsReference() && !cgType->IsOptionLikeRef()) {
                cgType = CGType::GetOrCreate(cgMod, CGType::GetRefTypeOf(cgCtx.GetCHIRBuilder(), *rst->GetType()),
                    CGType::TypeExtraInfo(rawRet->getType()->getPointerAddressSpace()));
            }
#endif
            CJC_NULLPTR_CHECK(chirExpr->GetTopLevelFunc());
            bool isSRetArg =
                irBuilder.GetInsertCGFunction()->IsSRet() && chirExpr->GetTopLevelFunc()->GetReturnValue() == rst;
            cgMod.SetOrUpdateMappedCGValue(rst, std::make_unique<CGValue>(rawRet, cgType, isSRetArg));
        }
        TraceLLVM21(cgMod, *chirExpr, index, "end");
    }
}

void EmitExpressionIR(CGModule& cgMod, std::vector<CHIR::Expression*> chirExprs)
{
    IRGenerator<ExpressionGeneratorImpl>(cgMod, chirExprs).EmitIR();
}
} // namespace Cangjie::CodeGen
