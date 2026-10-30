// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>

#include "Utils/CGCommonDef.h"
#include "cangjie/Option/Option.h"
#include "cangjie/CHIR/IR/CHIRBuilder.h"
#include "cangjie/CHIR/IR/Expression/Terminator.h"
#include "cangjie/CHIR/IR/Package.h"
#include "cangjie/CHIR/IR/Type/CustomTypeDef.h"

using namespace Cangjie;
using namespace Cangjie::CHIR;
using namespace Cangjie::CodeGen;

/**
 * @brief Tests for the predicate that decides whether a symbol defined in the current package may be
 * localized in the generated object file.
 *
 * Every case of the rule table is covered on both sides: the symbol that must be localized (a top
 * level `internal` declaration) and the ones that must keep their global binding because another
 * package, another language runtime or the incremental build may still reference them.
 */
class CGPackageInternalSymbolTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        nameMap = std::make_unique<std::unordered_map<unsigned int, std::string>>();
        cctx = std::make_unique<CHIRContext>(nameMap.get());
        builder = std::make_unique<CHIRBuilder>(*cctx);
        package = builder->CreatePackage("default");
    }

    Function* CreateFunc(const std::string& name)
    {
        auto* funcTy = builder->GetType<FuncType>(std::vector<Type*>{}, builder->GetUnitTy());
        auto* func = builder->CreateFunction(funcTy, name, name, "", "default");
        auto* body = builder->CreateBlockGroup(*func);
        func->InitBody(*body);
        auto* block = builder->CreateBlock(body);
        body->SetEntryBlock(block);
        block->AppendExpression(builder->CreateTerminator<Exit>(block));
        return func;
    }

    GlobalVar* CreateGlobalVar(const std::string& name)
    {
        return builder->CreateGlobalVar(builder->GetInt64Ty(), name, name, "", "default");
    }

    std::unique_ptr<std::unordered_map<unsigned int, std::string>> nameMap;
    std::unique_ptr<CHIRContext> cctx;
    std::unique_ptr<CHIRBuilder> builder;
    Package* package{nullptr};
};

TEST_F(CGPackageInternalSymbolTest, TopLevelInternalFunctionIsLocalizable)
{
    auto* func = CreateFunc("helper");
    ASSERT_EQ(func->Get<LinkTypeInfo>(), Linkage::EXTERNAL);
    EXPECT_TRUE(IsPackageInternalSymbol(*func));
}

TEST_F(CGPackageInternalSymbolTest, TopLevelInternalGlobalVarIsLocalizable)
{
    auto* gv = CreateGlobalVar("state");
    ASSERT_EQ(gv->Get<LinkTypeInfo>(), Linkage::EXTERNAL);
    EXPECT_TRUE(IsPackageInternalSymbol(*gv));
}

TEST_F(CGPackageInternalSymbolTest, PublicFunctionIsExported)
{
    auto* func = CreateFunc("api");
    func->EnableAttr(Attribute::PUBLIC);
    EXPECT_FALSE(IsPackageInternalSymbol(*func));
}

TEST_F(CGPackageInternalSymbolTest, ProtectedFunctionIsExported)
{
    // A `protected` member is reachable from a subclass living in another package.
    auto* func = CreateFunc("hook");
    func->EnableAttr(Attribute::PROTECTED);
    EXPECT_FALSE(IsPackageInternalSymbol(*func));
}

TEST_F(CGPackageInternalSymbolTest, SrcExportedFunctionIsNamedFromAnotherPackage)
{
    // Sema sets this attribute on the declarations another package may name: the ones whose body is
    // exported to the `cjo` and can be instantiated or inlined there, because the inlined body still
    // refers to them by their mangled name, and the ones the compiler itself calls.
    auto* func = CreateFunc("helper");
    func->EnableAttr(Attribute::SRC_EXPORTED);
    EXPECT_FALSE(IsPackageInternalSymbol(*func));
}

TEST_F(CGPackageInternalSymbolTest, SrcExportedGlobalVarIsNamedFromAnotherPackage)
{
    auto* gv = CreateGlobalVar("state");
    gv->EnableAttr(Attribute::SRC_EXPORTED);
    EXPECT_FALSE(IsPackageInternalSymbol(*gv));
}

TEST_F(CGPackageInternalSymbolTest, ImportedDeclarationHasNoDefinition)
{
    auto* func = CreateFunc("imported");
    func->EnableAttr(Attribute::IMPORTED);
    EXPECT_FALSE(IsPackageInternalSymbol(*func));
}

TEST_F(CGPackageInternalSymbolTest, JavaExportedFunctionIsLookedUpByName)
{
    auto* func = CreateFunc("javaApi");
    func->EnableAttr(Attribute::JAVA_EXPORT);
    EXPECT_FALSE(IsPackageInternalSymbol(*func));
}

TEST_F(CGPackageInternalSymbolTest, ObjCExportedFunctionIsLookedUpByName)
{
    auto* func = CreateFunc("objcApi");
    func->EnableAttr(Attribute::OBJ_C_EXPORT);
    EXPECT_FALSE(IsPackageInternalSymbol(*func));
}

TEST_F(CGPackageInternalSymbolTest, CompilerGeneratedFunctionIsPartOfTheAbi)
{
    // The `toAny` helpers, the `@C` wrappers and the default parameter functions are referenced
    // from other packages through the imported declaration.
    auto* func = CreateFunc("wrapper");
    func->EnableAttr(Attribute::COMPILER_ADD);
    EXPECT_FALSE(IsPackageInternalSymbol(*func));
}

TEST_F(CGPackageInternalSymbolTest, InitializerIsCalledByDependentPackages)
{
    auto* func = CreateFunc("initialize");
    func->EnableAttr(Attribute::INITIALIZER);
    EXPECT_FALSE(IsPackageInternalSymbol(*func));
}

TEST_F(CGPackageInternalSymbolTest, NonRecompiledSymbolIsLeftToTheIncrementalBuild)
{
    auto* func = CreateFunc("cached");
    func->EnableAttr(Attribute::NON_RECOMPILE);
    EXPECT_FALSE(IsPackageInternalSymbol(*func));
}

TEST_F(CGPackageInternalSymbolTest, MemberFunctionIsNotLocalizable)
{
    // The members of a custom type are reachable from another package through the type metadata and
    // through the generic instantiation generated there, so the first step of the change keeps them.
    auto* classDef = builder->CreateClass(INVALID_LOCATION, "C", "C", "default", true, false);
    auto* classTy = builder->GetType<ClassType>(classDef);
    classDef->SetType(*classTy);
    auto* method = CreateFunc("m");
    classDef->AddMethod(method);
    ASSERT_EQ(method->GetParentCustomTypeDef(), classDef);
    EXPECT_FALSE(IsPackageInternalSymbol(*method));
}

TEST_F(CGPackageInternalSymbolTest, InternalLinkageIsAlreadyLocal)
{
    auto* func = CreateFunc("local");
    func->Set<LinkTypeInfo>(Linkage::INTERNAL);
    EXPECT_FALSE(IsPackageInternalSymbol(*func));
}

TEST_F(CGPackageInternalSymbolTest, WeakODRIsMergedAcrossDsos)
{
    auto* func = CreateFunc("weak");
    func->Set<LinkTypeInfo>(Linkage::WEAK_ODR);
    EXPECT_FALSE(IsPackageInternalSymbol(*func));
}

TEST_F(CGPackageInternalSymbolTest, LinkOnceODRIsMergedAcrossDsos)
{
    auto* func = CreateFunc("linkonce");
    func->Set<LinkTypeInfo>(Linkage::LINKONCE_ODR);
    EXPECT_FALSE(IsPackageInternalSymbol(*func));
}

TEST_F(CGPackageInternalSymbolTest, ExternalWeakStaysResolvable)
{
    auto* func = CreateFunc("maybeImported");
    func->Set<LinkTypeInfo>(Linkage::EXTERNAL_WEAK);
    EXPECT_FALSE(IsPackageInternalSymbol(*func));
}

namespace {
GlobalOptions MakeOptions(GlobalOptions::OutputMode mode)
{
    GlobalOptions options;
    options.outputMode = mode;
    return options;
}
} // namespace

TEST(IsFinalLinkUnitForPackageInternalSymbolsTest, ExecutableIsFinal)
{
    EXPECT_TRUE(IsFinalLinkUnitForPackageInternalSymbols(
        MakeOptions(GlobalOptions::OutputMode::EXECUTABLE)));
}

TEST(IsFinalLinkUnitForPackageInternalSymbolsTest, SharedLibraryIsFinal)
{
    EXPECT_TRUE(IsFinalLinkUnitForPackageInternalSymbols(
        MakeOptions(GlobalOptions::OutputMode::SHARED_LIB)));
}

TEST(IsFinalLinkUnitForPackageInternalSymbolsTest, StaticLibraryIsNotFinal)
{
    // The archive is an input to a later link step that may name the symbols from another object
    // file. Localizing them here left ~845 strong symbols in the stdx archive and broke the link
    // with duplicate definitions, see the Linux CI of !2336.
    EXPECT_FALSE(IsFinalLinkUnitForPackageInternalSymbols(
        MakeOptions(GlobalOptions::OutputMode::STATIC_LIB)));
}

TEST(IsFinalLinkUnitForPackageInternalSymbolsTest, ObjectFileIsNotFinal)
{
    EXPECT_FALSE(IsFinalLinkUnitForPackageInternalSymbols(MakeOptions(GlobalOptions::OutputMode::OBJ)));
}

TEST(IsFinalLinkUnitForPackageInternalSymbolsTest, ExportForTestKeepsGlobalBinding)
{
    auto options = MakeOptions(GlobalOptions::OutputMode::SHARED_LIB);
    options.exportForTest = true;
    EXPECT_FALSE(IsFinalLinkUnitForPackageInternalSymbols(options));
}

TEST(IsFinalLinkUnitForPackageInternalSymbolsTest, MockKeepsGlobalBinding)
{
    auto options = MakeOptions(GlobalOptions::OutputMode::SHARED_LIB);
    options.mock = MockMode::ON;
    EXPECT_FALSE(IsFinalLinkUnitForPackageInternalSymbols(options));
    options.mock = MockMode::RUNTIME_ERROR;
    EXPECT_FALSE(IsFinalLinkUnitForPackageInternalSymbols(options));
    options.mock = MockMode::DEFAULT;
    EXPECT_TRUE(IsFinalLinkUnitForPackageInternalSymbols(options));
}
