// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/AST2CHIR/TranslateASTNode/Translator.h"

using namespace Cangjie::CHIR;
using namespace Cangjie;

Ptr<Value> Translator::Visit(const AST::ThrowExpr& throwExpr)
{
    auto loc = TranslateLocation(throwExpr.begin, throwExpr.end);
    CJC_NULLPTR_CHECK(throwExpr.expr);
    auto eVal = TranslateExprArg(*throwExpr.expr);
    if (eVal->GetType()->IsNothing()) {
        // The operand has already transferred control to its own continuation, so this outer throw is never
        // evaluated. Do not turn its synthetic Exit into another diagnostic root.
        currentBlock->DisableAttr(Attribute::UNREACHABLE_CONTINUATION);
        currentBlock->Remove<DebugLocationInfoForWarning>();
        CreateAndAppendTerminator<Exit>(loc, currentBlock);
        currentBlock = CreateBlock();
        return eVal;
    }
    Ptr<Expression> terminator = nullptr;
    if (tryCatchContext.empty()) {
        terminator = CreateAndAppendTerminator<RaiseException>(loc, eVal, currentBlock);
    } else {
        auto errBB = tryCatchContext.top();
        terminator = CreateAndAppendTerminator<RaiseException>(loc, eVal, errBB, currentBlock);
    }
    // For following unreachable expressions, and throw also has value of type 'Nothing'.
    CreateUnreachableContinuation(*terminator);
    return nullptr;
}
