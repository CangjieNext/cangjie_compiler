// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_EXTENDDECL_H
#define CANGJIE_AST_NODES_EXTENDDECL_H

#include "cangjie/AST/Nodes/InheritableDecl.h"

namespace Cangjie::AST {
/**
 * An ExtendDecl node represents a extend declaration.
 */
struct ExtendDecl : InheritableDecl {
    OwnedPtr<Type> extendedType;         /**< The type being extended. */
    Position wherePos;                   /**< Position of 'where'. */
    OwnedPtr<DummyBody> bodyScope;       /**< split body scope for LSP. */
    Position leftCurlPos;                /**< The position of '{'. */
    std::vector<OwnedPtr<Decl>> members; /**< Declarations of the extend decl. */
    Position rightCurlPos;               /**< The position of '}'. */
    ExtendDecl() : InheritableDecl(ASTKind::EXTEND_DECL)
    {
    }
    std::vector<OwnedPtr<Decl>>& GetMemberDecls() override
    {
        return members;
    }
    bool IsExportedDecl() const override;
};

/**
 * Placeholder for invalid declaration.
 */
struct InvalidDecl : Decl {
    explicit InvalidDecl(const Position& pos) : Decl(ASTKind::INVALID_DECL)
    {
        EnableAttr(Attribute::IS_BROKEN);
        begin = pos;
        end = pos;
        doNotExport = true;
    }
};
} // namespace Cangjie::AST

#endif // CANGJIE_AST_NODES_EXTENDDECL_H
