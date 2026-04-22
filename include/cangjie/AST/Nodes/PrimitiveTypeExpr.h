// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_PRIMITIVETYPEEXPR_H
#define CANGJIE_AST_NODES_PRIMITIVETYPEEXPR_H

#include "cangjie/AST/Nodes/Expr.h"
#include "cangjie/AST/Nodes/ModalASTInfo.h"

namespace Cangjie::AST {
/**
 * A PrimitiveTypeExpr is a placeholder Expr for primitive type static function call.
 * e.g. Int64.foo() is a MemberAccess, whose baseExpr is a PrimitiveTypeExpr, where its TypeKind =
 * TypeKind::TYPE_INT64.
 */
struct PrimitiveTypeExpr : Expr {
    TypeKind typeKind;
    ModalASTInfo modal;
    explicit PrimitiveTypeExpr(TypeKind typeKind) : Expr(ASTKind::PRIMITIVE_TYPE_EXPR), typeKind(typeKind)
    {
    }
    std::string ToString() const override;
};
} // namespace Cangjie::AST

#endif // CANGJIE_AST_NODES_PRIMITIVETYPEEXPR_H
