// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_CANDIDATE_H
#define CANGJIE_AST_CANDIDATE_H

#include <vector>
#include <unordered_set>
#include "cangjie/Utils/SafePointer.h"

namespace Cangjie {
namespace AST {
struct Decl;
struct Ty;
}
/**
 * This structure is used for lsp dot completion.
 * Since std::variant cannot perform correctly in windows, we chose to use this custom type.
 */
struct Candidate {
    std::vector<Ptr<AST::Decl>> decls;
    std::unordered_set<Ptr<AST::Ty>> tys;
    bool hasDecl;
    explicit Candidate(const std::vector<Ptr<AST::Decl>>& decls) : decls(decls), hasDecl(true)
    {
    }
    explicit Candidate(const std::unordered_set<Ptr<AST::Ty>>& tys) : tys(tys), hasDecl(false)
    {
    }
    Candidate() = default;
};
}
#endif