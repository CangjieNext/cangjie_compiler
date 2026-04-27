// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_OVERLOADABLEEXPR_H
#define CANGJIE_AST_NODES_OVERLOADABLEEXPR_H

#include "cangjie/AST/Nodes/Expr.h"

namespace Cangjie::AST {
/**
 * An OverloadableExpr represents an expr whose operator can be overloaded.
 * OverloadableExpr: AssignExpr, UnaryExpr, BinaryExpr, ArrayAccess.
 */
struct OverloadableExpr : Expr {
    TokenKind op{TokenKind::ILLEGAL}; /**< Operator kind. */
protected:
    OverloadableExpr(ASTKind kind) : Expr(kind)
    {
    }
};
} // namespace Cangjie::AST

#endif // CANGJIE_AST_NODES_OVERLOADABLEEXPR_H
