// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_WILDCARDEXPR_H
#define CANGJIE_AST_NODES_WILDCARDEXPR_H

#include "cangjie/AST/Nodes/Expr.h"

namespace Cangjie::AST {
/**
 * Represent wildcard left value expr.
 * e.g. _ in `_ = 1`
 */
struct WildcardExpr : Expr {
    WildcardExpr() : Expr(ASTKind::WILDCARD_EXPR)
    {
    }
    explicit WildcardExpr(const Position& begin) : Expr(ASTKind::WILDCARD_EXPR)
    {
        this->begin = begin;
        this->end = begin + static_cast<size_t>(Len(TokenKind::WILDCARD));
    }
};
} // namespace Cangjie::AST

#endif // CANGJIE_AST_NODES_WILDCARDEXPR_H
