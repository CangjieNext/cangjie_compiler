// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares AST transformations for the PrepareTypeCheck stage of Cangjie <-> Objective-C
 * interopability. The transformed nodes are untyped, as they are to be resolved by the type checker.
 */

#ifndef CANGJIE_SEMA_NATIVEFFI_OBJC_BEFORETYPECHECK_UTILS_ASTTRANSFORMER_H
#define CANGJIE_SEMA_NATIVEFFI_OBJC_BEFORETYPECHECK_UTILS_ASTTRANSFORMER_H

#include "NativeFFI/ObjC/Utils/NameGenerator.h"
#include "cangjie/AST/Node.h"

namespace Cangjie::Interop::ObjC {
class BeforeTypeCheckASTTransformer {
public:
    void TransformToRegistryCompanion(AST::ClassDecl& regComp, const AST::ClassDecl& impl) const noexcept;
    // @note mirror is not a const reference, because CreateRefType requires a mutable one.
    void TransformToHandleWrapper(AST::ClassDecl& handleWrapper, AST::ClassLikeDecl& mirror) const noexcept;
    void TransformToToString(AST::FuncDecl& target) const noexcept;
    void TransformToInitFromString(AST::FuncDecl& target) const noexcept;
};

} // namespace Cangjie::Interop::ObjC

#endif // CANGJIE_SEMA_NATIVEFFI_OBJC_BEFORETYPECHECK_UTILS_ASTTRANSFORMER_H
