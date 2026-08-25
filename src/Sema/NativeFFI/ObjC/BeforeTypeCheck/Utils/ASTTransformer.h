// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * TODO: desc
 */

#ifndef CANGJIE_SEMA_NATIVEFFI_OBJC_BEFORETYPECHECK_UTILS_ASTTRANSFORMER_H
#define CANGJIE_SEMA_NATIVEFFI_OBJC_BEFORETYPECHECK_UTILS_ASTTRANSFORMER_H

#include "NativeFFI/ObjC/Utils/ASTFactory.h"
#include "NativeFFI/ObjC/Utils/NameGenerator.h"
#include "cangjie/AST/Node.h"
#include "cangjie/Modules/ImportManager.h"
#include "cangjie/Sema/TypeManager.h"

namespace Cangjie::Interop::ObjC {
class ASTTransformer {
public:
    ASTTransformer(TypeManager& typeManager, ASTFactory& astFactory) noexcept;
    void TransformToRegistryCompanion(AST::ClassDecl& regComp, const AST::ClassDecl& impl) const noexcept;
    // @note mirror is not a const reference, because CreateRefType a requires mutable one.
    void TransformToHandleWrapper(AST::ClassDecl& handleWrapper, AST::ClassLikeDecl& mirror) const noexcept;
    void TransformToToString(AST::FuncDecl& target) const noexcept;
    void TransformToInitFromString(AST::FuncDecl& target) const noexcept;

private:
    NameGenerator nameGenerator;
    ASTFactory& astFactory;
};

} // namespace Cangjie::Interop::ObjC

#endif // CANGJIE_SEMA_NATIVEFFI_OBJC_BEFORETYPECHECK_UTILS_ASTTRANSFORMER_H