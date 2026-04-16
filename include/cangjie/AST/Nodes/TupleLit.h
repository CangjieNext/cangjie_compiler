// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_TUPLELIT_H
#define CANGJIE_AST_NODES_TUPLELIT_H

#include "cangjie/AST/Nodes/Expr.h"

namespace Cangjie::AST {
/**
 * A TupleLit node represents a tuple literal with expressions between comma.
 * ```
 * (1, 2, 3, 4)
 * ```
 */
struct TupleLit : Expr {
    Position leftParenPos;                /**< Position of '('. */
    std::vector<OwnedPtr<Expr>> children; /**< Children of tuple. */
    std::vector<Position> commaPosVector; /**< Positions of commas. */
    Position rightParenPos;               /**< Position of ')'. */
    TupleLit() : Expr(ASTKind::TUPLE_LIT)
    {
    }
};
} // namespace Cangjie::AST

#endif // CANGJIE_AST_NODES_TUPLELIT_H
