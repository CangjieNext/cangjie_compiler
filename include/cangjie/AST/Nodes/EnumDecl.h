// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_ENUMDECL_H
#define CANGJIE_AST_NODES_ENUMDECL_H

#include "cangjie/AST/Nodes/InheritableDecl.h"

namespace Cangjie::AST {
/**
 * Empty DummyBody scope for LSP split decl scopes.
 */
struct DummyBody : Node {
    DummyBody() : Node(ASTKind::DUMMY_BODY)
    {
    }
};

/**
 * An EnumDecl node represents an enum declaration.
 */
struct EnumDecl : InheritableDecl {
    /**
     * EnumDecl will be parsed according to the following rules:
     * 1. If there are NO associated values, then constructors in enumBody will be parsed to cases of VarLikeDecl
     * 2. If there are associated values, then constructors in enumBody will be parsed to cases of FuncDecl
     * 3. Members are functions defined in the enum
     */
    Position leftCurlPos;                     /**< The position of '{'. */
    OwnedPtr<DummyBody> bodyScope;            /**< split body scope for LSP. */
    std::vector<OwnedPtr<Decl>> constructors; /**< VarDecl or FuncDecl. */
    std::vector<Position> bitOrPosVector;     /**< Positions of '|'. */
    std::vector<OwnedPtr<Decl>> members;      /**< Functions in the enum. */
    Position rightCurlPos;                    /**< The position of '}'. */
    bool hasArguments{false};                 /**< Has arguments of the constructor, like Identifier1(TypeList1). */
    bool hasEllipsis{false};                  /**< Has ellipsis case, i.e. is non-exhaustive enum. */
    Position ellipsisPos;                     /**< Position of ellipsis, if one exists. */
    EnumDecl() : InheritableDecl(ASTKind::ENUM_DECL)
    {
    }
    std::vector<OwnedPtr<Decl>>& GetMemberDecls() override
    {
        return members;
    }
};
} // namespace Cangjie::AST

#endif // CANGJIE_AST_NODES_ENUMDECL_H
