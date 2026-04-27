// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_TYPECONVEXPR_H
#define CANGJIE_AST_NODES_TYPECONVEXPR_H

#include "cangjie/AST/Nodes/Expr.h"
#include "cangjie/AST/Nodes/Type.h"

namespace Cangjie::AST {
/**
 * A TypeConvExpr node represents numeric type convert expression.
 */
struct TypeConvExpr : Expr {
    OwnedPtr<Type> type;    /**< Must be numeric or function type. */
    Position leftParenPos;  /**< Position of '('. */
    OwnedPtr<Expr> expr;    /**< The expr to be convert. */
    Position rightParenPos; /**< Position of ')'. */
    TypeConvExpr() : Expr(ASTKind::TYPE_CONV_EXPR)
    {
    }
    std::string ToString() const override;
};
} // namespace Cangjie::AST

#endif // CANGJIE_AST_NODES_TYPECONVEXPR_H
