// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/Transformation/PatchCJMPMembers.h"

#include <unordered_map>

#include "cangjie/CHIR/IR/Type/ClassDef.h"
#include "cangjie/CHIR/Transformation/CHIRFieldPatcher.h"
#include "cangjie/CHIR/Transformation/CHIRFieldReferencePatcher.h"
#include "cangjie/CHIR/Transformation/PatchCJMPImplBodies.h"
#include "cangjie/CHIR/Utils/CHIRCasting.h"
#include "cangjie/CHIR/Utils/PlatformFFIClass.h"
#include "cangjie/Utils/CastingTemplate.h"

namespace Cangjie::CHIR {
PatchCJMPMembers::PatchCJMPMembers(const GlobalOptions& opts, Package& pkg, CHIRBuilder& builder, CHIRType& chirType)
    : compilingSpecific(opts.IsCompilingCJMPSpecific()), pkg(pkg), builder(builder), chirType(chirType)
{
}

/**
 * Patches common-part instance fields of @JavaMirror/@ObjCMirror classes during
 * CJMP specific compilation. For each mirror class, delegates to
 * CHIRFieldPatcher to collapse a field's CHIR type to Unit where a matching
 * mirror getter exists, since field access is redirected through JNI
 * getter/setter calls (patched by PatchFieldReferencesForMirror). No-op unless
 * compiling the CJMP specific part.
 */
void PatchCJMPMembers::PatchCommonFieldsForMirror()
{
    if (!compilingSpecific) {
        return;
    }

    CHIRFieldPatcher patcher(builder);
    for (auto& classDef : pkg.GetClasses()) {
        if (classDef->TestAttr(Attribute::JAVA_MIRROR)) {
            patcher.PatchCommonFieldsForMirror(classDef, Attribute::JAVA_MIRROR);
        }
        if (classDef->TestAttr(Attribute::OBJ_C_MIRROR)) {
            patcher.PatchCommonFieldsForMirror(classDef, Attribute::OBJ_C_MIRROR);
        }
    }
}

/**
 * Patches common-part instance fields of @JavaImpl classes during CJMP specific
 * compilation. Same field-to-Unit collapse as PatchCommonFieldsForMirror, but
 * matching on the JAVA_IMPL getter. No-op unless compiling the CJMP specific
 * part.
 */
void PatchCJMPMembers::PatchCommonFieldsForJavaImpl()
{
    if (!compilingSpecific) {
        return;
    }

    CHIRFieldPatcher patcher(builder);
    for (auto& classDef : pkg.GetClasses()) {
        if (classDef->TestAttr(Attribute::JAVA_IMPL)) {
            patcher.PatchCommonFieldsForJavaImpl(classDef);
        }
    }
}

/**
 * Rewrites field load/store instructions in common-part method bodies of
 * @JavaMirror and @ObjCMirror classes to call the mirror JNI getter/setter
 * instead of accessing the field directly. Runs CHIRFieldReferencePatcher for
 * both the Java and ObjC mirror attributes. No-op unless compiling the CJMP
 * specific part.
 */
void PatchCJMPMembers::PatchFieldReferencesForMirror()
{
    if (!compilingSpecific) {
        return;
    }

    CHIRFieldReferencePatcher javaPatcher(pkg, builder, Attribute::JAVA_MIRROR);
    javaPatcher.Run();

    CHIRFieldReferencePatcher objcPatcher(pkg, builder, Attribute::OBJ_C_MIRROR);
    objcPatcher.Run();
}

/**
 * Patches field references in @JavaImpl constructor and method bodies
 * inherited from the common part, redirecting direct field access to the
 * impl-side accessors. Delegates to PatchCJMPImplBodies::Run. No-op unless
 * compiling the CJMP specific part.
 */
void PatchCJMPMembers::PatchFieldReferencesForJavaImpl()
{
    if (!compilingSpecific) {
        return;
    }

    PatchCJMPImplBodies(pkg, builder).Run();
}

/**
 * Patches field initialization in @JavaImpl constructors inherited from the
 * common part. Delegates to PatchCJMPImplBodies::RunCtorFieldInitOnly, which
 * processes only the constructor field-init sites rather than full method
 * bodies. No-op unless compiling the CJMP specific part.
 */
void PatchCJMPMembers::PatchCtorFieldInitForJavaImpl()
{
    if (!compilingSpecific) {
        return;
    }

    PatchCJMPImplBodies(pkg, builder).RunCtorFieldInitOnly();
}

/**
 * Translates the AST superclass type of @p astDecl into a CHIR ClassType,
 * unwrapping a RefType wrapper when the translated super type is a reference to
 * a class. Returns null if @p astDecl has no superclass, the type fails to
 * translate, or the result is neither a class nor a ref-to-class.
 */
ClassType* PatchCJMPMembers::TranslateSuperClassToChirType(const AST::ClassLikeDecl& astDecl)
{
    auto astClassTy = DynamicCast<AST::ClassTy*>(astDecl.GetTy());
    if (!astClassTy) return nullptr;

    auto superClassTy = astClassTy->GetSuperClassTy();
    if (!superClassTy) return nullptr;

    Type* chirSuperTy = chirType.TranslateType(*superClassTy);
    if (!chirSuperTy) return nullptr;

    if (chirSuperTy->IsClass()) {
        return StaticCast<ClassType*>(chirSuperTy);
    }
    if (chirSuperTy->IsRef()) {
        auto refTy = StaticCast<RefType*>(chirSuperTy);
        auto baseTy = refTy->GetBaseType();
        if (baseTy && baseTy->IsClass()) {
            return StaticCast<ClassType*>(baseTy);
        }
    }
    return nullptr;
}

/**
 * Returns true if the superclass of @p astDecl is a platform "mirror" class
 * (@JavaMirror or @ObjCMirror). The CHIR superclass ClassDef is checked first
 * (via IsPlatformMirrorClass); if that is inconclusive, the AST superclass
 * declaration's annotations are consulted as a fallback. Used by
 * PatchSuperClass to decide whether the Object super reference should be
 * repointed to the platform parent.
 */
bool PatchCJMPMembers::IsPlatformMirrorSuperClass(
    const AST::ClassLikeDecl& astDecl, const ClassType& chirSuperClassTy)
{
    auto superClassDef = chirSuperClassTy.GetClassDef();
    if (superClassDef && IsPlatformMirrorClass(*superClassDef)) {
        return true;
    }

    auto astClassTy = DynamicCast<AST::ClassTy*>(astDecl.GetTy());
    if (!astClassTy) return false;

    auto astSuperClassTy = astClassTy->GetSuperClassTy();
    if (!astSuperClassTy || !astSuperClassTy->decl) return false;

    if (astSuperClassTy->decl->HasAnno(AST::AnnotationKind::JAVA_MIRROR)) return true;
    if (astSuperClassTy->decl->HasAnno(AST::AnnotationKind::OBJ_C_MIRROR)) return true;

    return false;
}

/**
 * Repoints the CHIR superclass reference of CJMP specific classes whose super
 * is the implicit Cangjie Object root to the real platform mirror superclass
 * (@JavaMirror/@ObjCMirror). A name->AST map of SPECIFIC class decls is built
 * once (across local and imported nominals) to look up the AST decl matching
 * each SPECIFIC CHIR ClassDef; its super type is translated to CHIR and, when
 * that super is a platform mirror, applied via SetSuperClassTy. Classes that
 * already inherit a real non-Object superclass are skipped
 * (HasObjectAsSuperClass). No-op unless compiling the CJMP specific part.
 */
void PatchCJMPMembers::PatchSuperClass(const std::vector<Ptr<const AST::Decl>>& nominalDecls,
    const std::vector<Ptr<const AST::Decl>>& importedNominalDecls)
{
    if (!compilingSpecific) {
        return;
    }

    std::unordered_map<std::string, const AST::ClassLikeDecl*> specificAstByName;
    specificAstByName.reserve(nominalDecls.size() + importedNominalDecls.size());
    for (const auto* container : {&nominalDecls, &importedNominalDecls}) {
        for (const auto& decl : *container) {
            if (decl->astKind == AST::ASTKind::CLASS_DECL &&
                decl->TestAttr(AST::Attribute::SPECIFIC)) {
                specificAstByName.try_emplace(decl->mangledName,
                    StaticCast<const AST::ClassLikeDecl*>(decl.get()));
            }
        }
    }

    for (auto classDef : pkg.GetAllClassDef()) {
        if (!classDef->TestAttr(Attribute::SPECIFIC)) {
            continue;
        }

        if (!HasObjectAsSuperClass(*classDef)) {
            continue;
        }

        auto it = specificAstByName.find(classDef->GetIdentifierWithoutPrefix());
        const AST::ClassLikeDecl* astDecl = (it != specificAstByName.end()) ? it->second : nullptr;
        if (!astDecl) {
            continue;
        }

        auto chirSuperClassTy = TranslateSuperClassToChirType(*astDecl);
        if (!chirSuperClassTy) {
            continue;
        }

        if (!IsPlatformMirrorSuperClass(*astDecl, *chirSuperClassTy)) {
            continue;
        }

        classDef->SetSuperClassTy(*chirSuperClassTy);
    }
}

} // namespace Cangjie::CHIR
