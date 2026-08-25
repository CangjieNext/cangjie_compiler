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

#ifndef CANGJIE_SEMA_NATIVEFFI_OBJC_UTILS_ASTQUERY_H
#define CANGJIE_SEMA_NATIVEFFI_OBJC_UTILS_ASTQUERY_H

#include "cangjie/AST/Match.h"
#include "cangjie/AST/Node.h"
#include "cangjie/Modules/ImportManager.h"

namespace Cangjie::Interop::ObjC {

bool IsObjCCompatible(const AST::Ty& ty) noexcept;
bool IsObjCObjectType(const AST::Ty& ty) noexcept;
bool IsObjCMirror(const AST::Decl& decl) noexcept;
/**
 * @note returns false for non ClassLike tys, e.g. for @ObjCMirror func.
 * Use AST::Decl overload instead.
 */
bool IsObjCMirror(const AST::Ty& ty) noexcept;
bool IsObjCMirrorSubtype(const AST::Decl& decl) noexcept;
bool IsObjCMirrorSubtype(const AST::Ty& ty) noexcept;
bool IsValidObjCMirror(const AST::Ty& ty) noexcept;
bool IsObjCImpl(const AST::ClassDecl& decl) noexcept;
bool IsObjCImpl(const AST::Ty& ty) noexcept;
bool IsValidObjCImpl(const AST::Ty& ty) noexcept;
bool IsObjCMirrorInterfaceHandleWrapper(const AST::ClassDecl& decl) noexcept;
bool IsObjCMirrorInterfaceHandleWrapper(const AST::Ty& ty) noexcept;
bool IsObjCImplRegistryCompanion(const AST::Decl& decl) noexcept;
bool IsObjCImplRegistryCompanion(const AST::Ty& ty) noexcept;
bool IsObjCPointer(const AST::Decl& decl) noexcept;
bool IsObjCPointer(const AST::Ty& ty) noexcept;
bool IsObjCFunc(const AST::Decl& decl) noexcept;
bool IsObjCFunc(const AST::Ty& ty) noexcept;
bool IsObjCBlock(const AST::Decl& decl) noexcept;
bool IsObjCBlock(const AST::Ty& ty) noexcept;
bool IsObjCId(const AST::Ty& ty) noexcept;
bool IsObjCId(const AST::Decl& decl) noexcept;
bool IsObjCFuncOrBlock(const AST::Decl& decl) noexcept;
bool IsObjCFuncOrBlock(const AST::Ty& ty) noexcept;
bool IsNSObject(const AST::ClassDecl& cd) noexcept;
bool IsNSString(const AST::ClassDecl& cd) noexcept;

Ptr<AST::ClassDecl> GetObjCMirrorInterfaceHandleWrapper(
    ImportManager& importManager, const AST::InterfaceDecl& decl) noexcept;

bool HasObjCMirrorSuperInterface(const AST::ClassLikeDecl& decl) noexcept;

Ptr<AST::ClassDecl> GetObjCMirrorSuperClass(const AST::ClassDecl& decl) noexcept;
bool HasObjCMirrorSuperClass(const AST::ClassDecl& decl) noexcept;

Ptr<AST::ClassDecl> GetObjCImplSuperClass(const AST::ClassDecl& decl) noexcept;
bool HasObjCImplSuperClass(const AST::ClassDecl& decl) noexcept;

Ptr<AST::ClassDecl> GetObjCImplRegistryCompanionSuperClass(const AST::ClassDecl& decl) noexcept;
bool HasObjCImplRegistryCompanionSuperClass(const AST::ClassDecl& decl) noexcept;

bool IsNativeHandleGetter(const AST::Decl& decl) noexcept;
Ptr<AST::FuncDecl> GetNativeHandleGetter(const AST::ClassLikeDecl& decl) noexcept;

Ptr<AST::VarDecl> GetNativeHandleField(const AST::ClassDecl& decl) noexcept;

Ptr<AST::VarDecl> GetObjCImplRegCompanionField(const AST::ClassDecl& decl) noexcept;
bool IsObjCImplRegistryCompanionField(const AST::Decl& decl) noexcept;

/**
 * @returns the first member of @p decl of AST kind @p K satisfying @p pred, or nullptr if there is none.
 */
template <AST::ASTKind K = AST::ASTKind::DECL, typename Pred>
auto GetMemberDecl(const AST::InheritableDecl& decl, Pred pred) noexcept
{
    auto& decls = decl.GetMemberDecls();
    auto found = std::find_if(decls.begin(), decls.end(), [&pred](auto& member) {
        auto typedMember = AST::As<K>(member.get());
        return typedMember && pred(*typedMember);
    });
    if (found != decls.end()) {
        return Ptr(AST::As<K>(found->get()));
    }
    return Ptr(AST::As<K>(nullptr));
}

Ptr<AST::FuncDecl> GetFinalizer(const AST::ClassDecl& decl) noexcept;

bool IsObjCInitMethod(const AST::FuncDecl& decl) noexcept;

/** 
* @returns true for a base ctor of any @ObjC object type 
*/
bool IsBaseCtor(const AST::FuncDecl& fd) noexcept;

Ptr<AST::FuncDecl> GetBaseCtor(const AST::ClassDecl& decl) noexcept;

bool IsObjCMirrorBaseCtor(const AST::FuncDecl& fd) noexcept;
Ptr<AST::FuncDecl> GetObjCMirrorBaseCtor(const AST::ClassDecl& decl) noexcept;

bool IsObjCMirrorInterfaceHandleWrapperBaseCtor(const AST::FuncDecl& fd) noexcept;
Ptr<AST::FuncDecl> GetObjCMirrorInterfaceHandleWrapperBaseCtor(const AST::ClassDecl& handleWrapper) noexcept;

bool IsObjCImplBaseCtor(const AST::FuncDecl& fd) noexcept;
Ptr<AST::FuncDecl> GetObjCImplBaseCtor(const AST::ClassDecl& decl) noexcept;

bool IsObjCImplRegCompanionBaseCtor(const AST::FuncDecl& fd) noexcept;
Ptr<AST::FuncDecl> GetObjCImplRegCompanionBaseCtor(const AST::ClassDecl& decl) noexcept;

bool IsObjCImplRegDataCtor(const AST::FuncDecl& fd) noexcept;
Ptr<AST::FuncDecl> GetObjCImplRegDataCtor(const AST::ClassDecl& impl, const AST::FuncDecl& origin) noexcept;

bool IsObjCImplUserCtor(const AST::FuncDecl& fd) noexcept;

bool IsGeneratedNSStringCtor(const AST::FuncDecl& fd) noexcept;
bool IsGeneratedNSObjectToString(const AST::FuncDecl& fd) noexcept;

bool IsGeneratedMember(const AST::Decl& decl) noexcept;

} // namespace Cangjie::Interop::ObjC

#endif // CANGJIE_SEMA_NATIVEFFI_OBJC_UTILS_ASTQUERY_H
