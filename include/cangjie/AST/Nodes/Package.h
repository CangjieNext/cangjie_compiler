// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_PACKAGE_H
#define CANGJIE_AST_NODES_PACKAGE_H

#include "cangjie/AST/Nodes/Decl.h"
#include "cangjie/AST/Nodes/File.h"

namespace Cangjie::AST {
enum class AccessLevel {
    PRIVATE,
    INTERNAL,
    PROTECTED,
    PUBLIC,
};
struct FuncDecl;

/**
 * A Package Node represents a package contains multi files and imports.
 */
struct Package : Node {
    std::string fullPackageName{"default"};               /**< Package name, default name is 'default'. */
    std::vector<OwnedPtr<File>> files;                    /**< All src files in package. */
    std::vector<OwnedPtr<Decl>> genericInstantiatedDecls; /**< The instantiations of the generic decls. */
    /** Imported non-generic functions or global const variables with source code. */
    std::vector<Ptr<Decl>> srcImportedNonGenericDecls;
    // Following three members are used after sema.
    std::vector<Ptr<FuncDecl>> inlineFuncDecls; /**< Imported inline functions. */
    AccessLevel accessible{AccessLevel::PUBLIC};
    bool hasSourceImportedDecl{false};
    bool isMacroPackage{false};
    bool noSubPkg{false};
    bool needExported{true}; /**< Parent path of package path is "src", there is no need to export this package. */

    // ===--------------------------------------------------------------------===//
    // Interop CJ Package Level Symbol Config
    // ===--------------------------------------------------------------------===//
    InteropCJStrategy interopCJApiStrategy = InteropCJStrategy::NONE;
    InteropCJGenericStrategyType interopCJGenericTypeStrategy = InteropCJGenericStrategyType::NONE;
    std::vector<std::string> interopCJIncludedApis;
    std::vector<std::string> interopCJExcludedApis;
    std::unordered_map<std::string, std::unordered_map<std::string, GenericTypeArguments>>
        allowedInteropCJGenericInstantiations;
    std::vector<std::string> interopTuples; /**< Record interop tuple configuration. Writed in sema. */
    bool isInteropCJPackageConfig{false};
    std::vector<LambdaPattern> lambdaPatterns;

private:
    std::vector<std::string> allDependentStdPkgs; /**< Record all dependent standard packages. */

public:
    Package() : Node(ASTKind::PACKAGE)
    {
    }
    Package(const std::string& pkgName) : Node(ASTKind::PACKAGE), fullPackageName(pkgName)
    {
    }

    bool IsEmpty() const;
    bool HasFtrDirective() const
    {
        for (auto& file : files) {
            if (file->feature) {
                return true;
            }
        }
        return false;
    }

    void AddDependentStdPkg(const std::string& pkgName)
    {
        allDependentStdPkgs.emplace_back(pkgName);
    }
    void ClearAllDependentStdPkgs()
    {
        allDependentStdPkgs.clear();
    }
    const std::vector<std::string>& GetAllDependentStdPkgs() const
    {
        return allDependentStdPkgs;
    }

    const std::vector<LambdaPattern>& GetLambdaPatterns() const
    {
        return lambdaPatterns;
    }
};

/**
 * A PackageDecl node represents imported package.
 */
struct PackageDecl : Decl {
    Ptr<Package> const srcPackage; /**< Imported source package node. */
    PackageDecl(Package& pkg) : Decl(ASTKind::PACKAGE_DECL), srcPackage(&pkg)
    {
        identifier = pkg.fullPackageName;
        fullPackageName = pkg.fullPackageName;
        if (pkg.TestAttr(Attribute::IMPORTED)) {
            EnableAttr(Attribute::IMPORTED);
        }
    }
};
} // namespace Cangjie::AST

#endif // CANGJIE_AST_NODES_PACKAGE_H
