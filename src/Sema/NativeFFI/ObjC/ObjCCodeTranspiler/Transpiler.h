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

#ifndef CANGJIE_SEMA_NATIVE_FFI_OBJC_INTEROP_TRANSPILER
#define CANGJIE_SEMA_NATIVE_FFI_OBJC_INTEROP_TRANSPILER

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

using ArgsList = std::vector<std::pair<std::string, std::string>>;

class Transpiler {
public:
    Transpiler(InteropContext& ctx, Ptr<AST::Decl> declArg, const std::string& outputFilePath,
        const std::string& cjLibOutputPath, InteropType interopType);
    Transpiler(InteropContext& ctx, Ptr<AST::Decl> declArg, const std::string& outputFilePath,
        const std::string& cjLibOutputPath, InteropType interopType, Native::FFI::GenericConfigInfo* genericConfig,
        bool isGenericGlueCode);
    void Generate();

private:
    struct EmittableObjCClassMetainfo GetObjCClassMetainfo(AST::ClassDecl* classDecl);
    struct EmittableObjCFuncMetainfo GetObjCFuncMetainfo(AST::FuncDecl& funcDecl, bool mangled = false);
    struct EmittableObjCFuncMetainfo GetObjCCtorMetainfo(AST::FuncDecl& funcDecl);
    struct EmittableObjCPropMetainfo GetObjCPropMetainfoFromProp(AST::VarDeclAbstract& varDecl);

    bool CheckFunction(OwnedPtr<AST::Decl>& arg) const;
    bool CheckCtor(OwnedPtr<AST::Decl>& arg) const;
    bool CheckProp(OwnedPtr<AST::Decl>& arg) const;
    void CollectDependencies(Ptr<AST::Ty> ty);

    std::stringstream sourceBody;
    std::stringstream headerBody;
    std::stringstream dependenciesStream;
    std::stringstream sourceImport;
    std::stringstream headerImport;
    std::stringstream staticReferences;
    std::stringstream funcInitialization;
    std::stringstream initialize;
    std::stringstream ctors;
    std::stringstream defaultFuncImpls;

    std::set<Ptr<AST::Decl>> dependencies;
    std::vector<std::string> typedefs;

    std::string resPreamble;
    const std::string& outputFilePath;
    const std::string& cjLibOutputPath;
    size_t currentBlockIndent = 0;
    Ptr<AST::Decl> decl;
    InteropContext& ctx;
    InteropType interopType;
    
    std::stringstream buffer;
    Native::FFI::GenericConfigInfo* genericConfig = nullptr;
    bool isGenericGlueCode{false};

    bool SkipSetterForValueTypeDecl(AST::Decl& declArg) const;
    bool IsNotThisActualTyFunc(const OwnedPtr<AST::Decl>& declPtr) const;

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

    inline static const std::string DEFAULT_OUTPUT_DIR = "objc-gen";
    inline static const std::string DEFAULT_OUTPUT_DIR_WARNING =
        "default value objc-gen of Objective-C generated sources directory will be changed in"
        " next Cangjie SDK release";
};
} // namespace Cangjie::Interop::ObjC

#endif // CANGJIE_SEMA_NATIVE_FFI_OBJC_INTEROP_TRANSPILER
