// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// Licensed under Apache-2.0 with Runtime Library Exception.
#ifndef CANGJIE_AST_ARKTS_H
#define CANGJIE_AST_ARKTS_H

#include "cangjie/AST/Node.h"
#include "cangjie/AST/Match.h"

namespace Cangjie::AST {
// Foreign members inherit the ArkTS ABI from their declaring class/interface.
inline bool IsArkTSForeignMember(const Decl& decl)
{
    for (auto owner = decl.outerDecl; owner; owner = owner->outerDecl) {
        if (owner->IsClassLikeDecl() || owner->astKind == ASTKind::STRUCT_DECL ||
            owner->astKind == ASTKind::ENUM_DECL || owner->astKind == ASTKind::EXTEND_DECL) {
            return owner->IsClassLikeDecl() && owner->HasAnno(AnnotationKind::ARKTS) &&
                owner->TestAttr(Attribute::FOREIGN);
        }
    }
    return false;
}

// Resolve aliases first: a spelling alone cannot identify the SDK macro.
inline bool IsMixedArkTSInterop(const MacroExpandDecl& call, const FuncDecl& definition)
{
    return definition.fullPackageName == "ohos.ark_interop_macro" && definition.identifier == "Interop" &&
        (call.HasAnno(AnnotationKind::ARKTS) ||
            (call.invocation.decl && call.invocation.decl->HasAnno(AnnotationKind::ARKTS)));
}

// Keep early entry generation and semantic acceptance on the same contract.
inline bool IsMinimalArkTSEntry(const FuncDecl& fd)
{
    if (!fd.HasAnno(AnnotationKind::ARKTS) || fd.TestAttr(Attribute::FOREIGN) ||
        !fd.TestAttr(Attribute::PUBLIC) || fd.outerDecl || !fd.funcBody ||
        !fd.funcBody->body || fd.GetGeneric() || fd.annotations.size() != 1) {
        return false;
    }
    for (auto& modifier : fd.modifiers) {
        if (modifier.modifier == TokenKind::CONST) {
            return false;
        }
    }
    for (auto& annotation : fd.annotations) {
        if (!annotation->args.empty()) {
            return false;
        }
    }
    for (auto& list : fd.funcBody->paramLists) {
        if (!list->params.empty() || list->hasVariableLenArg) {
            return false;
        }
    }
    auto ret = DynamicCast<PrimitiveType*>(fd.funcBody->retType.get());
    return ret && ret->str == "Unit";
}
} // namespace Cangjie::AST
#endif
