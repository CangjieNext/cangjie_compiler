// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_PROPDECL_H
#define CANGJIE_AST_NODES_PROPDECL_H

#include "cangjie/AST/Nodes/FuncDecl.h"
#include "cangjie/AST/Nodes/VarDecl.h"

namespace Cangjie::AST {
/**
 * A PropDecl node represents a property declaration.
 */
struct PropDecl : VarDecl {
    Position leftCurlPos; /**< The postion of '{'. */
    std::vector<OwnedPtr<FuncDecl>> getters;
    std::vector<OwnedPtr<FuncDecl>> setters;
    Position rightCurlPos; /**< The postion of '}'. */
    ~PropDecl() override = default;
    PropDecl() : VarDecl(ASTKind::PROP_DECL)
    {
    }
    bool IsExportedDecl() const override;

    bool IsOpen() const noexcept override;
};
} // namespace Cangjie::AST

#endif // CANGJIE_AST_NODES_PROPDECL_H
