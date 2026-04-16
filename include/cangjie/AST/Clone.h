// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares AST clone apis. Moreover, a callback function is provided as
 * a parameter @p visitor which supports doing something during clone period.
 */

#ifndef CANGJIE_AST_CLONE_H
#define CANGJIE_AST_CLONE_H

#include <functional>
#include <memory>

#include "cangjie/AST/Nodes/Node.h"

namespace Cangjie::AST {
struct Generic;
using VisitFunc = std::function<void(Node&, Node&)>;
void DefaultVisitFunc(const Node& source, const Node& target);
void SetIsClonedSourceCode(const Node& source, Node& target);
void CopyBasicInfo(Ptr<const Node> source, Ptr<Node> target);
MacroInvocation CloneMacroInvocation(const MacroInvocation& me);
OwnedPtr<Generic> CloneGeneric(const Generic& generic, const VisitFunc& visitor = DefaultVisitFunc);
// only used in ASTCloner, because it has function template
OwnedPtr<Node> RunCloneAst(Ptr<Node> node, const VisitFunc& visitFunc);

class ASTCloner {
public:
    template <typename T>
    static std::vector<OwnedPtr<T>> CloneVector(const std::vector<OwnedPtr<T>>& nodes)
    {
        std::vector<OwnedPtr<T>> resNodes;
        for (auto& it : nodes) {
            (void)resNodes.emplace_back(Clone(it.get()));
        }
        return resNodes;
    }

    template <typename T> static OwnedPtr<T> Clone(Ptr<T> node, const VisitFunc& visitFunc = DefaultVisitFunc);
    struct Impl;
};

template <typename T> OwnedPtr<T> ASTCloner::Clone(Ptr<T> node, const VisitFunc& visitFunc)
{
    if (!node) {
        return OwnedPtr<T>();
    }
    return OwnedPtr<T>(static_cast<T*>(RunCloneAst(Ptr<Node>(node.get()), visitFunc).release()));
}
} // namespace Cangjie::AST

#endif // CANGJIE_AST_CLONE_H
