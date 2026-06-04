// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_LOCALMODALAST_H
#define CANGJIE_AST_NODES_LOCALMODALAST_H

#include "cangjie/AST/ModalInfo.h"
#include "cangjie/Basic/Position.h"
#include <string_view>

namespace Cangjie::AST {
using Cangjie::LocalModal;
using Cangjie::ModalInfo;

enum class LocalModalAST {
    NONE,
    NOT,
    HALF,
    FULL,
};
LocalModal ToLocalModal(LocalModalAST mod);
std::string_view ToString(LocalModalAST local);

struct ModalASTInfo {
    void SetLocal(LocalModalAST loc, const Position& localBegin)
    {
        this->local = loc;
        this->localPos = localBegin;
    }

    const Position& LocalBegin() const
    {
        return localPos;
    }
    Position LocalEnd() const
    {
        return localPos + ToString(local).size();
    }
    Position End() const;
    bool HasLocal() const;
    LocalModalAST Local() const
    {
        return local;
    }
    ModalInfo ToModalInfo() const;
    operator ModalInfo() const
    {
        return ToModalInfo();
    }
    /// Returns true if any modal is set. (currently only local modal)
    operator bool() const;
    /// Returns true if no modal is set.
    bool Empty() const;

private:
    Position localPos;
    LocalModalAST local{};
};
} // namespace Cangjie::AST
#endif // CANGJIE_AST_NODES_LOCALMODALAST_H