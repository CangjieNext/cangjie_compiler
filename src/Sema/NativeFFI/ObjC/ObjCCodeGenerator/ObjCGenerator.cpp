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

#include "ObjCGenerator.h"
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
} // namespace

namespace Cangjie::Interop::ObjC {

using namespace Cangjie;
using namespace AST;
using namespace Native::FFI;
using std::string;

ObjCGenerator::ObjCGenerator(InteropContext& ctx, Ptr<Decl> declArg, const std::string& outputFilePath,
    const std::string& cjLibOutputPath, InteropType interopType)
    : outputFilePath(outputFilePath.empty() ? Emitter::DEFAULT_OUTPUT_DIR : outputFilePath),
      cjLibOutputPath(cjLibOutputPath),
      decl(declArg),
      ctx(ctx),
      interopType(interopType),
      paramMapper(ObjCParamMapper(ctx, declArg, nullptr))
{
    if (outputFilePath.empty()) {
        Warningln(Emitter::DEFAULT_OUTPUT_DIR_WARNING);
    }
}

ObjCGenerator::ObjCGenerator(InteropContext& ctx, Ptr<AST::Decl> declArg, const std::string& outputFilePath,
    const std::string& cjLibOutputPath, InteropType interopType, Native::FFI::GenericConfigInfo* genericConfig,
    bool isGenericGlueCode)
    : outputFilePath(outputFilePath.empty() ? Emitter::DEFAULT_OUTPUT_DIR : outputFilePath),
      cjLibOutputPath(cjLibOutputPath),
      decl(declArg),
      ctx(ctx),
      interopType(interopType),
      paramMapper(ObjCParamMapper(ctx, declArg, nullptr)),
      genericConfig(genericConfig),
      isGenericGlueCode(isGenericGlueCode)
{
    if (outputFilePath.empty()) {
        Warningln(Emitter::DEFAULT_OUTPUT_DIR_WARNING);
    }
}

bool ObjCGenerator::CheckFunction(OwnedPtr<Decl>& arg) {
    if (ctx.factory.IsGeneratedMember(*arg)) {
        return false;
    }

    if (!arg->TestAttr(Attribute::PUBLIC)) {
        return false;
    }
    if (interopType == InteropType::CJ_Mapping &&
        (!ctx.typeMapper.IsObjCCJMappingMember(*arg) || arg->IsOpen())) {
        return false;
    }
    if (IsNotThisActualTyFunc(arg)) {
        return false;
    }
    if (arg->astKind == ASTKind::FUNC_DECL &&
        !arg->TestAnyAttr(Attribute::CONSTRUCTOR, Attribute::FINALIZER)) {
        return true;
    }

    return false;
}

bool ObjCGenerator::CheckCtor(OwnedPtr<Decl>& arg) {
    if (!arg->TestAttr(Attribute::PUBLIC)) {
        return false;
    }
    if (ctx.factory.IsGeneratedMember(*arg.get())) {
        return false;
    }

    if (!arg->TestAttr(Attribute::CONSTRUCTOR)) {
        return false;
    }

    if (arg->astKind != ASTKind::FUNC_DECL) {
        // skip primary ctor, as it is desugared to init already
        return false;
    }

    const FuncDecl& funcDecl = *StaticAs<ASTKind::FUNC_DECL>(arg.get());
    if (!funcDecl.funcBody) {
        return false;
    }

    if (interopType == InteropType::CJ_Mapping && !ctx.typeMapper.IsObjCCJMappingMember(*arg)) {
        return false;
    }

    return true;
}

void ObjCGenerator::CollectDependencies(Ptr<Ty> ty) {
    if (ctx.typeMapper.IsObjCObjectType(*ty)) {
        // ObjCId is `id` representative which is builtin type for Objective-C
        if (TypeMapper::IsObjCId(*ty)) {
            return;
        }

        if (!ty->IsCoreOptionType()) {
            auto declTy = Ty::GetDeclOfTy(ty);
            if (declTy->identifier != ctx.nameGenerator.GetObjCDeclName(*decl)) {
                dependencies.insert(declTy);
            }
        }
    }
    for (auto&& typeArg : ty->typeArgs) {
        CollectDependencies(typeArg);
    }
}

std::string ObjCGenerator::FormatCJLibName() {
    auto cjLibName = Native::FFI::GetCangjieLibName(cjLibOutputPath, decl->fullPackageName, false);
    // Crutch that solves issue when cjLibOutputPath is dir or not provided.
    if (cjLibName.find("lib", 0) != 0) {
        cjLibName = "lib" + cjLibName;
    }
    size_t extIdx = cjLibName.find_last_of(".");
    const auto& sharedLibraryExtension = ctx.sharedLibraryExtension;
    if (extIdx == std::string::npos || cjLibName.substr(extIdx) != sharedLibraryExtension) {
        cjLibName += sharedLibraryExtension;
    }
    return cjLibName;
}

void ObjCGenerator::ProcessPreamble(
    struct EmittableObjCClassMetainfo metainfo,
    bool hasImplicitImplParent
) {
    Emitter::EmitFileImport(&importSource, metainfo.className);
    Emitter::EmitModuleImport(&importSource, Emitter::STDLIB_IMPORT);
    Emitter::EmitModuleImport(&importHeader, Emitter::FOUNDATION_IMPORT);
    Emitter::EmitModuleImport(&importHeader, Emitter::STDDEF_IMPORT);
    for (auto i : metainfo.interfaces) {
        Emitter::EmitFileImport(&importHeader, i);
    }


    Emitter::EmitInterfaceDeclaration(&tmpHeader, metainfo);
    Emitter::EmitFileImport(&importHeader, metainfo.superclassName);
    Emitter::EmitImplementationDeclaration(&tmpSource, metainfo);
    Emitter::EmitSyntheticHeaderFunctionDecls(&tmpHeader);
    if (!hasImplicitImplParent) {
        Emitter::EmitSyntheticRefsDealloc(&tmpHeader);
    }
}

void ObjCGenerator::ProcessSuffix(
    std::string cjLibName,
    struct EmittableObjCClassMetainfo metainfo,
    bool hasImplicitImplParent
) {
    Emitter::EmitAdditionalStaticReferences(&staticReferences);

    if (!hasImplicitImplParent) {
        auto deleteCJObjectMangledName = ctx.nameGenerator.GenerateDeleteCjObjectName(*decl);
        Emitter::EmitDeleteCJObject(&defaultFuncImpls, deleteCJObjectMangledName);
        Emitter::EmitDeleteCJObjectInitialization(&funcInitialization, deleteCJObjectMangledName);
        Emitter::EmitDealloc(&defaultFuncImpls);
    }

    Emitter::EmitEnd(&tmpHeader);
    Emitter::EmitEnd(&defaultFuncImpls); // TODO: ?
    Emitter::EmitInitialize(&initialize, cjLibName, metainfo);
}

void ObjCGenerator::ProcessMemberDecls(
    struct EmittableObjCClassMetainfo metainfo,
    ClassDecl* classDecl
) {
    std::set<std::vector<std::string>> generatedCtors = {};

    for (OwnedPtr<Decl>& declPtr : decl->GetMemberDecls()) {
        switch(declPtr->astKind) {
            case ASTKind::FUNC_DECL:
                if (CheckFunction(declPtr)) {
                    FuncDecl& funcDecl = *StaticAs<ASTKind::FUNC_DECL>(declPtr.get());
                    auto funcMeta = GetObjCFuncMetainfo(funcDecl);

                    Emitter::EmitObjCFuncHeaderFunctionDecl(&tmpHeader, funcMeta);
                    Emitter::EmitObjCFuncInitialization(&funcInitialization, funcMeta);
                    Emitter::EmitDefaultFunctionImplementation(&defaultFuncImpls, funcMeta);

                    CollectDependencies(funcDecl.GetTy());
                }
                if (CheckCtor(declPtr)) {
                    FuncDecl& funcDecl = *StaticAs<ASTKind::FUNC_DECL>(declPtr.get());
                    if (funcDecl.funcBody && funcDecl.funcBody->retType) {
                        auto funcMeta = GetObjCCtorMetainfo(funcDecl);

                        Emitter::EmitCtor(&ctors, funcMeta, metainfo);
                        Emitter::EmitObjCFuncInitialization(&funcInitialization, funcMeta);
                        Emitter::EmitObjCFuncHeaderFunctionDecl(&tmpHeader, funcMeta);

                        CollectDependencies(funcDecl.GetTy());
                        if (interopType == InteropType::ObjC_Mirror) {
                            generatedCtors.insert(funcMeta.selectorComponents); // TODO: fix
                        }
                    }
                }
                break;
            case ASTKind::VAR_DECL:
                if (CheckProp(declPtr)) {
                    VarDecl& propDecl = *StaticAs<ASTKind::VAR_DECL>(declPtr.get());
                    auto propMeta = GetObjCPropMetainfoFromVar(propDecl);
                    Emitter::EmitObjCFuncInitialization(&funcInitialization, propMeta.getter);
                    Emitter::EmitDefaultFunctionImplementation(&defaultFuncImpls, propMeta.getter);
                    Emitter::EmitProperty(&tmpHeader, propMeta);
                    Emitter::EmitObjCFuncHeaderFunctionDecl(&tmpHeader, propMeta.getter);
                    if (propMeta.isReadwrite) {
                        Emitter::EmitObjCFuncInitialization(&funcInitialization, propMeta.setter);
                        Emitter::EmitDefaultFunctionImplementation(&defaultFuncImpls, propMeta.setter);
                        Emitter::EmitObjCFuncHeaderFunctionDecl(&tmpHeader, propMeta.setter);
                    }
                    CollectDependencies(propDecl.GetTy());
                }
                break;
            case ASTKind::PROP_DECL:
                if (CheckProp(declPtr)) {
                    VarDecl& propDecl = *StaticAs<ASTKind::PROP_DECL>(declPtr.get());
                    auto propMeta = GetObjCPropMetainfoFromVar(propDecl);
                    Emitter::EmitObjCFuncInitialization(&funcInitialization, propMeta.getter);
                    Emitter::EmitDefaultFunctionImplementation(&defaultFuncImpls, propMeta.getter);
                    Emitter::EmitProperty(&tmpHeader, propMeta);
                    Emitter::EmitObjCFuncHeaderFunctionDecl(&tmpHeader, propMeta.getter);
                    if (propMeta.isReadwrite) {
                        Emitter::EmitObjCFuncInitialization(&funcInitialization, propMeta.setter);
                        Emitter::EmitDefaultFunctionImplementation(&defaultFuncImpls, propMeta.setter);
                        Emitter::EmitObjCFuncHeaderFunctionDecl(&tmpHeader, propMeta.setter);
                    }
                    CollectDependencies(propDecl.GetTy());
                }
                break;
            default:
                break;
        }
    }
    for (auto &declPtr : ctx.genDecls) {
        if (declPtr->curFile != decl->curFile || !declPtr->TestAttr(Attribute::C) ||
            declPtr->TestAttr(Attribute::FOREIGN)) {
            continue;
        }

        if (IsNotThisActualTyFunc(declPtr)) {
            continue;
        }

        switch(declPtr->astKind) {
            case ASTKind::FUNC_DECL:
                if (CheckFunction(declPtr)) {
                    Emitter::EmitObjCFuncStaticReference(&staticReferences,
                        GetObjCFuncMetainfo(*StaticAs<ASTKind::FUNC_DECL>(declPtr.get()), true));
                }
                break;
            default:
                break;
        }
    }
    // suffix
    for (auto d : dependencies) {
        (d->astKind == ASTKind::INTERFACE_DECL)
            ? Emitter::EmitForwardDeclarationProtocol(&dependenciesStream, d->identifier.Val())
            : Emitter::EmitForwardDeclaration(&dependenciesStream, d->identifier.Val());
        Emitter::EmitFileImport(&importSource, d->identifier.Val()); // TODO: not identifier
    }
    for (auto td : paramMapper.typedefs) {
        Emitter::EmitTypedef(&importHeader, td);
    }

    if (interopType == InteropType::ObjC_Mirror && classDecl) { // TODO: refactor
        auto ctorsToGenerate = ctx.factory.GetAllParentCtors(*classDecl);
        for (auto ctor : ctorsToGenerate) {
            // public superconstructors & non-public own constructors
            if (!ctor->TestAttr(Attribute::PUBLIC) && ctor->outerDecl != decl) {
                continue;
            }
            if (ctx.factory.IsGeneratedMember(*ctor)) {
                continue;
            }
            if (ctor->funcBody->paramLists[0]->params.size() > 1 && !ctx.nameGenerator.GetUserDefinedObjCName(*ctor)) {
                // remove when generating selectors without foreign name is implemented
                continue;
            }
            auto selectorComponents = ctx.nameGenerator.GetObjCDeclSelectorComponents(*ctor);
            if (generatedCtors.count(selectorComponents) > 0) {
                continue;
            }

            auto funcMeta = GetObjCFuncMetainfo(*ctor);
            funcMeta.retType = Emitter::ID_TYPE;

            std::string result = "";
            Emitter::EmitDoesNotRecognizeSelectorCtor(&ctors, funcMeta);
            Emitter::EmitObjCFuncHeaderFunctionDecl(&tmpHeader, funcMeta);
            generatedCtors.insert(selectorComponents);
        }
    }
}

/*
    Main access point to translation of CJ class to Objective C. Generates two files - .h and .m

    Example of header, .h:
    #import <Foundation/Foundation.h>
            ...
    @interface A : M
    + (void)initialize;
    - (id)init;
    @property (readwrite) UIntNative obj;
    - (void)goo;
            ...
    @end

    Example of source, .m:
    #import "A.h"
            ...
    static void (*CJImpl_ObjC_A_goo)(size_t) = NULL;
            ...
    @implementation A
    + (void)initialize {
            ...
    }
            ...
    - (void)goo {
        CJImpl_ObjC_A_goo(self.obj);
    }
            ...
    @end
*/
void ObjCGenerator::Generate()
{
    auto cjLibName = FormatCJLibName();
    auto classDecl = dynamic_cast<ClassDecl*>(decl.get());
    auto metainfo = GetObjCClassMetainfo(classDecl);

    auto hasImplicitImplParent = HasImplSuperClass(*classDecl);

    ProcessPreamble(metainfo, hasImplicitImplParent);
    ProcessMemberDecls(metainfo, classDecl);
    ProcessSuffix(cjLibName, metainfo, hasImplicitImplParent);

    WriteToFile();
}

// Filter out elements not currently instantiated.
bool ObjCGenerator::IsNotThisActualTyFunc(const OwnedPtr<Decl>& declPtr) {
    if (genericConfig) {
        auto actualOuterDeclName = genericConfig->declInstName;
        if (declPtr->identifier.Val().find(actualOuterDeclName) == std::string::npos) {
            return true;
        }
    }
    return false;
}


struct EmittableObjCClassMetainfo ObjCGenerator::GetObjCClassMetainfo(ClassDecl* classDecl) {
    auto className = ctx.nameGenerator.GetObjCDeclName(*classDecl);

    Ptr<ClassDecl> superClassPtr = classDecl ? classDecl->GetSuperClassDecl() : Ptr<ClassDecl>(nullptr);
    bool isClassInheritedFromClass = superClassPtr && superClassPtr->identifier.Val() != Cangjie::OBJECT_NAME;
    auto superclassName = isClassInheritedFromClass ? ctx.nameGenerator.GetObjCDeclName(*superClassPtr) : "";

    auto isFinal = interopType == InteropType::CJ_Mapping && ctx.typeMapper.IsOneWayMapping(*decl) && !decl->IsOpen();

    std::set<Ptr<Cangjie::AST::InterfaceTy>> interfaces = classDecl
        ? classDecl->GetSuperInterfaceTys()
        : std::set<Ptr<Cangjie::AST::InterfaceTy>>();

    std::vector<Ptr<Cangjie::AST::InterfaceTy>> interfacesVec;
    std::copy_if (interfaces.begin(), interfaces.end(),
        std::back_inserter(interfacesVec),
        [](Ptr<Cangjie::AST::InterfaceTy> i) {
            return !TypeMapper::IsObjCId(*i);
        }
    );

    std::vector<std::string> interfaceNames;
    for (auto i : interfacesVec) {
        interfaceNames.push_back(this->ctx.nameGenerator.GetObjCDeclName(*i->decl));
    }

    EmittableObjCClassMetainfo eocm;
    eocm.className = className;
    eocm.superclassName = superclassName;
    eocm.isFinal = isFinal;
    eocm.interfaces = interfaceNames;
    return eocm;
}

bool ObjCGenerator::CheckProp(OwnedPtr<Decl>& arg) {
    if (arg->astKind != ASTKind::VAR_DECL && arg->astKind != ASTKind::PROP_DECL) {
        return false;
    }

    if (!arg->TestAttr(Attribute::PUBLIC)) {
        return false;
    }

    if (ctx.factory.IsGeneratedNativeHandleField(*arg)) {
        return false;
    }

    if (interopType == InteropType::CJ_Mapping && !ctx.typeMapper.IsObjCCJMappingMember(*arg)) {
        return false;
    }

    return true;
}

bool ObjCGenerator::SkipSetterForValueTypeDecl(Decl& declArg) const
{
    return interopType == InteropType::CJ_Mapping && DynamicCast<StructTy*>(declArg.GetTy().get()) != nullptr;
}

struct EmittableObjCPropMetainfo ObjCGenerator::GetObjCPropMetainfoFromProp(PropDecl& varDecl) {
    EmittableObjCPropMetainfo empm = EmittableObjCPropMetainfo();
    empm.isStatic = varDecl.TestAttr(Attribute::STATIC);
    if (isGenericGlueCode && varDecl.GetTy()->IsGeneric()) {
        auto genericActualTy =
            TypeManager::GetPrimitiveTy(GetActualTypeKind(GetGenericActualType(genericConfig, varDecl.GetTy()->name)));
        empm.type = paramMapper.MapCJTypeToObjCType(*genericActualTy);
    } else {
        empm.type = paramMapper.MapCJTypeToObjCType(*varDecl.GetTy());
    }
    empm.isReadwrite = varDecl.isVar && !SkipSetterForValueTypeDecl(*decl);
    empm.name = ctx.nameGenerator.GetObjCDeclName(varDecl);
    auto bridge = (ctx.typeMapper.IsObjCObjectType(*varDecl.GetTy()) || ctx.typeMapper.IsObjCBlock(*varDecl.GetTy())) &&
                        !ctx.typeMapper.IsObjCCJMapping(*varDecl.GetTy());
    
    EmittableObjCFuncMetainfo getter;
    getter.isStatic             = empm.isStatic;
    getter.selectorComponents   = {};
    getter.identifier           = ctx.nameGenerator.GetObjCGetterName(varDecl);
    getter.mangledIdentifier    = ctx.nameGenerator.GetFieldGetterWrapperName(varDecl);
    getter.retType              = empm.type;
    getter.paramsDecl           = ""; // always no params for getter TODO: refactor
    getter.paramStaticRef       = empm.isStatic ? "" : "int64_t";
    getter.callingParams        = empm.isStatic ? "" : "self.$registryId";
    getter.convertedParams      = empm.isStatic ? "" : "self.$registryId";
    getter.bridge               = bridge;
    empm.getter = getter;

    if (empm.isReadwrite) {
        EmittableObjCFuncMetainfo setter;
        setter.isStatic             = empm.isStatic;
        setter.selectorComponents   = {};
        setter.identifier           = ctx.nameGenerator.GetObjCSetterName(varDecl);
        setter.mangledIdentifier    = ctx.nameGenerator.GetFieldSetterWrapperName(varDecl);
        setter.retType              = Emitter::VOID_TYPE;
        setter.bridge               = bridge;
        setter.paramsDecl           = "(" + empm.type + ")value"; // TODO: refactor
        setter.paramStaticRef       = (!empm.isStatic
                                        ? "(int64_t,"
                                        : "(") + empm.type + ")";
        setter.callingParams        = !empm.isStatic ? "self.$registryId, value" : "value";
        setter.convertedParams      = (!empm.isStatic
                                        ? "self.$registryId, "
                                        : "") + paramMapper.GenerateArgumentCast(*varDecl.GetTy(), "value");
        empm.setter = setter;
    }
    
    return empm;
}

struct EmittableObjCPropMetainfo ObjCGenerator::GetObjCPropMetainfoFromVar(VarDecl& varDecl) {
    EmittableObjCPropMetainfo empm = EmittableObjCPropMetainfo();
    empm.isStatic = varDecl.TestAttr(Attribute::STATIC);
    if (isGenericGlueCode && varDecl.GetTy()->IsGeneric()) {
        auto genericActualTy =
            TypeManager::GetPrimitiveTy(GetActualTypeKind(GetGenericActualType(genericConfig, varDecl.GetTy()->name)));
        empm.type = paramMapper.MapCJTypeToObjCType(*genericActualTy);
    } else {
        empm.type = paramMapper.MapCJTypeToObjCType(*varDecl.GetTy());
    }
    empm.isReadwrite = varDecl.isVar && !SkipSetterForValueTypeDecl(*decl);
    empm.name = ctx.nameGenerator.GetObjCDeclName(varDecl);
    auto bridge = (ctx.typeMapper.IsObjCObjectType(*varDecl.GetTy()) || ctx.typeMapper.IsObjCBlock(*varDecl.GetTy())) &&
            !ctx.typeMapper.IsObjCCJMapping(*varDecl.GetTy());
    
    EmittableObjCFuncMetainfo getter;
    getter.isStatic             = empm.isStatic;
    getter.selectorComponents   = {};
    getter.identifier           = ctx.nameGenerator.GetObjCGetterName(varDecl);
    getter.mangledIdentifier    = ctx.nameGenerator.GetFieldGetterWrapperName(varDecl);
    getter.retType              = empm.type;
    getter.paramsDecl           = ""; // always no params for getter TODO: refactor
    getter.paramStaticRef       = "int64_t";
    getter.callingParams        = !empm.isStatic ? "self.$registryId" : "";
    getter.convertedParams      = !empm.isStatic ? "self.$registryId" : "";
    getter.bridge               = bridge;
    empm.getter = getter;

    if (empm.isReadwrite) {
        EmittableObjCFuncMetainfo setter;
        setter.isStatic             = empm.isStatic;
        setter.selectorComponents   = {};
        setter.identifier           = ctx.nameGenerator.GetObjCSetterName(varDecl);
        setter.mangledIdentifier    = ctx.nameGenerator.GetFieldSetterWrapperName(varDecl);
        setter.retType              = Emitter::VOID_TYPE;
        setter.paramsDecl           = "(" + empm.type + ")value"; // TODO: refactor
        setter.paramStaticRef       = (!empm.isStatic
                                        ? "(int64_t,"
                                        : "(") + empm.type + ")";
        setter.callingParams        = !empm.isStatic ? "self.$registryId, value" : "value";
        setter.convertedParams      = (!empm.isStatic
                                        ? "self.$registryId, "
                                        : "") + paramMapper.GenerateArgumentCast(*varDecl.GetTy(), "value");
        setter.bridge               = false; // always VOID_TYPE
        empm.setter = setter;
    }
    
    return empm;
}

struct EmittableObjCFuncMetainfo ObjCGenerator::GetObjCFuncMetainfo(FuncDecl& funcDecl, bool mangled) {
    EmittableObjCFuncMetainfo eofm;

    eofm.isStatic            = funcDecl.TestAttr(Attribute::STATIC);
    eofm.selectorComponents  = ctx.nameGenerator.GetObjCDeclSelectorComponents(funcDecl);
    eofm.mangledIdentifier   = mangled ? funcDecl.identifier : ctx.nameGenerator.GenerateMethodWrapperName(funcDecl);
    eofm.identifier          = eofm.selectorComponents[0];
    auto& retTy = *StaticCast<const FuncTy*>(funcDecl.GetTy())->retTy;
    // eofm.retType             = ctx.typeMapper.IsObjCObjectType(retTy) || ctx.typeMapper.IsObjCBlock(retTy)
    //                             ? VOID_POINTER_TYPE
    //                             : MapCJTypeToObjCType(retTy);
    eofm.retType             = paramMapper.MapCJTypeToObjCType(retTy);
    eofm.paramsDecl          = paramMapper.GenerateFuncParamLists(funcDecl.funcBody->paramLists, eofm.selectorComponents,
                                FunctionListFormat::DECLARATION, eofm.isStatic ? ObjCFunctionType::STATIC : ObjCFunctionType::INSTANCE, // refactor
                                GetForeignNameAnnotation(funcDecl) != nullptr);
    auto collectTypes = [this](const OwnedPtr<FuncParam>& fp) -> std::string {
        return paramMapper.MapCJTypeToObjCType(*fp->GetTy());
    };
    eofm.paramStaticRef      = funcDecl.funcBody->paramLists[0]->params.size() == 0
                                    ? "()"
                                    : JoinVec<OwnedPtr<FuncParam>>(funcDecl.funcBody->paramLists[0]->params, collectTypes, ",", "(", ")", false);
    auto collectNames = [](const OwnedPtr<FuncParam>& fp) -> std::string {
        return fp->identifier;
    };
    eofm.callingParams       = JoinVec<OwnedPtr<FuncParam>>(funcDecl.funcBody->paramLists[0]->params, collectNames, ", ", ", ", "", false);
    eofm.convertedParams     = paramMapper.ConvertParamsListToArgsListToString(funcDecl.funcBody->paramLists, !eofm.isStatic);
    eofm.bridge              = (ctx.typeMapper.IsObjCObjectType(retTy) || ctx.typeMapper.IsObjCBlock(retTy)) &&
                                    !ctx.typeMapper.IsObjCCJMapping(retTy);
    return eofm;
}

struct EmittableObjCFuncMetainfo ObjCGenerator::GetObjCCtorMetainfo(FuncDecl& funcDecl) {
    EmittableObjCFuncMetainfo eofm;

    const auto ctor = interopType == InteropType::ObjC_Mirror
            ? ctx.factory.GetGeneratedImplCtor(*decl, funcDecl).get()
            : &funcDecl;
    CJC_ASSERT(ctor);
    const auto selectorComponents = ctx.nameGenerator.GetObjCDeclSelectorComponents(funcDecl);
    CJC_ASSERT(selectorComponents.size() > 0);
    // wrapper name MUST use generated ctor
    const auto cjWrapperName = genericConfig ? ctx.nameGenerator.GenerateInitCjObjectName(*ctor, &genericConfig->declInstName) :
        ctx.nameGenerator.GenerateInitCjObjectName(*ctor);

    eofm.isStatic            = ctor->TestAttr(Attribute::STATIC);
    eofm.selectorComponents  = selectorComponents;
    eofm.mangledIdentifier   = cjWrapperName;
    eofm.identifier          = eofm.selectorComponents[0];
    eofm.retType             = Emitter::ID_TYPE;
    eofm.paramsDecl          = paramMapper.GenerateFuncParamLists(funcDecl.funcBody->paramLists, eofm.selectorComponents,
                                FunctionListFormat::DECLARATION, eofm.isStatic ? ObjCFunctionType::STATIC : ObjCFunctionType::INSTANCE, // refactor
                                GetForeignNameAnnotation(funcDecl) != nullptr);
    auto collectTypes = [this](const OwnedPtr<FuncParam>& fp) -> std::string {
        return paramMapper.MapCJTypeToObjCType(*fp->GetTy());
    };
    eofm.paramStaticRef      = JoinVec<OwnedPtr<FuncParam>>(funcDecl.funcBody->paramLists[0]->params, collectTypes, ",", "(", ")", false);
    auto collectNames = [this](const OwnedPtr<FuncParam>& fp) -> std::string {
        return paramMapper.GenerateArgumentCast(*fp->GetTy(), fp->identifier);
    };
    eofm.callingParams       = JoinVec<OwnedPtr<FuncParam>>(funcDecl.funcBody->paramLists[0]->params, collectNames, ", ", ", ", "", false);
    eofm.bridge              = false;
    return eofm;
}

void ObjCGenerator::WriteToFile()
{
    WriteToHeader(
        res                         +
        importHeader.str()          +
        dependenciesStream.str()    +
        tmpHeader.str()
    );
    WriteToSource(
        resSource               +
        importSource.str()      +
        staticReferences.str()  +
        tmpSource.str()         +
        ctors.str()             +
        initialize.str()        +
        defaultFuncImpls.str()
    );
}

void ObjCGenerator::WriteToHeader(std::string content)
{
    auto objCDeclName = genericConfig ? ctx.nameGenerator.GetObjCDeclName(*decl, &genericConfig->declInstName) :
        ctx.nameGenerator.GetObjCDeclName(*decl);
    auto headerPath = FileUtil::JoinPath(outputFilePath, objCDeclName + ".h");
    FileUtil::WriteToFile(headerPath, content);
}

void ObjCGenerator::WriteToSource(std::string content)
{
    auto objCDeclName = genericConfig ? ctx.nameGenerator.GetObjCDeclName(*decl, &genericConfig->declInstName) :
        ctx.nameGenerator.GetObjCDeclName(*decl);
    auto sourcePath = FileUtil::JoinPath(outputFilePath, objCDeclName + ".m");
    FileUtil::WriteToFile(sourcePath, content);
}
} // namespace Cangjie::Interop::ObjC
