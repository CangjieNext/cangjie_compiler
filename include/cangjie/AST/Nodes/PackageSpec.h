// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_PACKAGESPEC_H
#define CANGJIE_AST_NODES_PACKAGESPEC_H

#include "cangjie/AST/Identifier.h"
#include "cangjie/AST/Nodes/Modifier.h"

namespace Cangjie::AST {
/**
 * A PackageSpec node represents the package of the current file.
 */
struct PackageSpec : Node {
    OwnedPtr<Modifier> modifier;
    Position macroPos;     /**< Position of 'macro'. */
    Position packagePos;   /**< Position of 'package'. */
    bool hasDoubleColon{}; /**< Has '::', i.e. orgnization name */
    // For the following three vectors, a valid `PackageSpec` should have the same size.
    /// Prefer to use GetPackageName().
    std::vector<std::string> prefixPaths; /**< Prefix paths in packageName. */
    std::vector<Position> prefixPoses;    /**< Prefix positions in packageName. */
    std::vector<Position> prefixDotPoses; /**< Dot positions in packageName. */
    Identifier packageName;               /**< Package name. */
    bool hasMacro{false};
    bool hasSpecific{false};
    bool hasCommon{false};
    PackageSpec() : Node(ASTKind::PACKAGE_SPEC)
    {
    }
    std::string GetPackageName() const;
};
} // namespace Cangjie::AST

#endif // CANGJIE_AST_NODES_PACKAGESPEC_H
