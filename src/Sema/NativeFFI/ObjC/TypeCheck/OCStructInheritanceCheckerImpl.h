// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares utility class that process inheritance of structured types.
 */

#ifndef CANGJIE_SEMA_OBJ_C_TYPE_CHECK_STRUCT_INHERITANCE_CHECKER_IMPL_H
#define CANGJIE_SEMA_OBJ_C_TYPE_CHECK_STRUCT_INHERITANCE_CHECKER_IMPL_H

#include "InheritanceChecker/MemberSignature.h"
#include "cangjie/AST/Node.h"
#include "cangjie/Basic/DiagnosticEngine.h"
#include "cangjie/Sema/TypeManager.h"

namespace Cangjie::Native::FFI::ObjC {

void CheckAnnotation(DiagnosticEngine& diag, TypeManager& typeManager, AST::AnnotationKind annotationKind,
    const MemberSignature& parent, const MemberSignature& child, const AST::Decl& checkingDecl);
void CheckForeignAnnotations(DiagnosticEngine& diag, TypeManager& typeManager, const MemberSignature& parent,
    const MemberSignature& child, const AST::Decl& checkingDecl);

/**
 * Inserts stubs for members of mirror interface handle wrapper classes.
 */
void GenerateObjCForeignAbstractStub(const std::unordered_map<Ptr<const AST::InheritableDecl>, MemberMap>& memberMap);
} // namespace Cangjie::Native::FFI::ObjC

#endif // CANGJIE_SEMA_OBJ_C_TYPE_CHECK_STRUCT_INHERITANCE_CHECKER_IMPL_H
