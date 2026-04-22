// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_LAMBDAEXPR_H
#define CANGJIE_AST_NODES_LAMBDAEXPR_H

#include "cangjie/AST/Nodes/Expr.h"

namespace Cangjie::AST {
struct FuncBody;
/**
 * A lambdaExpr node represents a lambda expression with '=>'.
 */
struct LambdaExpr : Expr {
    LambdaExpr();
    ~LambdaExpr() override;
    OwnedPtr<FuncBody> funcBody; /**< Funcbody of lambda. */
    std::string mangledName;     /**< The name after mangling. */
    bool needsRegion{false};     /**< Whether the func needs region. Set by sema, used by CHIR. */
    explicit LambdaExpr(OwnedPtr<FuncBody> body);
};
} // namespace Cangjie::AST

#endif // CANGJIE_AST_NODES_LAMBDAEXPR_H
