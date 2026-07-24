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
    // A non-completing operand has already transferred control to its unreachable continuation. The enclosing throw
    // is therefore never evaluated, and RaiseException cannot take the synthetic Nothing value as its exception.
    if (eVal->GetType()->IsNothing()) {
        // The operand's candidate root has no source successor at this position. Close the enclosing throw silently
        // so a later sequential source expression, rather than this unevaluated throw, starts the next warning root.
        maybeUnreachable.erase(currentBlock);
        auto terminator = CreateAndAppendTerminator<Exit>(loc, currentBlock);
        CreateUnreachableContinuation(*terminator, false, false);
        return eVal;
    }
    Ptr<Terminator> terminator = nullptr;
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
