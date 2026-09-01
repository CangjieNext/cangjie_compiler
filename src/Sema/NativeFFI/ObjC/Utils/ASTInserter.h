// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares the inserter of generated declarations of Cangjie <-> Objective-C interopability.
 * The inserter binds an inserted declaration to the location of its new owner, see InsertMode for the details.
 */

#ifndef CANGJIE_SEMA_NATIVEFFI_OBJC_UTILS_ASTINSERTER_H
#define CANGJIE_SEMA_NATIVEFFI_OBJC_UTILS_ASTINSERTER_H

#include "cangjie/AST/Node.h"

namespace Cangjie::Interop::ObjC {

/**
 * Defines how deeply an inserted node is bound to its new location.
 */
enum class InsertMode : uint8_t {
    /**
     * Bind the inserted node only. Its subnodes keep whatever state they already have.
     */
    SHALLOW,
    /**
     * Bind the inserted node and every node of its subtree.
     *
     * Use it for hand-built subtrees whose nodes were never bound to a file, or for subtrees relocated to
     * another file. Note that this overwrites the source ranges of the whole subtree, so it must not be used
     * on trees containing user-written code.
     */
    RECURSIVE,
};

class ASTInserter {
public:
    /**
     * Bind a declaration to @p target without inserting it into the class body.
     *
     * Use it for a generated declaration that has to know its owner before it can be filled in, and is
     * inserted into the body later.
     */
    void BindTo(AST::ClassLikeDecl& target, AST::Decl& decl, InsertMode mode = InsertMode::SHALLOW) const noexcept;

    void InsertInto(AST::File& target, OwnedPtr<AST::Decl> decl, InsertMode mode = InsertMode::SHALLOW) const noexcept;
    void InsertInto(
        AST::ClassLikeDecl& target, OwnedPtr<AST::Decl> decl, InsertMode mode = InsertMode::SHALLOW) const noexcept;
    void InsertGetterInto(AST::PropDecl& target, OwnedPtr<AST::FuncDecl> getterDecl,
        InsertMode mode = InsertMode::SHALLOW) const noexcept;
    void InsertSetterInto(AST::PropDecl& target, OwnedPtr<AST::FuncDecl> setterDecl,
        InsertMode mode = InsertMode::SHALLOW) const noexcept;
};

} // namespace Cangjie::Interop::ObjC

#endif // CANGJIE_SEMA_NATIVEFFI_OBJC_UTILS_ASTINSERTER_H
