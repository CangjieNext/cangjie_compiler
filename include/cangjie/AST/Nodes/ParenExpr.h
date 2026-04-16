// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_PARENEXPR_H
#define CANGJIE_AST_NODES_PARENEXPR_H

#include "cangjie/AST/Nodes/Expr.h"

namespace Cangjie::AST {
/**
 * A ParenExpr node represents a parenthesized expression which the expression between "(" and ")".
 * ```
 * a = (1+2)*3;
 * ```
 */
struct ParenExpr : Expr {
    Position leftParenPos;  /**< Position of '('. */
    OwnedPtr<Expr> expr;    /**< Expr between '(' and ')'. */
    Position rightParenPos; /**< Position of ')'. */
    ParenExpr() : Expr(ASTKind::PAREN_EXPR)
    {
    }
    void Clear() noexcept override;
    std::string ToString() const override;
};
} // namespace Cangjie::AST

#endif // CANGJIE_AST_NODES_PARENEXPR_H
