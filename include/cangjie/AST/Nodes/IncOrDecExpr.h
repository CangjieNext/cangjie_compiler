// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_INCORDECEXPR_H
#define CANGJIE_AST_NODES_INCORDECEXPR_H

#include "cangjie/AST/Nodes/OverloadableExpr.h"

namespace Cangjie::AST {
/**
 * An IncOrDecExpr node represents inc or dec expression.
 * e.g.
 *          a++  or  a--
 *          ^^^      ^^^
 */
struct IncOrDecExpr : OverloadableExpr {
    Position operatorPos; /**< Position of '++' or '--'. */
    OwnedPtr<Expr> expr;
    IncOrDecExpr() : OverloadableExpr(ASTKind::INC_OR_DEC_EXPR)
    {
    }
};
} // namespace Cangjie::AST

#endif // CANGJIE_AST_NODES_INCORDECEXPR_H
