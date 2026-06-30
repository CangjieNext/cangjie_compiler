// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * Test cases for incremental import for LSP.
 */
#include "gtest/gtest.h"

#include "TestCompilerInstance.h"
#include "cangjie/AST/Walker.h"
#include "cangjie/Parse/Parser.h"
#include "cangjie/Utils/CastingTemplate.h"

using namespace Cangjie;
using namespace AST;

#include "ModuleTestBase.h"

class ImportIncrTest : public ModuleTestBase {
protected:
    void BeforeCompileSingleFile() override
    {
        instance->importManager->SetPackageCjoCache("std.core", allAstData["std.core"]);
    }
};

namespace {
bool FindDeclInPackage(
    const std::vector<std::pair<std::string, std::vector<Ptr<Decl>>>>& decls, const std::string& packageName)
{
    for (auto& [name, decls] : decls) {
        for (auto& decl : decls) {
            if (decl->fullPackageName == packageName) {
                return true;
            }
        }
    }
    return false;
}

void SetupPackageCaches(TestCompilerInstance& instance,
                        const std::unordered_map<std::string, std::vector<uint8_t>>& allAstData,
                        const std::vector<std::string>& fileNames)
{
    instance.importManager->SetPackageCjoCache("std.core", allAstData.at("std.core"));
    for (const auto& fileName : fileNames) {
        instance.importManager->SetPackageCjoCache(fileName, allAstData.at(fileName));
    }
}

void VerifyInitialCompile(TestCompilerInstance& instance, Ptr<Package>& curPkg)
{
    bool ret = instance.Compile(CompileStage::IMPORT_PACKAGE);
    EXPECT_TRUE(ret);
    auto depPkgs = instance.importManager->GetAllImportedPackages();
    size_t depPkgsSize = 4; // std.core, vardecl, funcdecl, default package
    EXPECT_EQ(depPkgs.size(), depPkgsSize);
    EXPECT_FALSE(instance.GetSourcePackages().empty());
    curPkg = instance.GetSourcePackages()[0];
    CJC_NULLPTR_CHECK(curPkg);
    EXPECT_FALSE(curPkg->files.empty());
    auto decls = instance.importManager->GetImportedDecls(*curPkg->files.front());
    EXPECT_FALSE(decls.empty());
    bool find = FindDeclInPackage(decls, "classdecl");
    EXPECT_FALSE(find);
}

void VerifyIncrementalCompile(TestCompilerInstance& instance, Ptr<Package>& curPkg)
{
    bool ret = instance.Compile(CompileStage::IMPORT_PACKAGE);
    EXPECT_TRUE(ret);
    auto depPkgs = instance.importManager->GetAllImportedPackages();
    size_t depPkgsSize = 5; // std.core, vardecl, funcdecl, default package, classdecl
    EXPECT_EQ(depPkgs.size(), depPkgsSize);
    auto decls = instance.importManager->GetImportedDecls(*curPkg->files.front());
    EXPECT_FALSE(decls.empty());
    bool find = FindDeclInPackage(decls, "classdecl");
    EXPECT_TRUE(find);
}
}

TEST_F(ImportIncrTest, ImportIncrTest)
{
    diag.ClearError();
    instance = std::make_unique<TestCompilerInstance>(invocation, diag);
    instance->loadSrcFilesFromCache = true;
    SetupPackageCaches(*instance, allAstData, fileNames);
    std::string middleCode = R"(
        import vardecl.*
        import funcdecl.*

        main() {}
    )";
    instance->bufferCache.insert(
        {srcPath + "middle.cj",
         CompilerInstance::SrcCodeCacheInfo(CompilerInstance::SrcCodeChangeState::ADDED, middleCode)});
    Ptr<Package> curPkg;
    VerifyInitialCompile(*instance, curPkg);
    SetupPackageCaches(*instance, allAstData, fileNames);
    std::string updatedMiddleCode = R"(
        import vardecl.*
        import funcdecl.*
        import classdecl.*

        main() {}
    )";
    instance->bufferCache[srcPath + "middle.cj"] =
        CompilerInstance::SrcCodeCacheInfo(CompilerInstance::SrcCodeChangeState::CHANGED, updatedMiddleCode);
    VerifyIncrementalCompile(*instance, curPkg);
}