// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_UNARYEXPR_H
#define CANGJIE_AST_NODES_UNARYEXPR_H

#include "cangjie/AST/Nodes/OverloadableExpr.h"

namespace Cangjie::AST {
/**
 * UnaryExpr node represents unary operation expression.
 * e.g.
 *          !a  or  -a
 *          ^^      ^^
 */
struct UnaryExpr : OverloadableExpr {
    OwnedPtr<Expr> expr;  /**< The unary expr. */
    Position operatorPos; /**< Position of '- !'. */
    UnaryExpr() : OverloadableExpr(ASTKind::UNARY_EXPR)
    {
    }
    void Clear() noexcept override;
};
} // namespace Cangjie::AST

#endif // CANGJIE_AST_NODES_UNARYEXPR_H
