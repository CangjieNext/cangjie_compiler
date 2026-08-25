// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares after-typecheck Java interop stage: filling of the bodies with implementation for
 * the mirrored members.
 *
 */
#ifndef CANGJIE_SEMA_AFTER_TYPECHECK_NATIVE_FFI_JAVA_POPULATE_JAVA_MIRROR_STUBS
#define CANGJIE_SEMA_AFTER_TYPECHECK_NATIVE_FFI_JAVA_POPULATE_JAVA_MIRROR_STUBS

#include "AfterTypeCheckStage.h"
#include "InteropLibBridge.h"
#include "NativeFFI/Java/AfterTypeCheck/ASTFactory.h"
#include "NativeFFI/Java/AfterTypeCheck/AfterTypeCheckContext.h"
#include "cangjie/AST/Node.h"
#include "cangjie/AST/Types.h"

namespace Cangjie::Native::FFI::Java {
using namespace Interop::Java;

/**
 * Generates required declarations within @JavaImpl registry companion classes scope.
 */
class PopulateJavaMirrorStubs : public AfterTypeCheckStage {
public:
    explicit PopulateJavaMirrorStubs(TypeManager& typeManager, InteropLibBridge& ilib, ASTFactory& factory);

protected:
    void Process(AfterTypeCheckContext& ctx) override;
private:
    void Process(AfterTypeCheckContext& ctx, AST::ClassLikeDecl& mirror) const;

    /**
    * Generates a body for Java mirror constructor to initialize the generated wrapper
    * with a Java object created via JNI.
    *
    * before:
    *   init(a1: A, ..., an: N) {
    *   }
    *
    * after:
    *   init(a1: A, ..., an: N) {
    *       this({
    *           // Create Java object via JNI.
    *           ...
    *       })
    *   }
   */
    void PopulateUserConstructor(AfterTypeCheckContext& ctx,
        AST::FuncDecl& userCtor, AST::FuncDecl& wrappingCtor) const;

    /**
     * for func [fun]:
     *     func foo(args): Ret
     *
     * the following will be generated:
     *     func foo(args): Ret {
     *         *UnwrapJavaEntity*(
     *             Java_CFFI_callMethod_raw(
     *                 Java_CFFI_get_env(),
     *                 this.javaref, // or getJavaref if mirror is an interface
     *                 typeSignature, "foo", "(<argsSignature>)Ret",
     *                 [Java_CFFI_JavaEntity(args[0]), ... Java_CFFI_JavaEntity(args[n])]
     *         )
     *     }
     *
     * where *UnwrapJavaEntity* - generated unwrapper for Ret type value.
     */
    void PopulateUserMethod(AfterTypeCheckContext& ctx, AST::ClassLikeDecl& mirror, AST::FuncDecl& userMethod) const;

    void PopulateJArrayMethod(AST::ClassDecl& jarray, AST::FuncDecl& userMethod) const;

    /**
     * for prop [prop]:
     *   mut prop p: Ret
     *
     * the following will be generated:
     *     mut prop p: Ret {
     *         get() {
     *             *UnwrapJavaEntity*(
     *                 Java_CFFI_getField_raw(
     *                     Java_CFFI_get_env(), this.javaref, typeSignature, "p", "Ret"
     *             ))
     *         }
     *         set(v) {
     *             Java_CFFI_setField_raw(
     *                 Java_CFFI_get_env(),
     *                 this.javaref, typeSignature, "p", "Ret", Java_CFFI_JavaEntity(v)
     *             )
     *         }
     *     }
     */
    void PopulateUserProperty(AfterTypeCheckContext& ctx, AST::ClassLikeDecl& mirror, AST::PropDecl& userProp) const;

    /**
     * @see `PopulateUserProperty`
     */
    void InsertPropGetter(AfterTypeCheckContext& ctx, AST::ClassLikeDecl& mirror, AST::PropDecl& prop) const;

    /**
     * @see `PopulateUserProperty`
     */
    void InsertPropSetter(AfterTypeCheckContext& ctx, AST::ClassLikeDecl& mirror, AST::PropDecl& prop) const;

    TypeManager& typeManager;
    InteropLibBridge& ilib;
    ASTFactory& factory;
};
} // namespace Cangjie::Native::FFI::Java

#endif // CANGJIE_SEMA_AFTER_TYPECHECK_NATIVE_FFI_JAVA_POPULATE_JAVA_MIRROR_STUBS
