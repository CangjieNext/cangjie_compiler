// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares after-typecheck Java interop stage: generation of required declarations
 * within @JavaImpl registry companion classes.
 *
 */
#ifndef CANGJIE_SEMA_AFTER_TYPECHECK_NATIVE_FFI_JAVA_GENERATE_IN_JAVA_IMPL_REGISTRY_COMPANIONS
#define CANGJIE_SEMA_AFTER_TYPECHECK_NATIVE_FFI_JAVA_GENERATE_IN_JAVA_IMPL_REGISTRY_COMPANIONS

#include "AfterTypeCheckStage.h"
#include "InteropLibBridge.h"
#include "cangjie/AST/Node.h"
#include "cangjie/AST/Types.h"
#include "cangjie/Sema/TypeManager.h"

namespace Cangjie::Native::FFI::Java {
using namespace Interop::Java;

/**
 * Generates required declarations within @JavaImpl registry companion classes scope.
 */
class GenerateInJavaImplRegistryCompanion : public AfterTypeCheckStage {
public:
    explicit GenerateInJavaImplRegistryCompanion(TypeManager& typeManager, InteropLibBridge& ilib);
protected:
    void Process(AfterTypeCheckContext& ctx) override;
private:
    void Process(AST::ClassDecl& companion) const;

    /**
    * Generates constructor for @JavaImpl registry companion class.
    * This constructor is used for initializing cangjie-side @JavaImpl object right after java-side object is created.
    * Actual object initialization happens inside corresponding reference wrapper object.
    */
    OwnedPtr<AST::FuncDecl> GenerateConstructor(AST::ClassDecl& companion) const;

    TypeManager& typeManager;
    InteropLibBridge& ilib;
};
} // namespace Cangjie::Native::FFI::Java

#endif // CANGJIE_SEMA_AFTER_TYPECHECK_NATIVE_FFI_JAVA_GENERATE_IN_JAVA_IMPL_REGISTRY_COMPANIONS
