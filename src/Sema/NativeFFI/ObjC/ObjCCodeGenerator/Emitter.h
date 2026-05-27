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

#ifndef CANGJIE_SEMA_OBJC_EMITTER_H
#define CANGJIE_SEMA_OBJC_EMITTER_H

#include <fstream>
#include <string_view>
#include <unordered_set>

#include "NativeFFI/ObjC/AfterTypeCheck/Interop/Context.h"
#include "NativeFFI/ObjC/Utils/Handler.h"
#include "cangjie/AST/Match.h"
#include "cangjie/AST/Types.h"
#include "ObjCParamMapper.h"
#include "ObjCGenerator.h"
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
    inline static const std::string END = "@end";

    static void EmitHeaderImport(std::stringstream *s);

    static void EmitSourceImport(std::stringstream *s);

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
};
}

#endif // CANGJIE_SEMA_OBJC_EMITTER_H