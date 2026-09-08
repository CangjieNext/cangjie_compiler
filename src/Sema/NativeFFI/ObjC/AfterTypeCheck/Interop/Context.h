// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares core context for the core handlers of Cangjie <-> Objective-C interopability.
 */

#ifndef CANGJIE_SEMA_DESUGAR_OBJ_C_INTEROP_INTEROP_CONTEXT
#define CANGJIE_SEMA_DESUGAR_OBJ_C_INTEROP_INTEROP_CONTEXT

#include "InheritanceChecker/MemberSignature.h"
#include "NativeFFI/ObjC/AfterTypeCheck/Utils/AfterTypeCheckASTTransformer.h"
#include "NativeFFI/ObjC/Utils/ASTInserter.h"
#include "NativeFFI/ObjC/Utils/DeclarationCache.h"
#include "NativeFFI/ObjC/Utils/ASTFactory.h"
#include "NativeFFI/ObjC/Utils/InteropLibBridge.h"
#include "NativeFFI/ObjC/Utils/NameGenerator.h"
#include "NativeFFI/ObjC/Utils/TypeMapper.h"
#include "cangjie/AST/Node.h"
#include "cangjie/Basic/DiagnosticEngine.h"
#include "cangjie/Mangle/BaseMangler.h"
#include "cangjie/Modules/ImportManager.h"
#include "cangjie/Option/Option.h"
#include "cangjie/Sema/TypeManager.h"

namespace Cangjie::Interop::ObjC {

struct InteropContext {
    explicit InteropContext(AST::Package& pkg, TypeManager& typeManager, ImportManager& importManager,
        DiagnosticEngine& diag, const BaseMangler& mangler, const std::string& cjLibOutputPath,
        const std::string& outputObjCGenDir,
        const std::unordered_map<Ptr<const AST::InheritableDecl>, MemberMap>& structMemberSignatures,
        const Triple::OSType targetOsType, std::function<void(AST::Node&)> desugarPropRef)
        : pkg(pkg),
          desugarPropRef(std::move(desugarPropRef)),
          diag(diag),
          typeManager(typeManager),
          importManager(importManager),
          bridge(importManager, diag),
          typeMapper(bridge, typeManager),
          mangler(mangler),
          nameGenerator(mangler),
          declarationCache(),
          factory(bridge, typeManager, nameGenerator, typeMapper, importManager, declarationCache),
          astTransformer(importManager, typeManager, diag, astInserter),
          cjLibOutputPath(cjLibOutputPath),
          outputObjCGenDir(outputObjCGenDir),
          structMemberSignatures(structMemberSignatures),
          sharedLibraryExtension(GlobalOptions::GetSharedLibraryExtension(targetOsType))
    {
    }

    /**
     * The registry companion of @p impl, wherever @p impl was declared.
     *
     * `implToRegCompanion` holds the ones of this package; an imported impl carries its own alongside it in
     * the package it came from, and is reached through the import manager instead.
     *
     * @returns nullptr when there is none - an impl of this package marked broken gets no companion, and an
     *          imported one may be missing from the package it was loaded from.
     */
    Ptr<AST::ClassDecl> GetRegCompanion(AST::ClassDecl& impl) const noexcept
    {
        // Package names and not `Node::IsSamePackage`: a package node carries no file of its own, and that
        // predicate answers a comparison against one with `true` whichever package the other node is in.
        if (impl.fullPackageName != pkg.fullPackageName) {
            return importManager.GetImportedDecl<AST::ClassDecl>(
                impl.fullPackageName, nameGenerator.GenerateRegistryCompanionName(impl));
        }

        auto found = implToRegCompanion.find(Ptr(&impl));
        return found == implToRegCompanion.end() ? nullptr : found->second;
    }

    AST::Package& pkg;
    std::vector<Ptr<AST::ClassLikeDecl>> mirrors;
    std::vector<Ptr<AST::FuncDecl>> mirrorTopLevelFuncs;
    std::vector<Ptr<AST::ClassDecl>> impls;
    std::vector<Ptr<AST::ClassDecl>> mirrorInterfaceHandleWrappers;
    std::vector<Ptr<AST::ClassDecl>> regCompanions;
    /// W: MapImplToRegCompanion
    std::unordered_map<Ptr<AST::Decl>, Ptr<AST::ClassDecl>> implToRegCompanion;

    std::vector<OwnedPtr<AST::Decl>> genDecls;
    /**
     * Re-runs early property-accessor desugaring after field -> prop re-resolution.
     * R: MoveImplMembersToRegCompanion.
     */
    std::function<void(AST::Node&)> desugarPropRef;

    DiagnosticEngine& diag;
    TypeManager& typeManager;
    ImportManager& importManager;
    InteropLibBridge bridge;
    TypeMapper typeMapper;
    const BaseMangler& mangler;
    NameGenerator nameGenerator;
    DeclarationCache declarationCache;
    ASTFactory factory;
    AfterTypeCheckASTTransformer astTransformer;
    ASTInserter astInserter;
    const std::string& cjLibOutputPath;
    const std::string& outputObjCGenDir;
    const std::unordered_map<Ptr<const AST::InheritableDecl>, MemberMap>& structMemberSignatures;
    const std::string sharedLibraryExtension;
};

} // namespace Cangjie::Interop::ObjC

#endif // CANGJIE_SEMA_DESUGAR_OBJ_C_INTEROP_INTEROP_CONTEXT
