// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_TRAILINGCLOSUREEXPR_H
#define CANGJIE_AST_NODES_TRAILINGCLOSUREEXPR_H

#include "cangjie/AST/Nodes/LambdaExpr.h"

namespace Cangjie::AST {
struct TrailingClosureExpr : Expr {
    Position leftLambda;         /**< Left Position of Lambda. */
    OwnedPtr<Expr> expr;         /**< refExpr or callExpr. */
    OwnedPtr<LambdaExpr> lambda; /**< tail closure package. */
    Position rightLambda;        /**< Right Position of Lambda. */
    TrailingClosureExpr() : Expr(ASTKind::TRAIL_CLOSURE_EXPR)
    {
    }
};
} // namespace Cangjie::AST

#endif // CANGJIE_AST_NODES_TRAILINGCLOSUREEXPR_H
