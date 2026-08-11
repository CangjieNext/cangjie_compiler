// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_SEMA_NATIVE_FFI_JAVA_AFTER_TYPECHECK_AST_FACTORY
#define CANGJIE_SEMA_NATIVE_FFI_JAVA_AFTER_TYPECHECK_AST_FACTORY

#include "NativeFFI/Java/AfterTypeCheck/AfterTypeCheckContext.h"
#include "NativeFFI/Java/AfterTypeCheck/InteropLibBridge.h"
#include "NativeFFI/Java/AfterTypeCheck/JniBridge.h"
#include "NativeFFI/Java/CachingApi/JClassCache.h"
#include "NativeFFI/Java/CachingApi/JMethodIdCache.h"
#include "cangjie/AST/Node.h"

namespace Cangjie::Native::FFI::Java {

class ASTFactory {
    explicit ASTFactory();


    /**
     * Creates expression with `javaClassTy` (as a mirror/impl type) constructor call via JNI.
     * Returned expr has `JavaEntity` ty.
     * `args` are java-compatible arguments.
     * `constructor` is Cangjie declaration of Java constructor that should be called.
     * For marked constructors (`isMarkedConstructor`) is additionally passes an extra marker as an argument.
     */
    OwnedPtr<AST::Expr> CreateNewJavaObjectCall(
        AfterTypeCheckContext& ctx,
        Ptr<AST::Expr> jniEnv,
        AST::Ty& javaClassTy,
        AST::FuncDecl& constructor,
        std::vector<OwnedPtr<AST::Expr>> args) const;

private:

    /**
     * Creates tmp variable of VArray type by `args`.
     */
    OwnedPtr<AST::VarDecl> CreateTmpVArrayVarDecl(std::vector<OwnedPtr<AST::Expr>> args, AST::File& curFile) const;

    /**
     * Creates usages as RefExpr over parameters in `paramList`.
     */
    std::vector<OwnedPtr<AST::Expr>> CreateParamsUsage(const AST::FuncParamList& paramList) const;

    /**
     * Converts `values` expressions as JValue expressions in-place.
     */
    std::vector<OwnedPtr<AST::Expr>> ConvertToJValues(std::vector<OwnedPtr<AST::Expr>> values) const;

    TypeManager& typeManager;
    Interop::Java::InteropLibBridge& ilib;
    JniBridge& jni;
    JClassCache& jclassCache;
    JMethodIdCache& jmethodIdCache;
};

}

#endif // CANGJIE_SEMA_NATIVE_FFI_JAVA_AFTER_TYPECHECK_AST_FACTORY
