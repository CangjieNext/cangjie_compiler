// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "AfterTypeCheckContext.h"
#include "cangjie/AST/Match.h"
#include "cangjie/AST/Node.h"
#include "NativeFFI/Java/Utils.h"

namespace Cangjie::Native::FFI::Java {
using namespace AST;

std::vector<Ptr<ClassLikeDecl>> AfterTypeCheckContext::GetJavaMirrors() const
{
    return javaMirrors;
}

std::vector<Ptr<ClassDecl>> AfterTypeCheckContext::GetJavaImplReferenceWrappers() const
{
    return javaImplReferenceWrappers;
}

std::vector<Ptr<ClassDecl>> AfterTypeCheckContext::GetJavaImplRegistryCompanions() const
{
    return javaImplRegistryCompanions;
}

ClassDecl& AfterTypeCheckContext::GetJavaImplRegistryCompanion(const ClassDecl& referenceWrapper) const
{
    Ptr<ClassDecl> companion;
    for (auto registryCompanion : GetJavaImplRegistryCompanions()) {
        if (registryCompanion->identifier == GetImplRegistryCompanionClassName(referenceWrapper)) {
            companion = registryCompanion;
            break;
        }
    }

    CJC_NULLPTR_CHECK(companion);
    return *companion;
}

void AfterTypeCheckContext::CacheJavaImplReferenceWrapperConstructorsPair(FuncDecl& userDefinedCtor,
    FuncDecl& generatedCtor)
{
    javaImplRefWrapperCjToJavaConstructors[&userDefinedCtor] = &generatedCtor;
}

FuncDecl& AfterTypeCheckContext::GetJavaImplReferenceWrapperGeneratedConstructor(FuncDecl& userDefinedCtor)
{
    auto javaSideCtor = javaImplRefWrapperCjToJavaConstructors[&userDefinedCtor];
    CJC_NULLPTR_CHECK(javaSideCtor);
    return *javaSideCtor;
}

void AfterTypeCheckContext::CacheJavaImplRegistryCompanionReferenceField(ClassDecl& refWrapper, VarDecl& field)
{
    javaImplRegistryCompanionRefFields[&refWrapper] = &field;
}

VarDecl& AfterTypeCheckContext::GetJavaImplRegistryCompanionReferenceField(ClassDecl& refWrapper)
{
    auto registryCompanionRefField = javaImplRegistryCompanionRefFields[&refWrapper];
    CJC_NULLPTR_CHECK(registryCompanionRefField);
    return *registryCompanionRefField;
}

void AfterTypeCheckContext::CacheJavaImplWrappingConstructor(ClassDecl& refWrapper, FuncDecl& wrappingCtor)
{
    javaImplWrappingConstructors[&refWrapper] = &wrappingCtor;
}

FuncDecl& AfterTypeCheckContext::GetJavaImplWrappingConstructor(ClassDecl& refWrapper)
{
    auto wrappingCtor = javaImplWrappingConstructors[&refWrapper];
    CJC_NULLPTR_CHECK(wrappingCtor);
    return *wrappingCtor;
}

std::vector<Ptr<FuncDecl>> AfterTypeCheckContext::GetJavaImplUserDefinedConstructors(ClassDecl& refWrapper)
{
    std::vector<Ptr<FuncDecl>> ctors;

    for (auto member : refWrapper.GetMemberDeclPtrs()) {
        if (!IsUserDefinedJavaImplConstructor(*member)) {
            continue;
        }
        ctors.emplace_back(StaticAs<ASTKind::FUNC_DECL>(member));
    }
    return ctors;
}

void AfterTypeCheckContext::CacheJavaImplSuperCtorArgNativeFunc(const FuncDecl& ctor, Ptr<FuncDecl> nativeFunc)
{
    auto inativeFunc = javaImplUserCtorToNativeFuncs.insert({ &ctor, std::vector<Ptr<FuncDecl>>{} }).first;
    inativeFunc->second.push_back(nativeFunc);
}

const std::vector<Ptr<FuncDecl>> AfterTypeCheckContext::GetJavaImplSuperCtorArgNativeFuncs(const FuncDecl& ctor)
{
    return javaImplUserCtorToNativeFuncs[&ctor];
}

bool AfterTypeCheckContext::HasDesugaredJavaImplSuperConstructorCall(const AST::FuncDecl& userCtor) const
{
    return javaImplUserCtorToNativeFuncs.find(&userCtor) != javaImplUserCtorToNativeFuncs.end();
}

void AfterTypeCheckContext::CacheJavaImplSuperCtorCall(const FuncDecl& ctor, OwnedPtr<CallExpr>&& call)
{
    javaImplUserCtorToOriginalSuperCall[&ctor] = std::move(call);
}

CallExpr& AfterTypeCheckContext::GetJavaImplSuperCtorCall(const AST::FuncDecl& ctor)
{
    return *javaImplUserCtorToOriginalSuperCall[&ctor];
}

void AfterTypeCheckContext::AddGeneratedDecl(OwnedPtr<Decl>&& decl)
{
    CJC_ASSERT(decl->outerDecl == nullptr);
    generated.emplace_back(std::move(decl));
}

void AfterTypeCheckContext::FlushGeneratedDecls()
{
    for (auto& generatedDecl : generated) {
        /*
         * Make the generated declaration visible and available via ImportManager/CjoManager.
         */
        if (generatedDecl->IsExportedDecl()) {
            importManager.GetCjoManager()->AddGeneratedDeclToDeclMap(*generatedDecl);
        }
        auto& fileDecls = generatedDecl->curFile->decls;
        fileDecls.push_back(std::move(generatedDecl));
    }
    generated.clear();
}


AfterTypeCheckContext::AfterTypeCheckContext(const ImportManager& importManager, TypeManager& typeManager,
    AST::Package& pkg) : importManager(importManager), typeManager(typeManager), pkg(pkg),
    javaMirrors(Native::FFI::Java::GetJavaMirrors(pkg)),
    javaImplReferenceWrappers(Native::FFI::Java::GetJavaImpls(pkg)),
    javaImplRegistryCompanions(Native::FFI::Java::GetJavaImplRegistryCompanions(pkg))
{
}

} // namespace Cangjie::Interop::Java
