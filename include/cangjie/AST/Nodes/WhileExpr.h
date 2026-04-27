// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_WHILEEXPR_H
#define CANGJIE_AST_NODES_WHILEEXPR_H

#include "cangjie/AST/Nodes/Expr.h"
namespace Cangjie::AST {
struct Block;

/**
 * A WhileExpr node represents while expression.
 */
struct WhileExpr : Expr {
    Position whilePos;       /**< Position of 'while'. */
    Position leftParenPos;   /**< Positin of '('. */
    OwnedPtr<Expr> condExpr; /**< While condition. */
    Position rightParenPos;  /**< Position of ')'. */
    OwnedPtr<Block> body;    /**< Body of the WhileExpr. */
    SugarKind sugarKind = SugarKind::NO_SUGAR;
    WhileExpr() : Expr(ASTKind::WHILE_EXPR)
    {
    }
};

/**
 * A DoWhileExpr node represents do-while expression.
 */
struct DoWhileExpr : Expr {
    Position doPos;          /**< Position of 'do'. */
    OwnedPtr<Block> body;    /**< Body of the DoWhileExpr. */
    Position whilePos;       /**< Position of 'while'. */
    Position leftParenPos;   /**< Position of '('. */
    OwnedPtr<Expr> condExpr; /**< Do while condition. */
    Position rightParenPos;  /**< Position of ')'. */
    DoWhileExpr() : Expr(ASTKind::DO_WHILE_EXPR)
    {
    }
};
} // namespace Cangjie::AST

#endif // CANGJIE_AST_NODES_FORINEXPR_H
