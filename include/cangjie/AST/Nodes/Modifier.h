// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_MODIFIER_H
#define CANGJIE_AST_NODES_MODIFIER_H

#include "cangjie/AST/Nodes/ASTKind.h"
#include "cangjie/AST/Nodes/Node.h"

namespace Cangjie::AST {
enum class StringKind { NORMAL, JSTRING, MULTILINE, MULTILINE_RAW };

/**
 * Expression has suffix '?' or not.
 */
enum class SuffixKind { NONE, QUEST };

/**
 * Access control modifiers
 */
struct Modifier : Node {
    TokenKind modifier; /**< Modifier token. */
    bool isExplicit = false;
    Modifier(TokenKind kind, const Position& pos) : Node(ASTKind::MODIFIER), modifier(kind)
    {
        begin = pos;
        end = {pos.fileID, pos.line, pos.column + Len(kind)};
        isExplicit = true;
    }
    bool operator<(const Modifier& rhs) const
    {
        return (modifier < rhs.modifier);
    }
    std::string ToString() const override;
};
} // namespace Cangjie::AST

#endif // CANGJIE_AST_NODES_MODIFIER_H
