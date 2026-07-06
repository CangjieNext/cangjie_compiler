// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/AST2CHIR/AST2CHIR.h"

#include <unordered_map>

#include "cangjie/CHIR/IR/Type/ClassDef.h"
#include "cangjie/CHIR/Transformation/CHIRFieldPatcher.h"
#include "cangjie/CHIR/Transformation/CHIRFieldReferencePatcher.h"
#include "cangjie/CHIR/Transformation/PatchCJMPImplBodies.h"
#include "cangjie/CHIR/Utils/CHIRCasting.h"
#include "cangjie/CHIR/Utils/PlatformFFIClass.h"
#include "cangjie/Utils/CastingTemplate.h"

namespace Cangjie::CHIR {
void AST2CHIR::PatchCommonFieldsForMirror()
{
    if (!opts.IsCompilingCJMPSpecific()) {
        return;
    }

    CHIRFieldPatcher patcher(builder);
    for (auto& classDef : package->GetClasses()) {
        if (classDef->TestAttr(Attribute::JAVA_MIRROR)) {
            patcher.PatchCommonFieldsForMirror(classDef, Attribute::JAVA_MIRROR);
        }
        if (classDef->TestAttr(Attribute::OBJ_C_MIRROR)) {
            patcher.PatchCommonFieldsForMirror(classDef, Attribute::OBJ_C_MIRROR);
        }
    }
}

void AST2CHIR::PatchCommonFieldsForJavaImpl()
{
    if (!opts.IsCompilingCJMPSpecific()) {
        return;
    }

    CHIRFieldPatcher patcher(builder);
    for (auto& classDef : package->GetClasses()) {
        if (classDef->TestAttr(Attribute::JAVA_IMPL)) {
            patcher.PatchCommonFieldsForJavaImpl(classDef);
        }
    }
}

void AST2CHIR::PatchFieldReferencesForMirror()
{
    if (!opts.IsCompilingCJMPSpecific()) {
        return;
    }

    CHIRFieldReferencePatcher javaPatcher(*package, builder, Attribute::JAVA_MIRROR);
    javaPatcher.Run();

    CHIRFieldReferencePatcher objcPatcher(*package, builder, Attribute::OBJ_C_MIRROR);
    objcPatcher.Run();
}

void AST2CHIR::PatchFieldReferencesForJavaImpl()
{
    if (!opts.IsCompilingCJMPSpecific()) {
        return;
    }

    PatchCJMPImplBodies(*package, builder).Run();
}

void AST2CHIR::PatchCtorFieldInitForJavaImpl()
{
    if (!opts.IsCompilingCJMPSpecific()) {
        return;
    }

    PatchCJMPImplBodies(*package, builder).RunCtorFieldInitOnly();
}

ClassType* AST2CHIR::TranslateSuperClassToChirType(const AST::ClassLikeDecl& astDecl)
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

bool AST2CHIR::IsPlatformMirrorSuperClass(const AST::ClassLikeDecl& astDecl, const ClassType& chirSuperClassTy)
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

void AST2CHIR::PatchCJMPSuperClass()
{
    if (!opts.IsCompilingCJMPSpecific()) {
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

    for (auto classDef : package->GetAllClassDef()) {
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
