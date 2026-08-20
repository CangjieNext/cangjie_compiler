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
<<<<<<< HEAD
=======
     * This constructor could be called from cangjie
     * before [ctor]:
     *   init(a1: A, a2: B, ..., an: N) {
     *       <super(...)> // optional
     *       ...body...
     *   }
     * --------------------------------
     * after:
     *   init(a1: A, a2: B, ..., an: N) {
     *       super({
     *           let jniEnv = Java_CFFI_get_env()
     *           Java_CFFI_newGlobalReference(
     *               jniEnv,
     *               Java_CFFI_newJavaObject(jniEnv, typeSignature, "(<argsSignature>)V", [
     *                   Java_CFFI_JavaEntity(a1),
     *                   Java_CFFI_JavaEntity(a2),
     *                   ...,
     *                   Java_CFFI_JavaEntity(an),
     *                   Java_CFFI_JavaEntity(null) // mark argument for generated java constructor
     *                                              // of Type $$NativeConstructorMarker
     *                   ]),
     *               true // isWeak
     *           )
     *       }) // @JavaMirror constructor call
     *       <super(...)> // optional <- will be removed after java code generation
     *       ...body...
     *   }
     */
    void DesugarJavaImplConstructor(FuncDecl& ctor, FuncDecl& parentCtor);

    /**
     * Create native func
     * public @C func ${funcName} (${params}): retTy {
     *     ${nodes}
     * }
     */
    OwnedPtr<FuncDecl> CreateNativeFunc(const std::string& funcName,
        std::vector<OwnedPtr<FuncParam>> params, Ptr<Ty> retTy, std::vector<OwnedPtr<Node>> nodes);
    /**
     * Unwrap a refExpr of native func param into Cangjie type.
     * ref: jobject -> @JavaMirror/@JavaImpl
     */
    OwnedPtr<Expr> UnwrapRefExpr(OwnedPtr<RefExpr> ref, Ptr<Ty> targetTy, const ClassLikeDecl& decl);
    /**
     * Wrap the cangjie expr into native type.
     * expr: @JavaMirror/@JavaImpl -> jobject
     */
    OwnedPtr<Expr> WrapExprWithExceptionHandling(
        std::vector<OwnedPtr<Node>>&& nodes, OwnedPtr<Expr> expr, FuncParam& env, const ClassLikeDecl& decl);
    /**
     * Generate native for arg:
     * public @C func Java_xxx_superC${ctorId}A${argId}P${paramIds} (${usingParams}) {
     *     return withExceptionHandling(env, { =>
     *         let tmp0 = wrap($usingParams)
     *         let tmp1 = ${clonedExpr($arg)}
     *         return unwrap(tmp)
     *     })
     * }
     */
    OwnedPtr<FuncDecl> GenerateNativeFunc4Argument(const FuncArg& arg,
        const std::vector<OwnedPtr<FuncParam>>& params, ClassLikeDecl& decl, size_t ctorId, size_t argId);
    // Desugar super call in constructor
    OwnedPtr<CallExpr> DesugarJavaImplSuperCall(const FuncDecl& ctor, Decl& jniEnvVar);

    /**
     * This contructor could be called from java
     * for constructor [sampleCtor]:
     *   init(a1: A, a2: B, ..., an: N) { // or primary constructor
     *       <super(...)> // optional
     *       ...body...
     *   }
     *
     * the following will be generated:
     *     init(obj: Java_CFFI_JavaEntity, a1: A, a2: B, ..., an: N) {
     *         super(obj)
     *         ...body...
     *     }
     */
    OwnedPtr<FuncDecl> GenerateJavaImplConstructor(FuncDecl& sampleCtor, ClassLikeDecl& parent);

    /**
     * This generated method could be called from java
     * for method [sampleMethod] of class package.I:
     *
     * func foo(a1: A, a2: B, ..., an: N): Ret {
     *     ...body...
     * }
     *
     * the following native function will be generated:
     *
     * @C
     * func java_package_I_fooImpl{mangling}(env: JNIEnv_ptr, _: jobject, self: jlong, a1: A, a2: B, ..., an: N): Ret {
     *     *WrapJavaEntity*(Java_CFFI_getFromRegistry<I>(env, self).foo(a1, a2, ..., an))
     * }
     */

    void PushEnvParams(std::vector<OwnedPtr<FuncParam>>& params, std::string name = "env");

    void PushObjParams(std::vector<OwnedPtr<FuncParam>>& params, std::string name = "obj");

    void PushSelfParams(std::vector<OwnedPtr<FuncParam>>& params, std::string name = "self");

    OwnedPtr<CallExpr> GetFwdClassInstance(OwnedPtr<RefExpr> paramRef, Decl& fwdClassDecl);

    bool FillMethodParamsByArg(std::vector<OwnedPtr<FuncParam>>& params, std::vector<OwnedPtr<FuncArg>>& callArgs,
        FuncDecl& funcDecl, OwnedPtr<FuncParam>& arg, FuncParam& jniEnvPtrParam, Ptr<Ty> actualTy);

    OwnedPtr<Decl> GenerateNativeMethod(FuncDecl& sampleMethod, Decl& decl,
        const GenericConfigInfo* genericConfig = nullptr);

    void GenerateFuncParamsForNativeDeleteCjObject(
        Decl& decl, std::vector<OwnedPtr<FuncParam>>& params, FuncParam*& jniEnv, OwnedPtr<Expr>& selfRef);

    OwnedPtr<Decl> GenerateNativeFuncDeclBylambda(Decl& decl, OwnedPtr<LambdaExpr>& wrappedNodesLambda,
        std::vector<OwnedPtr<FuncParamList>>& paramLists, FuncParam& jniEnvPtrParam, ModalTy retTy,
        std::string funcName);

    OwnedPtr<Decl> GenerateNativeFuncDeclBylambda(OwnedPtr<LambdaExpr>& wrappedNodesLambda,
        std::vector<OwnedPtr<FuncParamList>>& paramLists, FuncParam& jniEnvPtrParam, ModalTy retTy,
        std::string funcName, Ptr<File>& curFile, std::string moduleName, std::string fullPackageName);

    std::string GetJniMethodName(const FuncDecl& method, const std::string* genericActualName = nullptr);

    std::string GetJniTupleItemName(const Ptr<TupleTy>& tupleTy, Package& pkg, size_t index);

    std::string GetJniMethodNameForProp(const PropDecl& propDecl, bool isSet,
        const std::string* genericActualName = nullptr) const;

    std::string GetJniSuperArgFuncName(const ClassLikeDecl& outer, const std::string& id) const;

    std::string GetJniInitCjObjectFuncName(const FuncDecl& ctor, bool isGeneratedCtor,
        const std::string* genericActualName = nullptr);
    std::string GetJniInitCjObjectFuncName(const Ptr<TupleTy>& tupleTy, Package& pkg);

    std::string GetJniInitCjObjectFuncNameForVarDecl(const VarDecl& ctor) const;

    std::string GetJniDeleteCjObjectFuncName(const Decl& decl) const;

    std::string GetJniDetachCjObjectFuncName(const Decl& decl) const;

    /**
     * @C public func Java_<type_signature>_deleteCJObject(env: JNIEnv_ptr, obj: jobject, self: jlong): Unit {
     *     Java_CFFI_deleteCJObject<type>(env, self, { objToDelete: <type> => objToDelete.javaref })
     * }
     */
    OwnedPtr<Decl> GenerateNativeDeleteCjObjectFunc(ClassLikeDecl& javaImpl, VarDecl& javaWeakRefField);

    /**
     * @C public func Java_Vector_deleteCJObject(env: JNIEnv_ptr, clazz: jclass, self: jlong): Unit {
     *      withExceptionHandling(env) {
     *          Java_CFFI_removeFromRegistry(self)
     *     }
     *   }
     */
    OwnedPtr<Decl> GenerateCJMappingNativeDeleteCjObjectFunc(Decl& decl);

    OwnedPtr<Decl> GenerateCJMappingNativeDetachCjObjectFunc(ClassDecl& fwdDecl, ClassDecl& classDecl);

    // A helper function to for the ctor of Enum.
    void GenerateNativeInitCJObjectEnumCtor(AST::EnumDecl& enumDecl);

    /**
     * @C public func Java_<enum_identifier>_initCJObject(env: JNIEnv_ptr, obj: jobject): jlong {
           return withExceptionHandling(env, { =>
               return Java_CFFI_put_to_registry_1(Enum.VarDecl)
           })
     * }
     */
    OwnedPtr<Decl> GenerateNativeInitCjObjectFuncForEnumCtorNoParams(AST::EnumDecl& enumDecl, AST::VarDecl& ctor);

    /**
     * when arg ClassLikeDecl is true(e.g. calss decl)::
     *
     * @C public func Java_<type_signature>_initCJObject{mangled}(env: JNIEnv_ptr, obj: jobject, args...): jlong {
     *     registry.put(
     *         <type>(Java_CFFI_newGlobalReference(env, Java_CFFI_JavaEntity(JOBJECT, obj), true), args...)
     *     )
     * }
     * when arg ClassLikeDecl is false (e.g. struct decl):
     *
     * @C public func Java_<type_signature>_initCJObject{mangled}(env: JNIEnv_ptr, obj: jobject, args...): jlong {
     *     registry.put(
     *         <type>(args...)
     *     )
     * }
     */
    OwnedPtr<Decl> GenerateNativeInitCjObjectFunc(FuncDecl& ctor,
        bool isClassLikeDecl,
        bool isOpenClass = false,
        Ptr<FuncDecl> fwdCtor = nullptr,
        const GenericConfigInfo* genericConfig = nullptr);

    OwnedPtr<Decl> GenerateNativeInitCjObjectFunc(const Ptr<TupleTy>& tuple, Package& pkg);

    /**
>>>>>>> 0c41e6d (feat: add local mode implementation)
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
