// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares java JNI bridging mechanisms.
 */
#ifndef CANGJIE_SEMA_NATIVE_FFI_JAVA_JNI_BRIDGE
#define CANGJIE_SEMA_NATIVE_FFI_JAVA_JNI_BRIDGE

#include "NativeFFI/Java/AfterTypeCheck/Utils.h"
#include "cangjie/AST/Node.h"
#include "cangjie/Mangle/BaseMangler.h"
#include "cangjie/Sema/TypeManager.h"

namespace Cangjie::Native::FFI::Java {

class JniBridge final {
public:
    explicit JniBridge(
        TypeManager& typeManager,
        const BaseMangler& mangler,
        Interop::Java::Utils& utils,
        AST::Decl& jniEnvPtrDecl,
        AST::Decl& jniJobjectDecl);

    /**
     * $jnienv: JNIEnv_Ptr
     */
    OwnedPtr<AST::FuncParam> CreateJniEnvParam(const std::string& name = "$jnienv") const;

    /**
     * $obj: jobject or jclass.
     */
    OwnedPtr<AST::FuncParam> CreateJniJobjectOrJclassParam(const std::string& name = "$obj") const;

    /**
     * $regId: jlong
     */
    OwnedPtr<AST::FuncParam> CreateRegistryIdParam(const std::string& name = "$regId") const;

    /**
     * Creates @C function with name `name`, return type `retTy` within `curFile` at `fullPackageName` at `moduleName`.
     * Appends `userParams` to native function parameters to comply with JNI ABI.
     * Parameters order of @C function: [jniEnv, objOrClass, <userParams>].
     * jniEnv and objOrClass params are inserted automatically with references back provided.
     */
    OwnedPtr<AST::FuncDecl> CreateNativeJavaABIFunc(
        const std::string& name,
        std::vector<OwnedPtr<AST::FuncParam>> userParams,
        Ptr<AST::Ty> retTy,
        AST::File& curFile,
        std::string& moduleName,
        std::string& fullPackageName,
        std::function<void(
            AST::FuncDecl& f,
            AST::FuncParam& jniEnv,
            AST::FuncParam& objOrClass,
            std::vector<Ptr<AST::FuncParam>> userParams)> builder) const;

    std::string GetJniMethodName(const AST::FuncDecl& method, const std::string* genericActualName = nullptr) const;

    std::string GetJniTupleItemName(const Ptr<AST::TupleTy>& tupleTy, AST::Package& pkg, size_t index) const;

    std::string GetJniMethodNameForProp(const AST::PropDecl& propDecl, bool isSet,
        const std::string* genericActualName = nullptr) const;

    std::string GetJniInitCjObjectFuncName(const AST::FuncDecl& ctor, bool isGeneratedCtor,
        const std::string* genericActualName = nullptr) const;
    std::string GetJniInitCjObjectFuncName(const Ptr<AST::TupleTy>& tupleTy, AST::Package& pkg) const;

    std::string GetJniInitCjObjectFuncNameForVarDecl(const AST::VarDecl& ctor) const;

    std::string GetJniDeleteCjObjectFuncName(const AST::Decl& decl) const;

    std::string GetJniDetachCjObjectFuncName(const AST::Decl& decl) const;

    std::string GetLambdaCallImplJniMethodName(const AST::Decl& decl) const;

    std::string GetJavaNativeFunctionName(const std::string& fqTypeName, const std::string& memberName) const;

    /**
     * For CType ty, ty is returned. For mirrors, impls and CJMapping JNI jobject is returned
     */
    AST::Ty& ConvertCangjieToJniTy(AST::Ty& javaCompatibleTy) const;
private:
    AST::Ty& GetJniEnvPtrTy() const;
    AST::Ty& GetJniJobjectDeclTy() const;
    AST::Ty& GetRegistryIdJavaTy() const;

    TypeManager& typeManager;
    const BaseMangler& mangler;
    Interop::Java::Utils& utils;

    AST::Decl& jniEnvPtrDecl;
    AST::Decl& jniJobjectDecl;
};

} // namespace Cangjie::Native::FFI::Java

#endif // CANGJIE_SEMA_NATIVE_FFI_JAVA_JNI_BRIDGE
