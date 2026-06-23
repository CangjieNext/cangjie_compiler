// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements class for objc code generation.
 */

#include "Transpiler.h"
#include "Emitter.h"
#include "NativeFFI/ObjC/Utils/ASTFactory.h"
#include "NativeFFI/ObjC/Utils/Common.h"
#include "NativeFFI/Utils.h"
#include "cangjie/Sema/TypeManager.h"
#include "cangjie/Utils/FileUtil.h"
#include <iostream>
#include <set>

namespace {
template <typename T>
std::string JoinVec(const std::vector<T>& vec, std::function<std::string(const T&)> trans,
    const std::string& sep = ", ", const std::string& pre = "", const std::string& suf = "", bool force = true)
{
    std::stringstream ss;
    auto it = vec.cbegin();
    if (it == vec.end()) {
        if (!force) {
            return "";
        }
        ss << pre << suf;
        return ss.str();
    }
    ss << pre;
    do {
        ss << trans(*it);
        ++it;
        if (it != vec.cend()) {
            ss << sep;
        }
    } while (it != vec.cend());
    ss << suf;
    return ss.str();
}
}

namespace Cangjie::Interop::ObjC {

void Emitter::EmitHeaderImport(std::stringstream *s) {
    *s << "#import <Foundation/Foundation.h>" << std::endl;
    *s << "#import <stddef.h>"                << std::endl;
}

void Emitter::EmitSourceImport(std::stringstream *s) {
    *s << "#import <stdlib.h>" << std::endl;
}

// #import "identifier.h";
void Emitter::EmitFileImport(std::stringstream *stream, const std::string& identifier) {
    *stream << "#import \"" << identifier << ".h\"" << std::endl;
}

// #import <identifier>;
void Emitter::EmitModuleImport(std::stringstream *s, const std::string& identifier) {
    *s << "#import " << identifier << std::endl;
}

// typedef type(*identifier);
void Emitter::EmitTypedef(std::stringstream *stream, std::string& identifier) {
    *stream << identifier <<  ";" << std::endl;
}

// @class identifier
void Emitter::EmitForwardDeclaration(std::stringstream *stream, const std::string& identifier) {
    *stream << "@class " << identifier << ";" << std::endl;
}

void Emitter::EmitForwardDeclarationProtocol(std::stringstream *stream, const std::string& identifier) {
    *stream << "@protocol " << identifier << ";" << std::endl;
}

// @interface A : M <Proto1, Proto2>
void Emitter::EmitInterfaceDeclaration(std::stringstream *stream, struct EmittableObjCClassMetainfo info) {
    *stream << "@interface " << info.className << " : " << info.superclassName
            << JoinVec<std::string>(info.interfaces,
                [](const std::string& s) { return s; }, ", ", " <", ">", false)
            << std::endl;
}

// @implementation A
void Emitter::EmitImplementationDeclaration(std::stringstream *stream, struct EmittableObjCClassMetainfo info) {
    *stream << "@implementation " << info.className << std::endl;
}

// - (int64_t)primitive2WithArg0:(int64_t)i withArg1:(float)f;
void Emitter::EmitObjCFuncHeaderFunctionDecl(std::stringstream *stream, struct EmittableObjCFuncMetainfo info) {
    *stream << (info.isStatic ? "+ (" : "- (") << info.retType << ")" << info.identifier << info.paramsDecl << ";"
        << std::endl;
}
// static int64_t (*CJImpl_ObjC_cjworld_Derived_mirror1WithArg0_CN7cjworld3BoxE)(int64_t,void*) = NULL;
void Emitter::EmitObjCFuncStaticReference(std::stringstream *stream, struct EmittableObjCFuncMetainfo info) {
    *stream << "extern " << info.retType << " " << info.mangledIdentifier << info.paramStaticRef << ";"
        << std::endl;
}

void Emitter::EmitObjCFuncInitialization(std::stringstream *stream, struct EmittableObjCFuncMetainfo info) {
    *stream << "        if ((" << info.mangledIdentifier << " = dlsym(CJWorldDLHandle, \"" << info.mangledIdentifier << "\")) == NULL) {" << std::endl;
    *stream << "            NSLog(@\"ERROR: Failed to find " << info.mangledIdentifier << " symbol in cjworld\");" << std::endl;
    *stream << "            exit(1);" << std::endl;
    *stream << "        }" << std::endl;
}

void Emitter::EmitSyntheticHeaderFunctionDecls(std::stringstream *stream) {
    *stream << "@property (readwrite) int64_t $registryId;" << std::endl;
    *stream << "+ (void)initialize;"        << std::endl;
}

void Emitter::EmitSyntheticRefsDealloc(std::stringstream *s) {
    *s << "- (void)deleteCJObject;"    << std::endl;
    *s << "- (void)dealloc;"           << std::endl;
}

void Emitter::EmitInitialize(std::stringstream *s, std::string cjLibraryPath, struct EmittableObjCClassMetainfo info) {
    *s << "+ (void)initialize {"                                                       << std::endl;
    *s << "    if (self == [" << info.className << " class]) {"                        << std::endl;
    *s << "        if (initCJRuntime(\"" << cjLibraryPath << "\") == false) {"         << std::endl;
    *s << "            exit(1);"                                                       << std::endl;
    *s << "        }"                                                                  << std::endl;
    *s << "    }"                                                                      << std::endl;
    *s << "}"                                                                          << std::endl;
}

void Emitter::EmitCtor(std::stringstream *s, struct EmittableObjCFuncMetainfo ctor, struct EmittableObjCClassMetainfo cls) {
    *s << "- (id)" << ctor.identifier << ctor.paramsDecl << " {" << std::endl;
    *s << "    self.$registryId = -1;" << std::endl;
    *s << "    self = (__bridge_transfer " << cls.className << "*)" << ctor.mangledIdentifier << "((__bridge void*)self" << ctor.callingParams << ");" << std::endl;
    *s << "    return self;" << std::endl;
    *s << "}" << std::endl;
}

void Emitter::EmitDoesNotRecognizeSelectorCtor(std::stringstream *s, struct EmittableObjCFuncMetainfo ctor) {
    *s << "- (id)" << ctor.identifier << ctor.paramsDecl << " {" << std::endl;
    *s << "    [self doesNotRecognizeSelector:_cmd];"           << std::endl;
    *s << "}"                                                   << std::endl;
}

void Emitter::EmitDefaultFunctionImplementation(std::stringstream *s, struct EmittableObjCFuncMetainfo func) {
    *s << (func.isStatic ? "+" : "-") << " (" << func.retType << ")" << func.identifier << func.paramsDecl << " {" << std::endl;
    if (!func.isStatic) {
        *s << "    if (self.$registryId == -1) {" << std::endl;
        *s << "        [NSException raise: @\"Use before Cangjie counterpart is initialized\" format: @\"selector `%@` is overriden in Cangjie and cannot be used before Cangjie counterpart is initialized\", NSStringFromSelector(_cmd)];" << std::endl;
        *s << "    }" << std::endl;
    }
    *s << "    " << (func.retType != "void" ? "return " : "" ) << (func.bridge ? "(__bridge " + func.retType + ")" : "")
                    << func.mangledIdentifier << "(" << func.convertedParams << ");" << std::endl;
    *s << "}" << std::endl;
}

void Emitter::EmitAdditionalStaticReferences(std::stringstream *s) {
    *s << "extern bool initCJRuntime(const char*);" << std::endl;
}

void Emitter::EmitDeleteCJObject(std::stringstream* s, std::string mangledName) {
    *s << "- (void)deleteCJObject {" << std::endl;
    *s << "    " << mangledName << "(self.$registryId);" << std::endl;
    *s << "}" << std::endl;
}

void Emitter::EmitDealloc(std::stringstream *s) {
    *s << "- (void)dealloc {" << std::endl;
    *s << "    [self deleteCJObject];" << std::endl;
    *s << "}" << std::endl;
}

void Emitter::EmitDeleteCJObjectInitialization(std::stringstream *s, std::string mangledName) {
    *s << "        if ((" << mangledName << " = dlsym(CJWorldDLHandle, \"" << mangledName << "\")) == NULL) {" << std::endl;
    *s << "            NSLog(@\"ERROR: Failed to find " << mangledName << " symbol in cjworld\");" << std::endl;
    *s << "            exit(1);" << std::endl;
    *s << "        }" << std::endl;
}

void Emitter::EmitProperty(std::stringstream *s, struct EmittableObjCPropMetainfo prop) {
    *s << "@property (" << (prop.isReadwrite ? "readwrite" : "readonly")
       << ", getter=" << prop.getter.identifier << (prop.isReadwrite ? ", setter=" + prop.setter.identifier : "")
       << ") " << prop.type << " " << prop.name << ";" << std::endl; 
}
}