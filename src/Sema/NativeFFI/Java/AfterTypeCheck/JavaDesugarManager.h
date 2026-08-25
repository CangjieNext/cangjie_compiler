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
#include "NativeFFI/Java/AfterTypeCheck/ASTFactory.h"
#include "NativeFFI/Java/AfterTypeCheck/AfterTypeCheckContext.h"
#include "NativeFFI/Java/AfterTypeCheck/JniBridge.h"
#include "NativeFFI/Java/CachingApi/JClassCache.h"
#include "NativeFFI/Java/CachingApi/JFieldIdCache.h"
#include "NativeFFI/Java/CachingApi/JMethodIdCache.h"
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
          jniBridge(typeManager, importManager, mangler, utils,
              *lib.GetJniEnvPtrDecl(),
              *lib.GetJobjectDecl(),
              *lib.GetJNINativeInterfaceDecl()),
          jclassCache(importManager, typeManager, lib, jniBridge),
          jmethodIdCache(importManager, typeManager, lib),
          jfieldIdCache(importManager, typeManager, lib),
          factory(typeManager, lib, jniBridge, jclassCache, jmethodIdCache, jfieldIdCache),
          javaCodeGenPath(javaCodeGenPath),
          outputLibPath(outputLibPath),
          memberMap(memberMap)
    {
        lib.CheckInteropLibVersion();
    }

    /**
     * Fills bodies for previously generated stubs.
     */
    void GenerateInMirrors(File& file);

    void GenerateInMirror(ClassDecl& classDecl);

    void GenerateJavaSourceCode(AfterTypeCheckContext& ctx);

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
     * Generates and inserts constructor body in java mirror (other than JObject):
     *
     * public init($ref: Java_CFFI_JavaEntity) {
     *     super($ref)
     * }
     */
    void InsertJavaMirrorWrappingConstructorBody(ClassDecl& decl);

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
     * public override func $getJavaRef(): Java_CFFI_JavaEntity {
     *     return $javaref
     * }
     */
    void InsertJavaRefGetterWithBody(ClassDecl& decl);

    /**
     * Inserts constructor body for JString of form `JString(String)`.
     * The operation consists of:
     * - Fills generated constructor with actual body.
     *
     * public init(s: String) {
     *   super(Java_CFFI_CangjieStringToJava(env, s))
     * }
     */
    void InsertJStringOfStringCtorBody(ClassDecl& decl);

    void GenerateNativeItemFunc(AfterTypeCheckContext& ctx, const Ptr<TupleTy>& tupleTy);

    ImportManager& importManager;
    TypeManager& typeManager;
    Utils utils;
    DiagnosticEngine& diag;
    const BaseMangler& mangler;
    InteropLibBridge lib;
    JniBridge jniBridge;
    JClassCache jclassCache;
    JMethodIdCache jmethodIdCache;
    JFieldIdCache jfieldIdCache;
    ASTFactory factory;

    const std::optional<std::string>& javaCodeGenPath;
    const std::string& outputLibPath;

    // contains the member signatures of structs.
    const std::unordered_map<Ptr<const AST::InheritableDecl>, MemberMap>& memberMap;
};

} // namespace Cangjie::Interop::Java

#endif // CANGJIE_SEMA_NATIVE_FFI_JAVA_DESUGAR_MANAGER
