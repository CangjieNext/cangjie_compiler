// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_INHERITABLEDECL_H
#define CANGJIE_AST_NODES_INHERITABLEDECL_H

#include "cangjie/AST/Nodes/Decl.h"

namespace Cangjie::AST {
struct Type;

struct InheritableDecl : public Decl {
    Position upperBoundPos;                     /**< Position of <:. */
    std::vector<OwnedPtr<Type>> inheritedTypes; /**< Super class or super interfaces. */
    std::set<Ptr<InterfaceTy>> GetSuperInterfaceTys() const;
    std::vector<Ptr<InterfaceTy>> GetStableSuperInterfaceTys() const;
    // guarantees sub-types always exist before super-types
    std::vector<Ptr<AST::ClassLikeDecl>> GetAllSuperDecls();

protected:
    InheritableDecl(ASTKind kind) : Decl(kind)
    {
    }
};
} // namespace Cangjie::AST

#endif // CANGJIE_AST_NODES_INHERITABLEDECL_H
