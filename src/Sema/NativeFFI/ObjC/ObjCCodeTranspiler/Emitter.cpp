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

namespace Cangjie::Interop::ObjC {

void Emitter::EmitHeaderImport(std::stringstream &s)
{
    s << "#import <Foundation/Foundation.h>" << std::endl;
    s << "#import <stddef.h>"                << std::endl;
}

void Emitter::EmitSourceImport(std::stringstream &s)
{
    s << "#import <stdlib.h>" << std::endl;
}

void Emitter::EmitFileImport(std::stringstream &s, const std::string& identifier)
{
    s << "#import \"" << identifier << ".h\"" << std::endl;
}

void Emitter::EmitModuleImport(std::stringstream &s, const std::string& identifier)
{
    s << "#import " << identifier << std::endl;
}

void Emitter::EmitTypedef(std::stringstream &s, std::string& identifier)
{
    s << identifier <<  ";" << std::endl;
}

void Emitter::EmitForwardDeclaration(std::stringstream &s, const std::string& identifier)
{
    s << "@class " << identifier << ";" << std::endl;
}

void Emitter::EmitForwardDeclarationProtocol(std::stringstream &s, const std::string& identifier)
{
    s << "@protocol " << identifier << ";" << std::endl;
}

void Emitter::EmitInterfaceDeclaration(std::stringstream &s, struct EmittableObjCClassMetainfo info)
{
    s << "@interface " << info.className << " : " << info.superclassName
      << JoinVec<std::string>(info.interfaces, [](const std::string& s) { return s; }, ", ", " <", ">", false)
      << std::endl;
}

void Emitter::EmitImplementationDeclaration(std::stringstream &s, struct EmittableObjCClassMetainfo info)
{
    s << "@implementation " << info.className << std::endl;
}

void Emitter::EmitObjCFuncHeaderFunctionDecl(std::stringstream &s, struct EmittableObjCFuncMetainfo info)
{
    s << (info.isStatic ? "+ (" : "- (") << info.retType << ")" << info.identifier << info.paramsDecl << ";"
       << std::endl;
}

void Emitter::EmitObjCFuncStaticReference(std::stringstream &s, struct EmittableObjCFuncMetainfo info)
{
    s << "extern " << info.retType << " " << info.mangledIdentifier << info.paramStaticRef << ";"
       << std::endl;
}

void Emitter::EmitObjCFuncInitialization(std::stringstream &s, struct EmittableObjCFuncMetainfo info)
{
    s << "        if ((" << info.mangledIdentifier << " = dlsym(CJWorldDLHandle, \"" << info.mangledIdentifier
      << "\")) == NULL) {" << std::endl;
    s << "            NSLog(@\"ERROR: Failed to find " << info.mangledIdentifier << " symbol in cjworld\");"
      << std::endl;
    s << "            exit(1);" << std::endl;
    s << "        }" << std::endl;
}

void Emitter::EmitSyntheticHeaderFunctionDecls(std::stringstream &s)
{
    s << "@property (readwrite) int64_t $registryId;" << std::endl;
    s << "+ (void)initialize;"        << std::endl;
}

void Emitter::EmitSyntheticRefsDealloc(std::stringstream &s)
{
    s << "- (void)deleteCJObject;"    << std::endl;
    s << "- (void)dealloc;"           << std::endl;
}

void Emitter::EmitInitialize(std::stringstream &s, std::string cjLibraryPath, struct EmittableObjCClassMetainfo info)
{
    s << "+ (void)initialize {"                                                       << std::endl;
    s << "    if (self == [" << info.className << " class]) {"                        << std::endl;
    s << "        if (initCJRuntime(\"" << cjLibraryPath << "\") == false) {"         << std::endl;
    s << "            exit(1);"                                                       << std::endl;
    s << "        }"                                                                  << std::endl;
    s << "    }"                                                                      << std::endl;
    s << "}"                                                                          << std::endl;
}

void Emitter::EmitCtor(std::stringstream &s, struct EmittableObjCFuncMetainfo ctor,
    struct EmittableObjCClassMetainfo cls)
{
    s << "- (id)" << ctor.identifier << ctor.paramsDecl << " {" << std::endl;
    s << "    self.$registryId = -1;" << std::endl;
    s << "    self = (__bridge_transfer " << cls.className << "*)" << ctor.mangledIdentifier
      << "((__bridge void*)self" << ctor.callingParams << ");" << std::endl;
    s << "    return self;" << std::endl;
    s << "}" << std::endl;
}

void Emitter::EmitDoesNotRecognizeSelectorCtor(std::stringstream &s, struct EmittableObjCFuncMetainfo ctor)
{
    s << "- (id)" << ctor.identifier << ctor.paramsDecl << " {" << std::endl;
    s << "    [self doesNotRecognizeSelector:_cmd];"            << std::endl;
    s << "    return nil;"                                      << std::endl;
    s << "}"                                                    << std::endl;
}

void Emitter::EmitDefaultFunctionImplementation(std::stringstream &s, struct EmittableObjCFuncMetainfo func)
{
    s << (func.isStatic ? "+" : "-") << " (" << func.retType << ")" << func.identifier
      << func.paramsDecl << " {" << std::endl;
    if (!func.isStatic) {
        s << "    if (self.$registryId == -1) {" << std::endl;
        s << "        [NSException raise: @\"Use before Cangjie counterpart is initialized\" format: @\"selector `%@`"
              " is overridden in Cangjie and cannot be used before Cangjie counterpart is initialized\","
              " NSStringFromSelector(_cmd)];" << std::endl;
        s << "    }" << std::endl;
    }
    s << "    " << (func.retType != "void" ? "return " : "") << (func.bridge ? "(__bridge_transfer " +
                    func.retType + ")" : "")
                << func.mangledIdentifier << "(" << func.convertedParams << ");" << std::endl;
    s << "}" << std::endl;
}

void Emitter::EmitAdditionalStaticReferences(std::stringstream &s)
{
    s << "extern bool initCJRuntime(const char*);" << std::endl;
}

void Emitter::EmitDeleteCJObject(std::stringstream &s, std::string mangledName)
{
    s << "- (void)deleteCJObject {"                        << std::endl;
    s << "    " << mangledName << "(self.$registryId);"    << std::endl;
    s << "}"                                               << std::endl;
}

void Emitter::EmitDealloc(std::stringstream &s)
{
    s << "- (void)dealloc {"           << std::endl;
    s << "    [self deleteCJObject];"  << std::endl;
    s << "}"                           << std::endl;
}

void Emitter::EmitDeleteCJObjectInitialization(std::stringstream &s, std::string mangledName)
{
    s << "        if ((" << mangledName << " = dlsym(CJWorldDLHandle, \"" << mangledName << "\")) == NULL) {"
      << std::endl;
    s << "            NSLog(@\"ERROR: Failed to find " << mangledName << " symbol in cjworld\");" << std::endl;
    s << "            exit(1);" << std::endl;
    s << "        }" << std::endl;
}

void Emitter::EmitProperty(std::stringstream &s, struct EmittableObjCPropMetainfo prop)
{
    s << "@property (" << (prop.isReadwrite ? "readwrite" : "readonly")
      << ", getter=" << prop.getter.identifier << (prop.isReadwrite ? ", setter=" + prop.setter.identifier : "")
      << ") " << prop.type << " " << prop.name << ";" << std::endl;
}
}