// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_INVALIDEXPR_H
#define CANGJIE_AST_NODES_INVALIDEXPR_H

#include "cangjie/AST/Nodes/Expr.h"

namespace Cangjie::AST {
/**
 * Placeholder for invalid expression.
 */
struct InvalidExpr : Expr {
    std::string value;
    InvalidExpr() : Expr(ASTKind::INVALID_EXPR)
    {
        EnableAttr(Attribute::IS_BROKEN);
    }
    explicit InvalidExpr(const Position& pos) : Expr(ASTKind::INVALID_EXPR)
    {
        EnableAttr(Attribute::IS_BROKEN);
        begin = pos;
        end = pos;
    }
};
} // namespace Cangjie::AST

#endif // CANGJIE_AST_NODES_INVALIDEXPR_H
