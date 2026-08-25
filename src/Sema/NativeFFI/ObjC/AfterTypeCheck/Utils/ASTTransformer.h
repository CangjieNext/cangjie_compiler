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

#ifndef CANGJIE_SEMA_NATIVEFFI_OBJC_AFTERTYPECHECK_UTILS_ASTTRANSFORMER_H
#define CANGJIE_SEMA_NATIVEFFI_OBJC_AFTERTYPECHECK_UTILS_ASTTRANSFORMER_H

#include "NativeFFI/ObjC/Utils/ASTFactory.h"
#include "NativeFFI/ObjC/Utils/ASTInserter.h"
#include "NativeFFI/ObjC/Utils/InteropLibBridge.h"
#include "NativeFFI/ObjC/Utils/NameGenerator.h"
#include "cangjie/AST/Node.h"
#include "cangjie/Modules/ImportManager.h"
#include "cangjie/Sema/TypeManager.h"

namespace Cangjie::Interop::ObjC {
class ASTTransformer {
public:
    ASTTransformer(ImportManager& importManager, TypeManager& typeManager, DiagnosticEngine& diag,
        ASTFactory& astFactory, ASTInserter& astInserter) noexcept;
    void TransformToObjCImplRegCompanionField(AST::VarDecl& vd, AST::ClassDecl& regComp) const noexcept;

    void TransformToObjCImplStaticProxyFunc(
        AST::FuncDecl& proxy, AST::FuncDecl& origin, AST::ClassDecl& regCompanion) const noexcept;
    void TransformToObjCImplProxyProp(AST::PropDecl& pd, AST::VarDecl& vd, AST::VarDecl& regCompField) const noexcept;

    void TransformToObjCImplBaseCtorDecl(AST::FuncDecl& ctor, AST::ClassDecl& impl) const noexcept;
    void TransformToObjCImplBaseCtorBody(
        AST::FuncBody& ctorBody, AST::ClassDecl& impl, AST::ClassDecl& regComp) const noexcept;

    void TransformToObjCImplRegCompBaseCtorBody(AST::FuncBody& ctorBody, AST::ClassDecl& regComp) const noexcept;

    void TransformToObjCImplRegDataCtorDecl(
        AST::FuncDecl& ctor, AST::ClassDecl& impl, AST::ClassDecl& regComp) const noexcept;
    void TransformToObjCImplUserCtorBody(AST::FuncBody& ctorBody, AST::FuncDecl& regDataCtor, AST::ClassDecl& impl,
        AST::ClassDecl& regComp) const noexcept;

private:
    void TransformToProxyPropGetter(
        AST::FuncDecl& getter, AST::VarDecl& origin, AST::VarDecl& regCompField, AST::PropDecl& pd) const noexcept;
    void TransformToProxyPropSetter(
        AST::FuncDecl& setter, AST::VarDecl& origin, AST::VarDecl& regCompField, AST::PropDecl& pd) const noexcept;

    NameGenerator nameGenerator;
    ImportManager& importManager;
    TypeManager& typeManager;
    ASTFactory& astFactory;
    ASTInserter& astInserter;
    InteropLibBridge objCLib;
};

} // namespace Cangjie::Interop::ObjC

#endif // CANGJIE_SEMA_NATIVEFFI_OBJC_AFTERTYPECHECK_UTILS_ASTTRANSFORMER_H