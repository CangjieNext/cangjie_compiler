// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_CMPNODEBYPOS_H
#define CANGJIE_AST_NODES_CMPNODEBYPOS_H

#include "cangjie/AST/Nodes/Node.h"

namespace Cangjie::AST {
struct Decl;
bool CompNodeByPos(Ptr<const Node> n1, Ptr<const Node> n2);

struct CmpNodeByPos {
    bool operator()(Ptr<const Node> n1, Ptr<const Node> n2) const
    {
        return CompNodeByPos(n1, n2);
    }
};
using OrderedDeclSet = std::set<Ptr<Decl>, CmpNodeByPos>;
} // namespace Cangjie::AST

#endif // CANGJIE_AST_NODES_CMPNODEBYPOS_H
