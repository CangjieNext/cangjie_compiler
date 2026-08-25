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

#ifndef CANGJIE_SEMA_NATIVEFFI_OBJC_UTILS_ASTFACTORY_H
#define CANGJIE_SEMA_NATIVEFFI_OBJC_UTILS_ASTFACTORY_H

#include "cangjie/AST/Node.h"

namespace Cangjie::Interop::ObjC {

class ASTFactory {
public:
    OwnedPtr<AST::ClassDecl> CreateClassDecl() const noexcept;
    OwnedPtr<AST::FuncDecl> CreateFuncDecl() const noexcept;
    OwnedPtr<AST::VarDecl> CreateVarDecl() const noexcept;
    OwnedPtr<AST::FuncBody> CreateFuncBody() const noexcept;
    OwnedPtr<AST::FuncParam> CreateFuncParam() const noexcept;
    OwnedPtr<AST::PropDecl> CreatePropDecl() const noexcept;
};

} // namespace Cangjie::Interop::ObjC

#endif // CANGJIE_SEMA_NATIVEFFI_OBJC_UTILS_ASTFACTORY_H
