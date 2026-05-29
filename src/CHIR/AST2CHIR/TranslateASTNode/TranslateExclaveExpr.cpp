// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/AST2CHIR/TranslateASTNode/Translator.h"

using namespace Cangjie::CHIR;
using namespace Cangjie;

Ptr<Value> Translator::Visit(const AST::ExclaveExpr& exclaveExpr)
{
    // 1. Create exclave terminator and body.
    auto subGroup = builder.CreateBlockGroup(*currentBlock->GetTopLevelFunc());
    auto exclave = builder.CreateTerminator<Exclave>(currentBlock);
    exclave->InitBody(*subGroup);
    auto loc = TranslateLocation(exclaveExpr);
    exclave->SetDebugLocation(loc);

    // 2. Translate body.
    blockGroupStack.emplace_back(subGroup);
    const auto& body = exclaveExpr.body;
    CJC_NULLPTR_CHECK(body);
    Visit(*body);
    blockGroupStack.pop_back();
    return nullptr;
}