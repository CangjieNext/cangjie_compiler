// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/AST2CHIR/TranslateASTNode/Translator.h"
#include "cangjie/CHIR/IR/IntrinsicKind.h"
#include "cangjie/CHIR/Utils/Utils.h"

using namespace Cangjie::CHIR;
using namespace Cangjie;

namespace {
/// Match Allocate dest users: NullLiteral Store + EXCLUSIVE_SCOPE Intrinsic + Load.
/// On success sets intrinsicExpr and nullInitStore; otherwise returns false.
bool MatchExclusiveScopeDestUsers(Value& local, Load& load, Expression*& intrinsicExpr, Store*& nullInitStore)
{
    auto users = GetNonDebugUsers(local);
    if (users.size() != 3) {
        return false;
    }

    intrinsicExpr = nullptr;
    nullInitStore = nullptr;
    for (auto* user : users) {
        if (user == &load) {
            continue;
        }
        if (auto* in = DynamicCast<Intrinsic*>(user);
            in != nullptr && in->GetIntrinsicKind() == EXCLUSIVE_SCOPE && in->GetNumOfOperands() == 2 &&
            in->GetOperand(1) == &local) {
            if (intrinsicExpr != nullptr) {
                return false;
            }
            intrinsicExpr = in;
            continue;
        }
        // Store(NullLiteral, local) from TranslateIntrinsicCall sret prep.
        if (auto* store = DynamicCast<Store*>(user); store != nullptr && store->GetLocation() == &local) {
            auto* stored = DynamicCast<LocalVar*>(store->GetValue());
            if (stored == nullptr || stored->GetExpr() == nullptr || !stored->GetExpr()->IsConstantNull()) {
                return false;
            }
            if (nullInitStore != nullptr) {
                return false;
            }
            nullInitStore = store;
            continue;
        }
        return false;
    }
    return intrinsicExpr != nullptr && nullInitStore != nullptr;
}

/// If retVal comes from the EXCLUSIVE_SCOPE intrinsic, remaps Intrinsic T& dest to retLoc,
/// then removes Load, NullLiteral Store, and Allocate. Returns true on success,
/// meaning retVal is no longer relevant, must skip generating Store of retVal to retLoc.
bool TryFoldExclusiveScopeIntoReturn(Value& retVal, Value& retLoc)
{
    auto* lv = DynamicCast<LocalVar*>(&retVal);
    if (lv == nullptr) {
        return false;
    }
    auto* load = DynamicCast<Load*>(lv->GetExpr());
    if (load == nullptr) {
        return false;
    }
    auto* local = load->GetLocation();
    CJC_NULLPTR_CHECK(local);
    if (&retLoc == local) {
        return false;
    }
    auto* allocVar = DynamicCast<LocalVar*>(local);
    if (allocVar == nullptr) {
        return false;
    }
    auto* allocExpr = allocVar->GetExpr();
    if (allocExpr == nullptr || allocExpr->GetExprKind() != CHIR::ExprKind::ALLOCATE) {
        return false;
    }

    Expression* intrinsicExpr = nullptr;
    Store* nullInitStore = nullptr;
    if (!MatchExclusiveScopeDestUsers(*local, *load, intrinsicExpr, nullInitStore)) {
        return false;
    }

    intrinsicExpr->ReplaceOperand(local, &retLoc);
    if (load->GetResult()->GetDebugExpr()) {
        load->GetResult()->GetDebugExpr()->RemoveSelfFromBlock();
    }
    load->RemoveSelfFromBlock();
    nullInitStore->RemoveSelfFromBlock();
    if (auto* nullLv = DynamicCast<LocalVar*>(nullInitStore->GetValue());
        nullLv != nullptr && GetNonDebugUsers(*nullLv).empty()) {
        if (nullLv->GetDebugExpr()) {
            nullLv->GetDebugExpr()->RemoveSelfFromBlock();
        }
        nullLv->GetExpr()->RemoveSelfFromBlock();
    }
    allocExpr->RemoveSelfFromBlock();
    return true;
}
} // namespace

int64_t Translator::CalculateDelayExitLevelForReturn()
{
    // If there are other blockGroup exprs, please modify the condition here.
    // for example: WHILE, LOOP...
    int64_t level = 0;
    Ptr<BlockGroup> bg = blockGroupStack.back();
    for (auto rBegin = blockGroupStack.crbegin(), rEnd = blockGroupStack.crend(); rBegin != rEnd; ++rBegin) {
        bg = *rBegin;
        CJC_NULLPTR_CHECK(currentBlock->GetTopLevelFunc());
        auto parentLambda = DynamicCast<Lambda*>(bg->GetOwnerExpression());
        if (bg == currentBlock->GetTopLevelFunc()->GetBody() || parentLambda != nullptr) {
            break;
        }
        Expression* expr = bg->GetOwnerExpression();
        if (expr && Is<ForIn>(expr)) {
            // cond blockGroup need sub 1, and in the end of for-in node
            // need also sub 1, so here set to 2.
            level += 2U;
        }
    }
    return level;
}

int64_t Translator::CalculateDelayExitLevelForThrow()
{
    return CalculateDelayExitLevelForReturn() + 1;
}

Ptr<Value> Translator::GetOuterBlockGroupReturnValLocation()
{
    for (auto reverseBegin = blockGroupStack.crbegin(), reverseEnd = blockGroupStack.crend();
        reverseBegin != reverseEnd; ++reverseBegin) {
        Ptr<BlockGroup> bg = *reverseBegin;
        Expression* ownedExpr = bg->GetOwnerExpression();
        if (ownedExpr && Is<ForIn>(ownedExpr) && forInExprReturnMap.count(ownedExpr->GetResult()) != 0) {
            return forInExprReturnMap[ownedExpr->GetResult()];
        }
    }
    if (blockGroupStack.empty()) {
        return nullptr;
    }
    auto curBlockGroup = blockGroupStack.back();
    if (auto func = curBlockGroup->GetOwnerFunc()) {
        return func->GetReturnValue();
    } else if (auto lambda = DynamicCast<Lambda*>(curBlockGroup->GetOwnerExpression())) {
        return lambda->GetReturnValue();
    }
    return nullptr;
}

Ptr<Value> Translator::Visit(const AST::ReturnExpr& expr)
{
    const auto& loc = TranslateLocation(expr);
    CJC_NULLPTR_CHECK(expr.expr);
    auto retVal = TranslateExprArg(*expr.expr);
    int64_t level = CalculateDelayExitLevelForReturn();
    Ptr<Value> ret = GetOuterBlockGroupReturnValLocation();
    if (ret != nullptr) {
        if (!TryFoldExclusiveScopeIntoReturn(*retVal, *ret)) {
            if (expr.TestAttr(AST::Attribute::COMPILER_ADD)) {
                CreateAndAppendWrappedStore(*retVal, *ret);
            } else {
                CreateAndAppendWrappedStore(*retVal, *ret, loc);
            }
        }
    }
    if (level > 0 && delayExitSignal) {
        UpdateDelayExitSignal(level);
    }
    if (finallyContext.empty()) {
        auto terminator = CreateAndAppendTerminator<Exit>(loc, currentBlock);
        /* compile add return expr should not print warning.
            public open func foo(): Int64 {
                unsafe{        ---------------> will have a compiler add return expr.
                    return 3
                }
            }
        */
        if (expr.TestAttr(AST::Attribute::COMPILER_ADD)) {
            terminator->Set<SkipCheck>(SkipKind::SKIP_DCE_WARNING);
        }
    } else {
        // When current is in try-finally context, store the control blocks with control kind value.
        auto& [_, controlBlocks] = finallyContext.top();
        auto index = static_cast<uint8_t>(ControlType::RETURN);
        auto prevBlock = currentBlock;
        // Create return in separate block, and control flow will be redirected to this block at the end of finally.
        currentBlock = CreateBlock();
        CreateAndAppendTerminator<Exit>(loc, currentBlock);
        // the pair of blocks is {the block before control flow, control flow's target block}.
        controlBlocks[index].emplace_back(prevBlock, currentBlock);
    }
    currentBlock = CreateBlock();
    currentBlock->SetDebugLocation(loc);
    return nullptr;
}
