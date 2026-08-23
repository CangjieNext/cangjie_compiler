// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares core support for java mirror and mirror subtype
 */
#ifndef CANGJIE_SEMA_NATIVE_FFI_JAVA_DESUGAR_MANAGER
#define CANGJIE_SEMA_NATIVE_FFI_JAVA_DESUGAR_MANAGER

#include "AfterTypeCheckStage.h"
#include "InteropLibBridge.h"
#include "NativeFFI/Java/AfterTypeCheck/AfterTypeCheckContext.h"
#include "NativeFFI/Java/AfterTypeCheck/JniBridge.h"
#include "Utils.h"

#include "cangjie/AST/Node.h"
#include "cangjie/Mangle/BaseMangler.h"
#include "cangjie/Modules/ImportManager.h"
#include "cangjie/Sema/TypeManager.h"
#include "InheritanceChecker/MemberSignature.h"
#include "cangjie/Utils/SafePointer.h"
#include <unordered_map>

namespace Cangjie::Interop::Java {
using namespace AST;
using namespace std;
using namespace Native::FFI::Java;

const std::string JAVA_ARRAY_GET_FOR_REF_TYPES = "$javaarrayget";
const std::string JAVA_ARRAY_SET_FOR_REF_TYPES = "$javaarrayset";
const std::string JAVA_IMPL_ENTITY_ARG_NAME_IN_GENERATED_CTOR = "$obj";

class JavaDesugarManager {
public:
    JavaDesugarManager(ImportManager& importManager, TypeManager& typeManager, DiagnosticEngine& diag,
        const BaseMangler& mangler, const std::optional<std::string>& javaCodeGenPath, const std::string& outputLibPath,
        const std::unordered_map<Ptr<const InheritableDecl>, MemberMap>& memberMap)
        : importManager(importManager),
          typeManager(typeManager),
          utils(importManager, typeManager),
          diag(diag),
          mangler(mangler),
          lib(importManager, typeManager, diag, utils),
          jniBridge(typeManager, mangler, utils, *lib.GetJniEnvPtrDecl(), *lib.GetJobjectDecl()),
          javaCodeGenPath(javaCodeGenPath),
          outputLibPath(outputLibPath),
          memberMap(memberMap)
    {
        lib.CheckInteropLibVersion();
    }

    /**
     * Constructors generation and javaref field insertion.
     * The first step: generate members in `JObject` and insert empty constructor in other mirrors. ([doStub] = `false`)
     * The second step: fill pregenerated bodies ([doStub] = `false`)
     */
    void GenerateInMirrors(File& file, bool doStub);

    void GenerateInMirror(ClassDecl& classDecl, bool doStub);

    void GenerateInSynthetic(ClassDecl& cd);

    /**
     * Desugar constructors, methods, etc
     */
    void DesugarMirrors(File& file);

    void GenerateJavaSourceCode(AfterTypeCheckContext& ctx);

    void DesugarJavaMirror(ClassDecl& mirror);
    void DesugarJavaMirror(InterfaceDecl& mirror);
    void ProcessJavaMirrorImplStages(AfterTypeCheckContext& ctx, std::function<void(AST::Node&)> desugarPropRef);
private:
    /**
     * Processes logically isolated interop stage.
     */
    template <typename S, class... StageArgs>
    std::enable_if_t<std::is_base_of_v<AfterTypeCheckStage, S>, void>
    Process(AfterTypeCheckContext& ctx, StageArgs&&... args) const
    {
        S stage(std::forward<StageArgs>(args)...);
        stage(ctx);
    }

    /**
     * Inserts javaref decl to the class decl:
     *
     * let javaref: Java_CFFI_JavaEntity
     */
    void InsertJavaRefVarDecl(ClassDecl& decl);

    void InsertArrayJavaEntityGet(ClassDecl& decl);
    void InsertArrayJavaEntitySet(ClassDecl& decl);

    /**
     * Generates and inserts constructor in java mirror:
     *
     * public init($ref: Java_CFFI_JavaEntity) {
     *     this.javaref = ref // for JObject
     *     // super($ref) // for other mirrors
     * }
     */
    void InsertJavaMirrorCtor(ClassDecl& decl, bool doStub);

    /**
     * var $hasInited: Bool = false
     */
    void InsertJavaMirrorHasInited(ClassDecl& mirror);

    /**
     * ~init() {
     *     Java_CFFI_deleteGlobalReference($jnienv, this.javaref)
     * }
     */
    void InsertJavaMirrorFinalizer(ClassDecl& mirror);

    /**
     * Generates and inserts javaref getter as javaref field will be in synthetic class
     * that implements current interface.
     *
     * abstract getter
     * public func $getJavaRef(): Java_CFFI_JavaEntity
     */
    void InsertAbstractJavaRefGetter(ClassLikeDecl& decl);

    /**
     * public override func $getJavaRef(): Java_CFFI_JavaEntity {
     *     return $javaref
     * }
     */
    void InsertJavaRefGetterWithBody(ClassDecl& decl);

     /**
     * Rewrites a Java mirror constructor to initialize the generated wrapper
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
    void DesugarJavaMirrorConstructor(FuncDecl& ctor, FuncDecl& generatedCtor);

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
    void DesugarJavaMirrorMethod(FuncDecl& fun, ClassLikeDecl& mirror);

    /**
     * used in DesugarJavaMirrorMethod for method's body generation
     *
     */
    void AddJavaMirrorMethodBody(ClassLikeDecl& mirror,
        FuncDecl& fun,
        OwnedPtr<Expr> javaRefCall);

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
    void DesugarJavaMirrorProp(PropDecl& prop);

    void InsertJavaMirrorPropGetter(PropDecl& prop);
    void InsertJavaMirrorPropSetter(PropDecl& prop);

    /**
     * Inserts constructor of form `JString(String)`.
     * The operation consists of two steps:
     * 1) Insert constructor stub (constructor with empty body): [doStub] = `true`
     * 2) Fills generated constructor with actual body: [doStub] = `false`
     *
     * public init(s: String) {
     *   super(Java_CFFI_CangjieStringToJava(env, s))
     * }
     */
    void InsertJStringOfStringCtor(ClassDecl& decl, bool doStub);

    void GenerateNativeItemFunc(AfterTypeCheckContext& ctx, const Ptr<TupleTy>& tupleTy);

    ImportManager& importManager;
    TypeManager& typeManager;
    Utils utils;
    DiagnosticEngine& diag;
    const BaseMangler& mangler;
    InteropLibBridge lib;
    JniBridge jniBridge;
    const std::optional<std::string>& javaCodeGenPath;
    const std::string& outputLibPath;

    // contains the member signatures of structs.
    const std::unordered_map<Ptr<const AST::InheritableDecl>, MemberMap>& memberMap;
};

} // namespace Cangjie::Interop::Java

#endif // CANGJIE_SEMA_NATIVE_FFI_JAVA_DESUGAR_MANAGER
