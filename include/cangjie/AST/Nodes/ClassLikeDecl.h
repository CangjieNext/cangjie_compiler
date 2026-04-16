// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_CLASSLIKEDECL_H
#define CANGJIE_AST_NODES_CLASSLIKEDECL_H

#include "cangjie/AST/Nodes/InheritableDecl.h"

namespace Cangjie::AST {
/**
 * Common parts of ClassDecl and InterfaceDecl.
 */
struct ClassLikeDecl : InheritableDecl {
    std::set<Ptr<Decl>> subDecls; /**< A <: B ==>  B.subDecls = {A}. */
protected:
    ClassLikeDecl(ASTKind kind) : InheritableDecl(kind)
    {
    }
};
} // namespace Cangjie::AST
#endif
