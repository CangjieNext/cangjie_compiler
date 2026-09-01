// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares common context for the PrepareTypeCheck handlers of Cangjie <-> Objective-C interopability.
 */

#ifndef CANGJIE_SEMA_NATIVEFFI_OBJC_BEFORETYPECHECK_CONTEXT_H
#define CANGJIE_SEMA_NATIVEFFI_OBJC_BEFORETYPECHECK_CONTEXT_H

#include "NativeFFI/ObjC/BeforeTypeCheck/Utils/BeforeTypeCheckASTTransformer.h"
#include "NativeFFI/ObjC/Utils/ASTInserter.h"
#include "cangjie/AST/Node.h"
#include "cangjie/Mangle/BaseMangler.h"
#include "cangjie/Modules/ImportManager.h"
#include "cangjie/Sema/TypeManager.h"

namespace Cangjie::Interop::ObjC {

struct PrepareTypeCheckContext {
    PrepareTypeCheckContext(AST::Package& pkg, TypeManager& typeManager, ImportManager& importManager) noexcept;

    AST::Package& pkg;
    std::vector<Ptr<AST::ClassLikeDecl>> mirrors;
    std::vector<Ptr<AST::ClassDecl>> impls;

    TypeManager& typeManager;
    ImportManager& importManager;
    BeforeTypeCheckASTTransformer astTransformer;
    ASTInserter astInserter;
    BaseMangler mangler;
    NameGenerator nameGenerator;
};

} // namespace Cangjie::Interop::ObjC

#endif // CANGJIE_SEMA_NATIVEFFI_OBJC_BEFORETYPECHECK_CONTEXT_H
