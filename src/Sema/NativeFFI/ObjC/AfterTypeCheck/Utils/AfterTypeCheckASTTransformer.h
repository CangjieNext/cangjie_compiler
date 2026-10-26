// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares AST transformations for AfterTypeCheck ObjC interop.
 * Every transformation takes an already created node and fills it in place.
 */

#ifndef CANGJIE_SEMA_NATIVEFFI_OBJC_AFTERTYPECHECK_UTILS_ASTTRANSFORMER_H
#define CANGJIE_SEMA_NATIVEFFI_OBJC_AFTERTYPECHECK_UTILS_ASTTRANSFORMER_H

#include "NativeFFI/ObjC/Utils/ASTInserter.h"
#include "NativeFFI/ObjC/Utils/InteropLibBridge.h"
#include "cangjie/AST/Node.h"
#include "cangjie/Modules/ImportManager.h"
#include "cangjie/Sema/TypeManager.h"

namespace Cangjie::Interop::ObjC {
class AfterTypeCheckASTTransformer {
public:
    AfterTypeCheckASTTransformer(ImportManager& importManager, TypeManager& typeManager, DiagnosticEngine& diag,
        ASTInserter& astInserter) noexcept;
    void TransformToObjCImplRegCompanionField(AST::VarDecl& vd, AST::ClassDecl& regComp) const noexcept;

    void TransformToObjCImplStaticProxyFunc(
        AST::FuncDecl& proxy, AST::FuncDecl& origin, AST::ClassDecl& regCompanion) const noexcept;
    /**
     * @param receiver decl the accessors read the moved field through: the `$reg` field for an instance field,
     *                 the registry companion class itself for a static one.
     */
    void TransformToObjCImplProxyProp(AST::PropDecl& pd, AST::VarDecl& vd, AST::Decl& receiver) const noexcept;

    void TransformToObjCImplBaseCtorDecl(AST::FuncDecl& ctor, AST::ClassDecl& impl) const noexcept;
    void TransformToObjCImplBaseCtorBody(
        AST::FuncBody& ctorBody, AST::ClassDecl& impl, AST::ClassDecl& regComp) const noexcept;

    void TransformToObjCImplRegCompBaseCtorBody(AST::FuncBody& ctorBody, AST::ClassDecl& regComp) const noexcept;

    void TransformToObjCImplRegDataCtorDecl(
        AST::FuncDecl& ctor, AST::ClassDecl& impl, AST::ClassDecl& regComp) const noexcept;
    /**
     * @param objCClassRef expression yielding the `NativeObjCClass` handle of `impl`, e.g. the cached class access.
     */
    void TransformToObjCImplUserCtorBody(AST::FuncBody& ctorBody, AST::FuncDecl& regDataCtor, AST::ClassDecl& impl,
        AST::ClassDecl& regComp, OwnedPtr<AST::Expr> objCClassRef) const noexcept;

private:
    /**
     * Appends the marker argument to @p call if the ctor it resolves to takes one.
     *
     * The marker makes the signature of a generated ctor unwritable by a user, which is what tells it apart
     * from a user ctor of otherwise equal signature - `NativeObjCId` is an alias of `CPointer<Unit>` and so
     * `RegistryId` of `Int64`, both of them plain ObjC-compatible types a user may take. Whether a callee
     * carries the marker is therefore decided by the type of its params and never by their names, which take
     * no part in overload resolution; BACKCOMPAT, a mirror compiled before `CPointer<T>` became
     * ObjC-compatible has a marker-less base ctor and such a call is left alone.
     *
     * ASTFactory carries the same helper for the call sites still living there; the two collapse
     * into this one once that factory is gone.
     */
    void AppendNativeObjCIdMarkerIfNeeded(AST::CallExpr& call, Ptr<AST::File> curFile) const noexcept;

    void TransformToProxyPropGetter(
        AST::FuncDecl& getter, AST::VarDecl& origin, AST::Decl& receiver, AST::PropDecl& pd) const noexcept;
    void TransformToProxyPropSetter(
        AST::FuncDecl& setter, AST::VarDecl& origin, AST::Decl& receiver, AST::PropDecl& pd) const noexcept;

    TypeManager& typeManager;
    ASTInserter& astInserter;
    InteropLibBridge objCLib;
};

} // namespace Cangjie::Interop::ObjC

#endif // CANGJIE_SEMA_NATIVEFFI_OBJC_AFTERTYPECHECK_UTILS_ASTTRANSFORMER_H
