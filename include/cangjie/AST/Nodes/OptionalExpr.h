// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_OPTIONALEXPR_H
#define CANGJIE_AST_NODES_OPTIONALEXPR_H

#include "cangjie/AST/Nodes/Expr.h"

namespace Cangjie::AST {
struct OptionalExpr : Expr {
    OwnedPtr<Expr> baseExpr;
    Position questPos; /**< The position of '?'. */
    explicit OptionalExpr() : Expr(ASTKind::OPTIONAL_EXPR)
    {
    }
    std::string ToString() const override;
};

struct OptionalChainExpr : Expr {
    OwnedPtr<Expr> expr;
    explicit OptionalChainExpr() : Expr(ASTKind::OPTIONAL_CHAIN_EXPR)
    {
    }
    std::string ToString() const override;
};
} // namespace Cangjie::AST

#endif // CANGJIE_AST_NODES_OPTIONALEXPR_H
