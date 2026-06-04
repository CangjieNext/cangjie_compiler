// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares class for Objective-C code generation.
 */

#ifndef CANGJIE_SEMA_OBJC_GENERATOR_H
#define CANGJIE_SEMA_OBJC_GENERATOR_H

#include <fstream>
#include <string_view>
#include <unordered_set>

#include "NativeFFI/ObjC/AfterTypeCheck/Interop/Context.h"
#include "NativeFFI/ObjC/Utils/Handler.h"
#include "cangjie/AST/Match.h"
#include "cangjie/AST/Types.h"
#include "ObjCParamMapper.h"
#include "Utils.h"

namespace Cangjie::Interop::ObjC {

struct EmittableObjCFuncMetainfo {
    std::string identifier;
    std::string mangledIdentifier;
    std::vector<std::string> selectorComponents;
    bool isStatic;
    bool bridge;
    std::string paramsDecl;
    std::string paramStaticRef;
    std::string callingParams;
    std::string convertedParams;
    std::string retType;
};

struct EmittableObjCClassMetainfo {
    std::string className;
    std::string superclassName;
    std::vector<std::string> interfaces;
    bool isFinal;
};

struct EmittableObjCPropMetainfo {
    std::string name;
    std::string type;
    bool isStatic;
    bool isReadwrite;
    struct EmittableObjCFuncMetainfo getter;
    struct EmittableObjCFuncMetainfo setter;
};

class Emitter {
public:
    inline static const std::string FOUNDATION_IMPORT = "<Foundation/Foundation.h>";
    inline static const std::string STDDEF_IMPORT = "<stddef.h>";
    inline static const std::string STDLIB_IMPORT = "<stdlib.h>";

    inline static const std::string VOID_TYPE = "void";
    inline static const std::string VOID_POINTER_TYPE = "void*";
    inline static const std::string ID_TYPE = "id";
    inline static const std::string UNSUPPORTED_TYPE = "UNSUPPORTED_TYPE";
    inline static const std::string INT64_T = "int64_t";

    inline static const std::string CAST_TO_VOID_PTR = "(__bridge void*)";
    inline static const std::string CAST_TO_VOID_PTR_RETAINED = "(__bridge_retained void*)";
    inline static const std::string CAST_TO_VOID_PTR_UNSAFE = "(void*)";

    inline static const std::string SELF_WEAKLINK_NAME = "$registryId";
    inline static const std::string SELF_NAME = "self";

    inline static const std::string DEFAULT_OUTPUT_DIR = "objc-gen";
    inline static const std::string DEFAULT_OUTPUT_DIR_WARNING =
        "default value objc-gen of Objective-C generated sources directory will be changed in next Cangjie SDK release";


    // #import "identifier.h"
    static void EmitFileImport(std::stringstream *stream, const std::string& identifier);
    // #import <identifier>
    static void EmitModuleImport(std::stringstream *s, const std::string& identifier);

    // typedef type(*identifier);
    static void EmitTypedef(std::stringstream *stream, std::string& identifier);
    // @class identifier;
    static void EmitForwardDeclaration(std::stringstream *stream, const std::string& Identifier);
    // @protocol identifier;
    static void EmitForwardDeclarationProtocol(std::stringstream *stream, const std::string& Identifier);

    // @interface A : M <Proto1, Proto2>
    static void EmitInterfaceDeclaration(std::stringstream *stream, struct EmittableObjCClassMetainfo info);
    // @implementation A
    static void EmitImplementationDeclaration(std::stringstream *stream, struct EmittableObjCClassMetainfo info);

    // - (int64_t)mirror2WithArg0:(Box*)arg0 withArg1:(Box*)arg1;
    static void EmitObjCFuncHeaderFunctionDecl(std::stringstream *stream, struct EmittableObjCFuncMetainfo ctor);
    // static int64_t (*CJImpl_ObjC_cjworld_Derived_mirror1WithArg0_CN7cjworld3BoxE)(int64_t,void*) = NULL;
    static void EmitObjCFuncStaticReference(std::stringstream *stream, struct EmittableObjCFuncMetainfo info);

    static void EmitObjCFuncInitialization(std::stringstream *stream, struct EmittableObjCFuncMetainfo info);

    static void EmitSyntheticHeaderFunctionDecls(std::stringstream *stream);

    static void EmitSyntheticRefsDealloc(std::stringstream *stream);

    static void EmitInitialize(std::stringstream *stream, std::string cjLibraryPath, struct EmittableObjCClassMetainfo info);

    static void EmitCtor(std::stringstream *s, struct EmittableObjCFuncMetainfo ctor, struct EmittableObjCClassMetainfo cls);

    static void EmitDoesNotRecognizeSelectorCtor(std::stringstream *s, struct EmittableObjCFuncMetainfo ctor);

    static void EmitDefaultFunctionImplementation(std::stringstream *s, struct EmittableObjCFuncMetainfo func);

    static void EmitAdditionalStaticReferences(std::stringstream* s);

    static void EmitDeleteCJObject(std::stringstream *s, std::string mangledName);

    static void EmitDeleteCJObjectInitialization(std::stringstream *s, std::string mangledName);

    static void EmitDealloc(std::stringstream *s);

    static void EmitProperty(std::stringstream *s, struct EmittableObjCPropMetainfo prop);

    static void EmitEnd(std::stringstream *s);
};

using ArgsList = std::vector<std::pair<std::string, std::string>>;

class ObjCGenerator {
public:
    ObjCGenerator(InteropContext& ctx, Ptr<AST::Decl> declArg, const std::string& outputFilePath,
        const std::string& cjLibOutputPath, InteropType interopType);
    ObjCGenerator(InteropContext& ctx, Ptr<AST::Decl> declArg, const std::string& outputFilePath,
        const std::string& cjLibOutputPath, InteropType interopType, Native::FFI::GenericConfigInfo* genericConfig,
        bool isGenericGlueCode);
    void Generate();

private:
    struct EmittableObjCClassMetainfo GetObjCClassMetainfo(AST::ClassDecl* classDecl);
    struct EmittableObjCFuncMetainfo GetObjCFuncMetainfo(AST::FuncDecl& funcDecl, bool mangled = false);
    struct EmittableObjCFuncMetainfo GetObjCCtorMetainfo(AST::FuncDecl& funcDecl);
    struct EmittableObjCPropMetainfo GetObjCPropMetainfoFromProp(AST::PropDecl& varDecl);
    struct EmittableObjCPropMetainfo GetObjCPropMetainfoFromVar(AST::VarDecl& varDecl);

    bool CheckFunction(OwnedPtr<AST::Decl>& arg);
    bool CheckCtor(OwnedPtr<AST::Decl>& arg);
    bool CheckProp(OwnedPtr<AST::Decl>& arg);
    void CollectDependencies(Ptr<AST::Ty> ty);

    std::stringstream tmpSource;
    std::stringstream tmpHeader;
    std::stringstream dependenciesStream;
    std::stringstream importSource;
    std::stringstream importHeader;
    std::stringstream staticReferences;
    std::stringstream funcInitialization;
    std::stringstream initialize;
    std::stringstream ctors;
    std::stringstream defaultFuncImpls;

    std::set<Ptr<AST::Decl>> dependencies;

    std::string resPreamble;
    std::string res;
    std::string resSource;
    const std::string& outputFilePath;
    const std::string& cjLibOutputPath;
    size_t currentBlockIndent = 0;
    Ptr<AST::Decl> decl;
    InteropContext& ctx;
    InteropType interopType;
    ObjCParamMapper paramMapper;
    
    std::stringstream buffer;
    Native::FFI::GenericConfigInfo* genericConfig = nullptr;
    bool isGenericGlueCode{false};

    bool SkipSetterForValueTypeDecl(AST::Decl& declArg) const;
    bool IsNotThisActualTyFunc(const OwnedPtr<AST::Decl>& declPtr);

    std::string FormatCJLibName();
    void WriteToFile();
    void WriteToHeader(std::string content);
    void WriteToSource(std::string content);

    // Write buffer helper functions
    void WriteSeq(const std::vector<std::string>& statements);
    void WriteIf(
        const std::string& cond, const std::function<void()> then, const std::function<void()> other = nullptr);
    void WriteFunc(const std::string& signature, const std::function<void()> body);
    void WriteFor(const std::string& header, const std::function<void()> loop);
    void WriteBlock(
        std::function<void()> action, const std::string& pre = "", const std::string& suf = "", bool flush = false);
    
    void ProcessPreamble(
        struct EmittableObjCClassMetainfo metainfo,
        bool hasImplicitImplParent
    );
    void ProcessMemberDecls(
        struct EmittableObjCClassMetainfo metainfo,
        AST::ClassDecl* classDecl
    );
    void ProcessSuffix(
        std::string cjLibName,
        struct EmittableObjCClassMetainfo metainfo,
        bool hasImplicitImplParent
    );
};
} // namespace Cangjie::Interop::ObjC

#endif // CANGJIE_SEMA_OBJC_GENERATOR_H
