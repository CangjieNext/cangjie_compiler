// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares after-typecheck Java interop stage: rewriting fields & access to them to the properties
 * within @JavaImpl reference wrappers.
 */
#ifndef CANGJIE_SEMA_AFTER_TYPECHECK_NATIVE_FFI_JAVA_REWRITE_JAVA_IMPL_REFERENCE_WRAPPER_FIELDS
#define CANGJIE_SEMA_AFTER_TYPECHECK_NATIVE_FFI_JAVA_REWRITE_JAVA_IMPL_REFERENCE_WRAPPER_FIELDS

#include "AfterTypeCheckStage.h"
#include "Utils.h"
#include "cangjie/AST/Node.h"
#include <unordered_map>

namespace Cangjie::Native::FFI::Java {
using namespace Interop::Java;

/**
 * Rewrites fields to properties within @JavaImpl reference wrappers.
 * Replaces all reference wrapper field usages to the generated properties accessors.
 */
class RewriteJavaImplReferenceWrapperFields : public AfterTypeCheckStage {
public:
    explicit RewriteJavaImplReferenceWrapperFields(TypeManager& typeManager, Native::FFI::Java::Utils& utils,
        std::function<void(AST::Node&)> desugarPropRef);
protected:
    void Process(AfterTypeCheckContext& ctx) override;
private:
    void RelocateFields(AfterTypeCheckContext& ctx, AST::ClassDecl& refWrapper, AST::ClassDecl& registryCompanion) const;

    void RewriteFieldAccess(AfterTypeCheckContext& ctx, AST::Package& pkg) const;

    void PushProxyProperties() const;

    OwnedPtr<AST::PropDecl> GenerateProxyProperty(AST::VarDecl& userField, AST::VarDecl& regCompanionRefField) const;

    AST::VarDecl& CloneField(AST::VarDecl& sample, AST::ClassDecl& registryCompanion) const;

    /**
     * Cache for generated property mapping.
     * Key: user-declared field (within reference wrapper).
     * Value: compiler-generated proxy property (within reference wrapper too).
     */
    mutable std::unordered_map<Ptr<AST::VarDecl>, OwnedPtr<AST::PropDecl>> generatedProps;

    TypeManager& typeManager;
    Native::FFI::Java::Utils& utils;
    /**
     * Due to re-resolve, early-stage property desugaring should happen again.
     */
    std::function<void(AST::Node&)> desugarPropRef;
};
} // namespace Cangjie::Native::FFI::Java

#endif // CANGJIE_SEMA_AFTER_TYPECHECK_NATIVE_FFI_JAVA_REWRITE_JAVA_IMPL_REFERENCE_WRAPPER_FIELDS
