// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_IMPORTSPEC_H
#define CANGJIE_AST_NODES_IMPORTSPEC_H

#include "cangjie/AST/Nodes/Annotation.h"
#include "cangjie/AST/Nodes/Modifier.h"

namespace Cangjie::AST {
enum class ImportKind {
    IMPORT_SINGLE,
    IMPORT_ALIAS,
    IMPORT_ALL,
    IMPORT_MULTI,
};

/**
 * single import: a.b.c or a::b.c
 * multi import: a.b.{c, d} or a::b.{c, d} or a::{c, d}
 * alias import: a.b as ab or a::b as orgaB
 * import all: a.b.* or a::b.*. a::* is invalid.
 */
struct ImportContent : Node {
    ImportKind kind{ImportKind::IMPORT_SINGLE};
    bool hasDoubleColon{}; /**< Has '::', i.e. orgnization name */
    /// Prefer to use GetPrefixPath() and GetImportedPackageName().
    std::vector<std::string> prefixPaths; /**< Prefix paths in packageName. */
    std::vector<Position> prefixPoses;    /**< Prefix positions in packageName. */
    std::vector<Position> prefixDotPoses; /**< Dot positions in packageName. */
    // Used for single-import, alias-import, all-import
    Identifier identifier; /**< Imported identifier. */
    // Used for alias-import
    Position asPos;       /**< Position of 'as'. */
    Identifier aliasName; /**< Alias identifier. */
    // Used for multi-import
    Position leftCurlPos;             /**< Position of '{'. */
    std::vector<ImportContent> items; /**< Multi-imported items. */
    std::vector<Position> commaPoses; /**< Positions of each ','. */
    Position rightCurlPos;            /**< Position of '}'. */
    bool isDecl{false};               /**< Identifier is a declaration, set by ImportManager. */
    ImportContent() : Node(ASTKind::IMPORT_CONTENT)
    {
    }
    std::string ToString() const override;
    std::string GetPrefixPath() const;
    std::string GetImportedPackageName() const;
    std::string GetImportedPackageNameWithIsDecl() const;
    std::vector<std::string> GetPossiblePackageNames() const;
};

/**
 * An ImportSpec represents an import sentence in file header.
 */
struct ImportSpec : Node {
    OwnedPtr<Modifier> modifier;
    Position importPos; /**< Position of 'import'. */
    ImportContent content;
    std::vector<OwnedPtr<Annotation>> annotations; /**< Annotation set of the ImportSpec. */
    /**< Implicitly export all imported decls, initialized with true to  ensure compatibility. */
    bool withImplicitExport{true};
    ImportSpec() : Node(ASTKind::IMPORT_SPEC)
    {
    }

    bool IsReExport(bool noSubPkg = false) const
    {
        if (!modifier || modifier->modifier == TokenKind::PRIVATE) {
            return false;
        }
        return noSubPkg ? modifier->modifier != TokenKind::INTERNAL : true;
    }
    bool IsImportAll() const
    {
        return content.kind == ImportKind::IMPORT_ALL;
    }
    bool IsImportSingle() const
    {
        return content.kind == ImportKind::IMPORT_SINGLE;
    }
    bool IsImportAlias() const
    {
        return content.kind == ImportKind::IMPORT_ALIAS;
    }
    bool IsImportMulti() const
    {
        return content.kind == ImportKind::IMPORT_MULTI;
    }

    bool IsPrivateImport() const
    {
        return !modifier || modifier->modifier == TokenKind::PRIVATE;
    }
};
} // namespace Cangjie::AST

#endif // CANGJIE_AST_NODES_IMPORTSPEC_H
