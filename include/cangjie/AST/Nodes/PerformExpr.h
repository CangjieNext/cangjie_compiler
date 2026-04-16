// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_PERFORMEXPR_H
#define CANGJIE_AST_NODES_PERFORMEXPR_H

#include "cangjie/AST/Nodes/Expr.h"

namespace Cangjie::AST {
/**
 * A PerformExpr node represents perform expression.
 */
struct PerformExpr : Expr {
    Position performPos; /**< Position of 'perform'. */
    OwnedPtr<Expr> expr; /**< Perform expr. */
    PerformExpr() : Expr(ASTKind::PERFORM_EXPR)
    {
    }
};
} // namespace Cangjie::AST

#endif // CANGJIE_AST_NODES_PERFORMEXPR_H
