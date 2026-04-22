// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_BUILTINDECL_H
#define CANGJIE_AST_NODES_BUILTINDECL_H

#include "cangjie/AST/Nodes/Decl.h"
#include <cstdint>

namespace Cangjie::AST {

enum class BuiltInType : uint8_t {
    ARRAY,
    POINTER,
    CSTRING,
    CFUNC,
    VARRAY,
    COPY,
    // Will have other builtin types.
};

/**
 * Built-in decl is for built-in types, such as Array, CString.
 */
struct BuiltInDecl : public Decl {
    BuiltInType type;
    /** Synthetic members (e.g. `init`) used by Sema lookup and constructor resolution. */
    std::vector<OwnedPtr<Decl>> memberDecls;

    BuiltInDecl(BuiltInType type) : Decl(ASTKind::BUILTIN_DECL), type(type)
    {
    }
    bool IsType(BuiltInType ty) const
    {
        return ty == type;
    }
    std::vector<OwnedPtr<Decl>>& GetMemberDecls() override;
};
} // namespace Cangjie::AST

#endif
