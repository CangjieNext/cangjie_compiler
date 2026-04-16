// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_ASEXPR_H
#define CANGJIE_AST_NODES_ASEXPR_H

#include "cangjie/AST/Nodes/Expr.h"

namespace Cangjie::AST {
/**
 * AsExpr node represents `as` operation expression.
 */
struct AsExpr : Expr {
    OwnedPtr<Expr> leftExpr; /**< Left expr. */
    OwnedPtr<Type> asType;   /**< Right type. */
    Position asPos;          /**< Position of 'as'. */
    AsExpr() : Expr(ASTKind::AS_EXPR)
    {
    }
    std::string ToString() const override;
};
} // namespace Cangjie::AST

#endif // CANGJIE_AST_NODES_ASEXPR_H
