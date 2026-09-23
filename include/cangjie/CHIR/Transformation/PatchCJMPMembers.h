// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_CHIR_TRANSFORMATION_PATCH_CJMP_MEMBERS_H
#define CANGJIE_CHIR_TRANSFORMATION_PATCH_CJMP_MEMBERS_H

#include <vector>

#include "cangjie/AST/Node.h"
#include "cangjie/CHIR/IR/CHIRBuilder.h"
#include "cangjie/CHIR/IR/Package.h"
#include "cangjie/CHIR/IR/Type/CHIRType.h"
#include "cangjie/Option/Option.h"

namespace Cangjie::CHIR {

class ClassType;

/**
 * @brief Facade that coordinates all CJMP specific-part member patching on the
 *        translated CHIR package.
 *
 * Covers four concerns, each scoped to @JavaMirror/@ObjCMirror or @JavaImpl
 * classes during CJMP specific-part compilation:
 *  - superclass repointing (SPECIFIC class -> platform mirror parent),
 *  - instance-field type collapsing (field -> Unit where a getter exists),
 *  - field-reference rewriting (direct load/store -> getter/setter calls),
 *  - constructor field-initialization insertion for @JavaImpl common-part ctors.
 *
 * The constructor evaluates the IsCompilingCJMPSpecific() guard once and caches
 * it, so every method is a no-op outside the CJMP specific part without
 * re-checking the option. Per-pass work is delegated to the focused
 * transformation classes CHIRFieldPatcher, CHIRFieldReferencePatcher, and
 * PatchCJMPImplBodies; PatchSuperClass is implemented directly here.
 *
 * Methods are intentionally split across three AST2CHIR translation phases and
 * cannot be merged into a single Run(): non-CJMP steps (SetExtendInfo,
 * UpdateExtendParent, ProcessClassStructVarInits) are interleaved between them
 * by AST2CHIR::TranslateNominalDecls.
 */
class PatchCJMPMembers {
public:
    /**
     * @brief Construct the patcher bound to @p pkg, @p builder, and @p chirType.
     * @param opts global options; caches IsCompilingCJMPSpecific() once.
     * @param pkg  the CHIR package whose class defs will be patched.
     * @param builder CHIR builder for creating types/expressions.
     * @param chirType CHIR type manager used for AST->CHIR type translation.
     */
    PatchCJMPMembers(const GlobalOptions& opts, Package& pkg, CHIRBuilder& builder, CHIRType& chirType);

    /** @name Phase 1 — before nominal-decl translation */
    ///@{
    /**
     * @brief Repoints the CHIR superclass of CJMP @SPECIFIC classes whose
     *        implicit super is the Cangjie Object root to the real platform
     *        mirror superclass (@JavaMirror/@ObjCMirror).
     *
     * Builds a name->AST map from @p nominalDecls and @p importedNominalDecls
     * to look up the AST decl matching each SPECIFIC CHIR ClassDef; its super
     * type is translated to CHIR and, when that super is a platform mirror,
     * applied via SetSuperClassTy. Classes that already inherit a real
     * non-Object superclass are skipped (HasObjectAsSuperClass). No-op unless
     * compiling the CJMP specific part.
     *
     * @param nominalDecls         current-package nominal decls.
     * @param importedNominalDecls imported nominal decls.
     */
    void PatchSuperClass(const std::vector<Ptr<const AST::Decl>>& nominalDecls,
        const std::vector<Ptr<const AST::Decl>>& importedNominalDecls);
    ///@}

    /** @name Phase 2a — collapse field storage types */
    ///@{
    /**
     * @brief Collapses common-part instance fields of @JavaMirror/@ObjCMirror
     *        classes to Unit where a matching mirror getter exists, delegating
     *        to CHIRFieldPatcher. No-op unless compiling the CJMP specific part.
     */
    void PatchCommonFieldsForMirror();

    /**
     * @brief Same field-to-Unit collapse as PatchCommonFieldsForMirror, but
     *        for @JavaImpl classes matching on the JAVA_IMPL getter. No-op
     *        unless compiling the CJMP specific part.
     */
    void PatchCommonFieldsForJavaImpl();
    ///@}

    /** @name Phase 2b — rewrite field references */
    ///@{
    /**
     * @brief Rewrites field load/store in common-part method bodies of
     *        @JavaMirror and @ObjCMirror classes to call mirror JNI
     *        getter/setters instead of direct field access, delegating to
     *        CHIRFieldReferencePatcher (run for both Java and ObjC attributes).
     *        No-op unless compiling the CJMP specific part.
     */
    void PatchFieldReferencesForMirror();

    /**
     * @brief Patches field references in @JavaImpl constructor/method bodies
     *        inherited from the common part, redirecting direct field access
     *        to impl-side accessors, delegating to PatchCJMPImplBodies::Run.
     *        No-op unless compiling the CJMP specific part.
     */
    void PatchFieldReferencesForJavaImpl();
    ///@}

    /** @name Phase 3 — after all decls translated */
    ///@{
    /**
     * @brief Inserts field-initialization calls for non-MUT fields with
     *        initializerFunc in @JavaImpl FROM_COMMON_PART constructors. Must
     *        run after constructor bodies are translated. Delegates to
     *        PatchCJMPImplBodies::RunCtorFieldInitOnly. No-op unless compiling
     *        the CJMP specific part.
     */
    void PatchCtorFieldInitForJavaImpl();
    ///@}

private:
    /**
     * @brief Translates the AST superclass type of @p astDecl into a CHIR
     *        ClassType, unwrapping a RefType wrapper when the translated super
     *        type is a reference to a class.
     * @return The ClassType, or null if @p astDecl has no superclass, the type
     *         fails to translate, or the result is neither a class nor a
     *         ref-to-class.
     */
    ClassType* TranslateSuperClassToChirType(const AST::ClassLikeDecl& astDecl);

    /**
     * @brief Returns true if the superclass of @p astDecl is a platform
     *        "mirror" class (@JavaMirror or @ObjCMirror).
     *
     * The CHIR superclass ClassDef is checked first (via IsPlatformMirrorClass);
     * if inconclusive, the AST superclass declaration's annotations are
     * consulted as a fallback.
     */
    bool IsPlatformMirrorSuperClass(const AST::ClassLikeDecl& astDecl, const ClassType& chirSuperClassTy);

    /** Cached result of opts.IsCompilingCJMPSpecific(); when false every method is a no-op. */
    bool compilingSpecific;
    Package& pkg;
    CHIRBuilder& builder;
    CHIRType& chirType;
};

} // namespace Cangjie::CHIR

#endif // CANGJIE_CHIR_TRANSFORMATION_PATCH_CJMP_MEMBERS_H
