// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares MemberSignature struct.
 */

#ifndef CANGJIE_SEMA_MEMBER_SIGNATURE_H
#define CANGJIE_SEMA_MEMBER_SIGNATURE_H

#include "cangjie/AST/Nodes/Node.h"

namespace Cangjie {
namespace AST {
struct ExtendDecl;
}

struct MemberSignature {
    Ptr<AST::Decl> decl = nullptr;

    AST::ModalTy GetTy() const
    {
        return ty;
    }

    void SetTy(AST::ModalTy newTy)
    {
        ty = newTy;
    }

    AST::ModalTy structTy = nullptr;
    Ptr<AST::ExtendDecl> extendDecl = nullptr; // If the member is a member of another visible extension, it points to the
                                          // extension declaration. Otherwise, it is null.
    std::vector<std::unordered_set<AST::ModalTy>> upperBounds;
    std::unordered_set<Ptr<const AST::Ty>> inconsistentTypes; // List of the corresponding types came from super-types which
                                                         // are inconsistent
    bool shouldBeImplemented = false;                    // True: if this member has multiple default implementation.
    bool replaceOther = false;                           // True: if this member override others.
    bool isInheritedInterface = false; // True: if current member is implementing inherited interface decl.

    MemberSignature() = default;

    MemberSignature(Ptr<AST::Decl> declIn, AST::ModalTy memberTyIn, AST::ModalTy structTyIn = nullptr,
        Ptr<AST::ExtendDecl> extendDeclIn = nullptr,
        std::vector<std::unordered_set<AST::ModalTy>> upperBoundsIn = {})
        : decl(std::move(declIn)),
          structTy(std::move(structTyIn)),
          extendDecl(std::move(extendDeclIn)),
          upperBounds(std::move(upperBoundsIn)),
          ty(std::move(memberTyIn))
    {
    }

private:
    AST::ModalTy ty = nullptr;
};

using MemberMap = std::multimap<std::string, MemberSignature>;

} // namespace Cangjie

#endif
