// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/AST/Nodes/Pattern.h"

#include "cangjie/AST/Nodes/Expr.h"
#include "cangjie/AST/Nodes/VarDecl.h"

namespace Cangjie::AST {

VarPattern::VarPattern(SrcIdentifier ident, const Position& nodePos) : Pattern(ASTKind::VAR_PATTERN)
{
    varDecl = MakeOwned<VarDecl>();
    varDecl->begin = nodePos;
    varDecl->identifier = std::move(ident);
    varDecl->end = nodePos;
    varDecl->end = varDecl->identifier.End();
    varDecl->isVar = false;
    begin = nodePos;
    end = varDecl->end;
    varDecl->parentPattern = this;
}

} // namespace Cangjie::AST
