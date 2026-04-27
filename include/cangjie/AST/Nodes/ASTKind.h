// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_ASTNODECORE_H
#define CANGJIE_AST_NODES_ASTNODECORE_H

#include <algorithm>
#include <bitset>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include "cangjie/Utils/SafePointer.h"

namespace Cangjie {
enum class ScopeKind : uint8_t;
enum class ExprKind : uint8_t;
} // namespace Cangjie

namespace Cangjie::AST {
struct GenericsTy;
struct Ty;
using TyVar = GenericsTy;
using TyVarEnv = std::map<Ptr<TyVar>, std::set<Ptr<Ty>>>;

/**
 * ASTKind of the Node.
 */
enum class ASTKind : uint8_t {
#define ASTKIND(KIND, VALUE, NODE, SIZE) KIND,
#include "cangjie/AST/ASTKind.inc"
#undef ASTKIND
};

/**
 * Mapping of ASTKind to string.
 */
const static std::unordered_map<ASTKind, std::string> ASTKIND_TO_STR = {
#define ASTKIND(KIND, VALUE, NODE, SIZE) {ASTKind::KIND, VALUE},
#include "cangjie/AST/ASTKind.inc"
#undef ASTKIND
};

// Used to convert unique_ptr vector to pointer vector.
template <typename T> std::vector<Ptr<T>> ConverVector(const std::vector<OwnedPtr<T>>& source)
{
    std::vector<Ptr<T>> result;
    auto convertPtr = [&result](const OwnedPtr<T>& ptr) { result.push_back(ptr.get()); };
    std::for_each(source.begin(), source.end(), convertPtr);
    return result;
}
} // namespace Cangjie::AST

#endif // CANGJIE_AST_NODES_ASTNODECORE_H
