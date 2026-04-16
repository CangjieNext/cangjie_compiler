// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_RESUMEEXPR_H
#define CANGJIE_AST_NODES_RESUMEEXPR_H

#include "cangjie/AST/Nodes/Expr.h"

namespace Cangjie::AST {
struct Handler;
/**
 * A ResumeExpr node represents resume expression.
 */
struct ResumeExpr : Expr {
    Position resumePos;                    /**< Position of 'resume'. */
    Position withPos;                      /**< Position of 'with' (optional). */
    OwnedPtr<Expr> withExpr;               /**< With expr (optional). */
    Position throwingPos;                  /**< Position of 'throwing' (optional). */
    OwnedPtr<Expr> throwingExpr;           /**< Throwing expr (optional). */
    std::optional<Ptr<Handler>> enclosing; /**< Pointer to the (immediate) enclosing handler, if any. */
    ResumeExpr() : Expr(ASTKind::RESUME_EXPR)
    {
    }
};
} // namespace Cangjie::AST

#include "cangjie/AST/Nodes/ReturnExpr.h"

#endif // CANGJIE_AST_NODES_RESUMEEXPR_H
