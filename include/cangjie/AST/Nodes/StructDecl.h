// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_STRUCTDECL_H
#define CANGJIE_AST_NODES_STRUCTDECL_H

#include "cangjie/AST/Nodes/InheritableDecl.h"

namespace Cangjie::AST {
/**
 * StructBody definition.
 */
struct StructBody : Node {
    Position leftCurlPos;              /**< Position of '{'. */
    std::vector<OwnedPtr<Decl>> decls; /**< Declarations in the body. */
    Position rightCurlPos;             /**< Position of '}'. */
    StructBody() : Node(ASTKind::STRUCT_BODY)
    {
    }
};

/**
 * A StructDecl node represents a struct declaration.
 */
struct StructDecl : InheritableDecl {
    OwnedPtr<StructBody> body; /**< StructBody. */
    bool HasConstOrFrozenInit()
    {
        if (!hasConstOrFrozenInit.has_value()) {
            const auto& decls = body->decls;
            bool has = std::find_if(decls.cbegin(), decls.cend(), [](const OwnedPtr<Decl>& decl) {
                return decl->TestAttr(Attribute::CONSTRUCTOR) &&
                    (decl->IsConst() || decl->HasAnno(AnnotationKind::FROZEN));
            }) != decls.cend();
            hasConstOrFrozenInit = {has};
        }
        return *hasConstOrFrozenInit;
    }

    StructDecl() : InheritableDecl(ASTKind::STRUCT_DECL)
    {
    }
    std::vector<OwnedPtr<Decl>>& GetMemberDecls() override
    {
        return body ? body->decls : Decl::GetMemberDecls();
    }

private:
    std::optional<bool> hasConstOrFrozenInit{std::nullopt};
};
} // namespace Cangjie::AST

#endif // CANGJIE_AST_NODES_STRUCTDECL_H
