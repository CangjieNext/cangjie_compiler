// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_BINARYEXPR_H
#define CANGJIE_AST_NODES_BINARYEXPR_H

#include "cangjie/AST/Nodes/OverloadableExpr.h"

namespace Cangjie::AST {
/**
 * BinaryExpr node represents binary operation expression.
 * e.g.
 *          a + b  or  a % b
 *          ^^^^^      ^^^^^
 */
struct BinaryExpr : OverloadableExpr {
    OwnedPtr<Expr> leftExpr;  /**< Left expr. */
    OwnedPtr<Expr> rightExpr; /**< Right expr. */
    Position operatorPos;     /**< Position of '&&', '||', etc. */
    BinaryExpr() : OverloadableExpr(ASTKind::BINARY_EXPR)
    {
    }
    void Clear() noexcept override;
};
} // namespace Cangjie::AST

#endif // CANGJIE_AST_NODES_BINARYEXPR_H
