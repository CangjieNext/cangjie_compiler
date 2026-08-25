// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares after-typecheck Java interop stage: desugaring super method calls for java impls.
 */
#ifndef CANGJIE_SEMA_AFTER_TYPECHECK_NATIVE_FFI_JAVA_DESUGAR_JAVA_IMPL_SUPER_METHOD_CALL
#define CANGJIE_SEMA_AFTER_TYPECHECK_NATIVE_FFI_JAVA_DESUGAR_JAVA_IMPL_SUPER_METHOD_CALL

#include "AfterTypeCheckStage.h"
#include "InteropLibBridge.h"
#include "NativeFFI/Java/AfterTypeCheck/ASTFactory.h"
#include "NativeFFI/Java/AfterTypeCheck/AfterTypeCheckContext.h"
#include "cangjie/AST/Node.h"

namespace Cangjie::Native::FFI::Java {
using namespace Interop::Java;

/**
 * Super calls to methods in @JavaImpl reference wrappers are desugared at this stage.
 * Without desugaring, call `super.foo()` targets `foo` method in its parent (mirror).
 * In mirror, java methods are called virtually by default.
 */
class DesugarJavaImplSuperMethodCall : public AfterTypeCheckStage {
public:
    explicit DesugarJavaImplSuperMethodCall(
        InteropLibBridge& ilib, ASTFactory& factory);

protected:
    void Process(AfterTypeCheckContext& ctx) override;
private:
    void DesugarSuperMethodCall(AfterTypeCheckContext& ctx, AST::CallExpr& call, AST::ClassDecl& impl) const;

    InteropLibBridge& ilib;
    ASTFactory& factory;
};

}

#endif // CANGJIE_SEMA_AFTER_TYPECHECK_NATIVE_FFI_JAVA_DESUGAR_JAVA_IMPL_SUPER_METHOD_CALL
