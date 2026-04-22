// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_EXCLAVEEXPR_H
#define CANGJIE_AST_NODES_EXCLAVEEXPR_H

#include "cangjie/AST/Nodes/Expr.h"
#include "cangjie/AST/Nodes/Block.h"

namespace Cangjie::AST {
struct ExclaveExpr : Expr {
    Position exclavePos;  /**< The position of 'exclave'. */
    OwnedPtr<Block> body; /**< The body of the exclave. */
    ExclaveExpr() : Expr(ASTKind::EXCLAVE_EXPR)
    {
    }
};
} // namespace Cangjie::AST

#endif // CANGJIE_AST_NODES_EXCLAVEEXPR_H
