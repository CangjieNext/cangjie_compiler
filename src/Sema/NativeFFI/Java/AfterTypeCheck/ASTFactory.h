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
#include "NativeFFI/Java/CachingApi/JFieldIdCache.h"
#include "NativeFFI/Java/CachingApi/JMethodIdCache.h"
#include "NativeFFI/Java/JavaMemberSignature.h"
#include "cangjie/AST/Node.h"

namespace Cangjie::Native::FFI::Java {

class ASTFactory {
public:
    explicit ASTFactory(
        TypeManager& typeManager, InteropLibBridge& ilib, JniBridge& jni,
        JClassCache& jclassCache, JMethodIdCache& jmethodIdCache, JFieldIdCache& jfieldIdCache);


    /**
     * Creates expression with `javaClassTy` (as a mirror/impl type) constructor call via JNI.
     * Returned expr has `JavaEntity` ty.
     * `args` are java-compatible arguments.
     * `constructor` is Cangjie declaration of Java constructor that should be called.
     * For marked constructors (`isMarkedConstructor`) is additionally passes an extra marker as an argument.
     * Includes all required JNI setup (JNIEnv, class, constructor ID,
     * arguments and exception handling), and returns the created object.
     */
    OwnedPtr<AST::Expr> CreateNewJavaObjectCall(AfterTypeCheckContext& ctx,
        Ptr<AST::Expr> jniEnvPtr,
        JavaMemberSignature constructor,
        std::vector<OwnedPtr<AST::Expr>> args) const;

    /*
     * Creates expression of `javaClassTy` (as a mirror/impl type) constructor call via JNI.
     * Returned expr has `JavaEntity` ty.
     * `args` are java-compatible arguments.
     * `constructor` is Cangjie declaration of Java constructor that should be called.
     * For marked constructors (`isMarkedConstructor`) is additionally passes an extra marker as an argument.
     * Includes all required JNI setup (JNIEnv, class, constructor ID,
     * arguments and exception handling), and returns the created object.
     *
     * In comparison with the overloaded version accepting `Ptr<AST::Expr> jniEnvPtr` parameter,
     * this function creates and caches jniEnvPtr by itself.
     */
    OwnedPtr<AST::Expr> CreateNewJavaObjectCall(AfterTypeCheckContext& ctx,
        AST::File& curFile,
        JavaMemberSignature constructor,
        std::vector<OwnedPtr<AST::Expr>> args) const;

    OwnedPtr<AST::Expr> CreateNonvirtualJavaMethodCall(AfterTypeCheckContext& ctx,
        Ptr<AST::Expr> jniEnvPtr, Ptr<AST::Expr> jobjectInstance,
        JavaMemberSignature method, AST::Ty& retTy, std::vector<OwnedPtr<AST::Expr>> javaCompatibleArgs) const;

    OwnedPtr<AST::Expr> CreateNonvirtualJavaMethodCall(AfterTypeCheckContext& ctx,
        AST::File& curFile, Ptr<AST::Expr> jobjectInstance,
        JavaMemberSignature method, AST::Ty& retTy, std::vector<OwnedPtr<AST::Expr>> args) const;

    OwnedPtr<AST::Expr> CreateStaticJavaMethodCall(AfterTypeCheckContext& ctx,
        Ptr<AST::Expr> jniEnvPtr,
        JavaMemberSignature method, AST::Ty& retTy, std::vector<OwnedPtr<AST::Expr>> javaCompatibleArgs) const;

    OwnedPtr<AST::Expr> CreateStaticJavaMethodCall(AfterTypeCheckContext& ctx,
        AST::File& curFile,
        JavaMemberSignature method, AST::Ty& retTy, std::vector<OwnedPtr<AST::Expr>> args) const;

    OwnedPtr<AST::Expr> CreateVirtualJavaMethodCall(AfterTypeCheckContext& ctx,
        Ptr<AST::Expr> jniEnvPtr, Ptr<AST::Expr> jobjectInstance,
        JavaMemberSignature method, AST::Ty& retTy, std::vector<OwnedPtr<AST::Expr>> javaCompatibleArgs) const;

    OwnedPtr<AST::Expr> CreateVirtualJavaMethodCall(AfterTypeCheckContext& ctx,
        AST::File& curFile, Ptr<AST::Expr> jobjectInstance,
        JavaMemberSignature method, AST::Ty& retTy, std::vector<OwnedPtr<AST::Expr>> args) const;
    
    
    OwnedPtr<AST::Expr> CreateInstanceJavaFieldGetCall(AfterTypeCheckContext& ctx,
        AST::File& curFile, Ptr<AST::Expr> jobjectInstance,
        JavaMemberSignature field, AST::Ty& ty) const;
    
    OwnedPtr<AST::Expr> CreateStaticJavaFieldGetCall(AfterTypeCheckContext& ctx,
        AST::File& curFile,
        JavaMemberSignature field, AST::Ty& ty) const;
    
    OwnedPtr<AST::Expr> CreateInstanceJavaFieldSetCall(AfterTypeCheckContext& ctx,
        AST::File& curFile, Ptr<AST::Expr> jobjectInstance,
        JavaMemberSignature field, AST::Ty& ty, OwnedPtr<AST::Expr> value) const;
    
    OwnedPtr<AST::Expr> CreateStaticJavaFieldSetCall(AfterTypeCheckContext& ctx,
        AST::File& curFile,
        JavaMemberSignature field, AST::Ty& ty, OwnedPtr<AST::Expr> value) const;

    /**
     * Creates usages as RefExpr over parameters in `paramList`.
     */
    std::vector<OwnedPtr<AST::Expr>> CreateParamsUsage(const AST::FuncParamList& paramList) const;

    std::vector<OwnedPtr<AST::Expr>> ExtractArgExprs(const std::vector<OwnedPtr<FuncArg>>& args) const;

    /**
     * AST Block builder. Inserts JNI `handlePendingException` after builder execution
     * and returns user value saved into variable.
     */
    OwnedPtr<AST::Expr> CreateBlockWithJniHandlePendingException(Ptr<AST::Expr> jniEnvPtr,
        std::function<void(AST::Block&)> builder) const;
    
    OwnedPtr<AST::Expr> CreateBlockWithJniHandlePendingException(Ptr<AST::Expr> jniEnvPtr,
        std::vector<OwnedPtr<Node>> nodes) const;

private:

    OwnedPtr<AST::Expr> CreateJavaMethodCall(AfterTypeCheckContext& ctx,
        Ptr<AST::Expr> jniEnvPtr, Ptr<AST::Expr> jobjectInstance,
        JavaMemberSignature method, AST::Ty& retTy, std::vector<OwnedPtr<AST::Expr>> javaCompatibleArgs,
        bool isVirtual) const;

    OwnedPtr<AST::Expr> CreateJavaMethodCall(AfterTypeCheckContext& ctx,
        AST::File& curFile, Ptr<AST::Expr> jobjectInstance,
        JavaMemberSignature method, AST::Ty& retTy, std::vector<OwnedPtr<AST::Expr>> args, bool isVirtual) const;
    
    OwnedPtr<AST::Expr> CreateJavaFieldGetCall(AfterTypeCheckContext& ctx,
        Ptr<AST::Expr> jniEnvPtr, Ptr<AST::Expr> jobjectInstance,
        JavaMemberSignature field, AST::Ty& ty) const;
    
    OwnedPtr<AST::Expr> CreateJavaFieldGetCall(AfterTypeCheckContext& ctx,
        AST::File& curFile, Ptr<AST::Expr> jobjectInstance,
        JavaMemberSignature field, AST::Ty& ty) const;

    OwnedPtr<AST::Expr> CreateJavaFieldSetCall(AfterTypeCheckContext& ctx,
        Ptr<AST::Expr> jniEnvPtr, Ptr<AST::Expr> jobjectInstance,
        JavaMemberSignature field, AST::Ty& ty, OwnedPtr<AST::Expr> value) const;

    OwnedPtr<AST::Expr> CreateJavaFieldSetCall(AfterTypeCheckContext& ctx,
        AST::File& curFile, Ptr<AST::Expr> jobjectInstance,
        JavaMemberSignature field, AST::Ty& ty, OwnedPtr<AST::Expr> value) const;
    
    /**
     * Creates tmp variable of VArray type by `args`.
     */
    OwnedPtr<AST::VarDecl> CreateTmpVArrayVarDecl(std::vector<OwnedPtr<AST::Expr>> args, AST::File& curFile) const;

    /**
     * Converts `values` expressions as JValue expressions in-place.
     */
    std::vector<OwnedPtr<AST::Expr>> ConvertToJValues(std::vector<OwnedPtr<AST::Expr>> values) const;

    TypeManager& typeManager;
    Interop::Java::InteropLibBridge& ilib;
    JniBridge& jni;
    JClassCache& jclassCache;
    JMethodIdCache& jmethodIdCache;
    JFieldIdCache& jfieldIdCache;
};

}

#endif // CANGJIE_SEMA_NATIVE_FFI_JAVA_AFTER_TYPECHECK_AST_FACTORY
