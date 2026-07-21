// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares sema after-typecheck context for java FFI compiler stages.
 */
#ifndef CANGJIE_SEMA_NATIVE_FFI_JAVA_AFTER_TYPECHECK_CONTEXT
#define CANGJIE_SEMA_NATIVE_FFI_JAVA_AFTER_TYPECHECK_CONTEXT

#include "cangjie/AST/Node.h"
#include "cangjie/Modules/ImportManager.h"
#include "cangjie/Sema/TypeManager.h"
#include "cangjie/Utils/SafePointer.h"
#include <unordered_map>

namespace Cangjie::Native::FFI::Java {

struct AfterTypeCheckContext {
    explicit AfterTypeCheckContext(const ImportManager& importManager, TypeManager& typeManager, AST::Package& pkg);

    std::vector<Ptr<AST::ClassLikeDecl>> GetJavaMirrors() const;
    std::vector<Ptr<AST::ClassDecl>> GetJavaImplReferenceWrappers() const;
    std::vector<Ptr<AST::ClassDecl>> GetJavaImplRegistryCompanions() const;
    AST::ClassDecl& GetJavaImplRegistryCompanion(const AST::ClassDecl& referenceWrapper) const;
    void CacheJavaImplReferenceWrapperConstructorsPair(AST::FuncDecl& userDefinedCtor, AST::FuncDecl& generatedCtor);
    AST::FuncDecl& GetJavaImplReferenceWrapperGeneratedConstructor(AST::FuncDecl& userDefinedCtor);

    void CacheJavaImplRegistryCompanionReferenceField(AST::ClassDecl& refWrapper, AST::VarDecl& field);
    AST::VarDecl& GetJavaImplRegistryCompanionReferenceField(AST::ClassDecl& refWrapper);

    void CacheJavaImplWrappingConstructor(AST::ClassDecl& refWrapper, AST::FuncDecl& wrappingCtor);
    AST::FuncDecl& GetJavaImplWrappingConstructor(AST::ClassDecl& refWrapper);

    std::vector<Ptr<AST::FuncDecl>> GetJavaImplUserDefinedConstructors(AST::ClassDecl& refWrapper);

    void CacheJavaImplSuperCtorArgNativeFunc(const AST::FuncDecl& ctor, Ptr<AST::FuncDecl> nativeFunc);
    const std::vector<Ptr<AST::FuncDecl>> GetJavaImplSuperCtorArgNativeFuncs(const AST::FuncDecl& ctor);
    bool HasDesugaredJavaImplSuperConstructorCall(const AST::FuncDecl& userCtor) const;

    void CacheJavaImplSuperCtorCall(const AST::FuncDecl& ctor, OwnedPtr<AST::CallExpr>&& call);
    AST::CallExpr& GetJavaImplSuperCtorCall(const AST::FuncDecl& ctor);

    void AddGeneratedDecl(OwnedPtr<AST::Decl>&& decl);

    void FlushGeneratedDecls();

    const ImportManager& importManager;
    TypeManager& typeManager;
    AST::Package& pkg;
private:
    const std::vector<Ptr<AST::ClassLikeDecl>> javaMirrors;
    const std::vector<Ptr<AST::ClassDecl>> javaImplReferenceWrappers;
    const std::vector<Ptr<AST::ClassDecl>> javaImplRegistryCompanions;

    /**
     * Key: user-defined constructor in @JavaImpl reference wrapper (callable in cangjie).
     * Value: corresponding generated constructor (callable in java).
     */
    std::unordered_map<Ptr<AST::FuncDecl>, Ptr<AST::FuncDecl>> javaImplRefWrapperCjToJavaConstructors;

    /**
     * Key: reference wrapper class.
     * Value: corresponding registry companion reference field within reference wrapper (`$reg`).
     */
    std::unordered_map<Ptr<AST::ClassDecl>, Ptr<AST::VarDecl>> javaImplRegistryCompanionRefFields;

    /**
     * Key: reference wrapper class.
     * Value: corresponding wrapping constructor.
     */
    std::unordered_map<Ptr<AST::ClassDecl>, Ptr<AST::FuncDecl>> javaImplWrappingConstructors;

    /**
     * Key: user-defined constructor in @JavaImpl.
     * Value: vector of corresponding generated native functions with the meaning of argument value computation.
     */
    std::unordered_map<Ptr<const AST::FuncDecl>, std::vector<Ptr<AST::FuncDecl>>> javaImplUserCtorToNativeFuncs;

    /**
     * Key: user-defined constructor in @JavaImpl.
     * Value: original, user-defined super constructor call within corresponding constructor.
     */
    std::unordered_map<Ptr<const AST::FuncDecl>, OwnedPtr<AST::CallExpr>> javaImplUserCtorToOriginalSuperCall;

    // Generated top-level declaration.
    std::vector<OwnedPtr<AST::Decl>> generated;
};


} // namespace Cangjie::Native::FFI::Java

#endif // CANGJIE_SEMA_NATIVE_FFI_JAVA_AFTER_TYPECHECK_CONTEXT
