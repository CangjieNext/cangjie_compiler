// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_TOKENPART_H
#define CANGJIE_AST_NODES_TOKENPART_H

#include "cangjie/AST/Nodes/Expr.h"

namespace Cangjie::AST {
/**
 * A TokenPart node represents Consecutive tokens separated by interpolation in quote expression.
 */
struct TokenPart : Expr {
    std::vector<Token> tokens;
    TokenPart() : Expr(ASTKind::TOKEN_PART)
    {
    }
    TokenPart(std::vector<Token>& tokens) : Expr(ASTKind::TOKEN_PART), tokens(tokens)
    {
        if (!tokens.empty()) {
            begin = tokens[0].Begin();
            end = tokens.back().End();
        }
    }
};

/**
 * A QuoteExpr node represents quote expression.
 */
struct QuoteExpr : Expr {
    Position quotePos;      /**< Position of 'quote'. */
    Position leftParenPos;  /**< Position of '('. */
    Position rightParenPos; /**< Position of ')'. */
    /**
     * Arguments of the quote expression, include DollarExpr and TokenPart.
     */
    std::vector<OwnedPtr<Expr>> exprs;
    QuoteExpr() : Expr(ASTKind::QUOTE_EXPR)
    {
    }
};
} // namespace Cangjie::AST

#endif // CANGJIE_AST_NODES_TOKENPART_H
