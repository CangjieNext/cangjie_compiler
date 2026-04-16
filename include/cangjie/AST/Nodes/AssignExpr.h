// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_ASSIGNEXPR_H
#define CANGJIE_AST_NODES_ASSIGNEXPR_H

#include "cangjie/AST/Nodes/OverloadableExpr.h"

namespace Cangjie::AST {
/**
 * An AssignExpr Node represents assignment expression.
 * example:
 *          a = b
 *          ^^^^^
 */
struct AssignExpr : OverloadableExpr {
    OwnedPtr<Expr> leftValue; /**< Left hand side expr. */
    Position assignPos{};     /**< Position of the assign operator. */
    OwnedPtr<Expr> rightExpr; /**< Right hand side expr. */
    bool isCompound = false;
    AssignExpr() : OverloadableExpr(ASTKind::ASSIGN_EXPR)
    {
    }
    void Clear() noexcept override;
};
} // namespace Cangjie::AST

#endif // CANGJIE_AST_NODES_ASSIGNEXPR_H
