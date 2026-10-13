// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "gtest/gtest.h"

#define private public
#include "cangjie/CHIR/AST2CHIR/AST2CHIR.h"
#undef private

using namespace Cangjie;
using namespace Cangjie::CHIR;

class AST2CHIRTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        diag.SetSourceManager(&sourceManager);
        AST2CHIR::AST2CHIRBuilder config{};
        config.SetGlobalOptions(options)
            ->SetGenericInstantiationManager(nullptr)
            ->SetImportManager(importManager)
            ->SetSourceManager(sourceManager)
            ->SetTypeManager(types)
            ->SetDiag(diag)
            ->SetIncreKind(IncreKind::INVALID)
            ->SetCachedInfo(cachedInfo)
            ->SetCHIRBuilder(builder)
            ->SetCHIRType(chirType);
        ast2chir = std::make_unique<AST2CHIR>(config);
        ast2chir->package = builder.CreatePackage("test");
        ast2chir->mergingSpecific = true;
        funcDecl.identifier = "wrapper";
        funcDecl.mangledName = "wrapper";
        funcDecl.EnableAttr(AST::Attribute::FROM_COMMON_PART, AST::Attribute::NO_MANGLE);
        SetFuncType(true);
    }

    void SetFuncType(bool isC)
    {
        funcDecl.SetTy(types.GetFunctionTy({}, TypeManager::GetPrimitiveTy(AST::TypeKind::TYPE_UNIT),
            {isC, false, false, false}));
    }

    Function* AddDeserializedFunc(const std::string& name)
    {
        auto funcTy = builder.GetType<FuncType>(std::vector<Type*>{}, builder.GetUnitTy(), false,
            funcDecl.GetTy()->IsCFunc());
        auto func = builder.CreateFunction(funcTy, name, "wrapper", "rawWrapper", "test");
        func->InitBody(*builder.CreateBlockGroup(*func));
        ast2chir->BuildDeserializedTable();
        return func;
    }

    GlobalOptions options;
    SourceManager sourceManager;
    DiagnosticEngine diag;
    TypeManager types;
    ImportManager importManager{diag, types, options};
    CompilationCache cachedInfo;
    CHIRContext context;
    CHIRBuilder builder{context};
    std::unordered_map<AST::Ty*, Type*> typeMap;
    CHIRTypeCache typeCache{typeMap};
    CHIRType chirType{builder, typeCache};
    std::unique_ptr<AST2CHIR> ast2chir;
    AST::FuncDecl funcDecl;
};

TEST_F(AST2CHIRTest, CommonCFuncUsesSuffixedIdentifier)
{
    auto func = AddDeserializedFunc("wrapper$real");
    auto body = func->GetBody();

    AST::File file;
    funcDecl.curFile = &file;
    ast2chir->CreateFuncSignatureAndSetGlobalCache(funcDecl);

    EXPECT_EQ(ast2chir->globalCache.TryGet(funcDecl), func);
    EXPECT_FALSE(ast2chir->NeedTranslate(funcDecl));
    EXPECT_EQ(func->GetBody(), body);
    EXPECT_TRUE(ast2chir->CheckCommonPartDeclsDeserialized());
}

TEST_F(AST2CHIRTest, MissingCommonCFuncIsRecorded)
{
    EXPECT_EQ(ast2chir->TryGetDeserialized<Function>(funcDecl), nullptr);
    EXPECT_EQ(ast2chir->missingCommonPartDecls.count(&funcDecl), 1);
}

TEST_F(AST2CHIRTest, CommonNonCFuncDoesNotUseSuffixedIdentifier)
{
    SetFuncType(false);
    AddDeserializedFunc("wrapper$real");

    EXPECT_EQ(ast2chir->TryGetDeserialized<Function>(funcDecl), nullptr);
    EXPECT_EQ(ast2chir->missingCommonPartDecls.count(&funcDecl), 1);
}

TEST_F(AST2CHIRTest, MangledCommonCFuncDoesNotUseSuffixedIdentifier)
{
    funcDecl.DisableAttr(AST::Attribute::NO_MANGLE);
    AddDeserializedFunc("wrapper$real");

    EXPECT_EQ(ast2chir->TryGetDeserialized<Function>(funcDecl), nullptr);
    EXPECT_EQ(ast2chir->missingCommonPartDecls.count(&funcDecl), 1);
}

TEST_F(AST2CHIRTest, ForeignCommonCFuncUsesRawMangledName)
{
    funcDecl.EnableAttr(AST::Attribute::FOREIGN);
    funcDecl.rawMangleName = "rawWrapper";
    auto func = AddDeserializedFunc("wrapper");
    func->EnableAttr(Attribute::FOREIGN);
    ast2chir->deserializedVals.clear();
    ast2chir->BuildDeserializedTable();

    EXPECT_EQ(ast2chir->TryGetDeserialized<Function>(funcDecl), func);
}
