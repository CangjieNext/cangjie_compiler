// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "gtest/gtest.h"
#include "cangjie/AST/Node.h"
#include "cangjie/CHIR/CHIR.h"
#include "cangjie/CHIR/AST2CHIR/AST2CHIRNodeMap.h"
#include "cangjie/CHIR/IR/Type/CHIRType.h"
#include "cangjie/Frontend/CompilerInstance.h"
#include "cangjie/IncrementalCompilation/CachedMangleMap.h"
#include "cangjie/Modules/PackageManager.h"
#include "cangjie/Option/Option.h"

using namespace Cangjie;

// ═══════════════════════════════════════════════════
// AST2CHIRNodeMap::Clear tests
// ═══════════════════════════════════════════════════

TEST(AST2CHIRNodeMapTest, ClearEmptiesCache)
{
    CHIR::AST2CHIRNodeMap<CHIR::CustomTypeDef> nodeMap;
    EXPECT_TRUE(nodeMap.GetALL().empty());

    // Clear on empty map should be safe.
    nodeMap.Clear();
    EXPECT_TRUE(nodeMap.GetALL().empty());
}

// ═══════════════════════════════════════════════════
// CachedMangleMap::Clear tests
// ═══════════════════════════════════════════════════

TEST(CachedMangleMapTest, ClearRemovesAllData)
{
    CachedMangleMap map;
    map.incrRemovedDecls.emplace("test_decl_1");
    map.importedInlineDecls.emplace("inline_decl_1");
    map.newExternalDecls.emplace("external_decl_1");

    EXPECT_FALSE(map.incrRemovedDecls.empty());
    EXPECT_FALSE(map.importedInlineDecls.empty());
    EXPECT_FALSE(map.newExternalDecls.empty());

    map.Clear();

    EXPECT_TRUE(map.incrRemovedDecls.empty());
    EXPECT_TRUE(map.importedInlineDecls.empty());
    EXPECT_TRUE(map.newExternalDecls.empty());
}

TEST(CachedMangleMapTest, ClearIsIdempotent)
{
    CachedMangleMap map;
    map.incrRemovedDecls.emplace("decl_1");

    map.Clear();
    EXPECT_TRUE(map.incrRemovedDecls.empty());

    // Second clear should be safe.
    map.Clear();
    EXPECT_TRUE(map.incrRemovedDecls.empty());
}

// ═══════════════════════════════════════════════════
// PackageManager::ClearASTReferences tests
// ═══════════════════════════════════════════════════

TEST(PackageManagerTest, ClearASTReferencesEmptiesMaps)
{
    DiagnosticEngine diag;
    TypeManager typeManager;
    GlobalOptions opts;
    ImportManager importManager(diag, typeManager, opts);
    PackageManager pm(importManager);

    // Add some data.
    pm.packageToOtherSccMap["pkg1"] = {"dep1", "dep2"};
    pm.packageInfoMap["pkg1"] = std::make_unique<PackageInfo>("pkg1");

    EXPECT_FALSE(pm.packageToOtherSccMap.empty());
    EXPECT_FALSE(pm.packageInfoMap.empty());

    pm.ClearASTReferences();

    EXPECT_TRUE(pm.packageToOtherSccMap.empty());
    EXPECT_TRUE(pm.packageInfoMap.empty());
}

TEST(PackageManagerTest, ClearASTReferencesIsIdempotent)
{
    DiagnosticEngine diag;
    TypeManager typeManager;
    GlobalOptions opts;
    ImportManager importManager(diag, typeManager, opts);
    PackageManager pm(importManager);

    pm.packageToOtherSccMap["pkg1"] = {"dep1"};
    pm.ClearASTReferences();
    EXPECT_TRUE(pm.packageToOtherSccMap.empty());

    // Second call should be safe.
    pm.ClearASTReferences();
    EXPECT_TRUE(pm.packageToOtherSccMap.empty());
}

// ═══════════════════════════════════════════════════
// GlobalOptions::noReleaseAST tests
// ═══════════════════════════════════════════════════

TEST(GlobalOptionsTest, NoReleaseASTDefaultsFalse)
{
    GlobalOptions opts;
    EXPECT_FALSE(opts.noReleaseAST);
}

TEST(GlobalOptionsTest, NoReleaseASTCanBeSet)
{
    GlobalOptions opts;
    opts.noReleaseAST = true;
    EXPECT_TRUE(opts.noReleaseAST);
}

// ═══════════════════════════════════════════════════
// PackageCodeGenInfo tests
// ═══════════════════════════════════════════════════

TEST(PackageCodeGenInfoTest, DefaultValues)
{
    CompilerInstance::PackageCodeGenInfo info;
    EXPECT_TRUE(info.fullPackageName.empty());
    EXPECT_FALSE(info.isEmpty);
}

TEST(PackageCodeGenInfoTest, StoresPackageName)
{
    CompilerInstance::PackageCodeGenInfo info;
    info.fullPackageName = "std.core";
    info.isEmpty = true;
    EXPECT_EQ(info.fullPackageName, "std.core");
    EXPECT_TRUE(info.isEmpty);
}

// ═══════════════════════════════════════════════════
// ImportManager::ClearASTReferences tests
// ═══════════════════════════════════════════════════

TEST(ImportManagerTest, ClearASTReferencesIsIdempotent)
{
    DiagnosticEngine diag;
    TypeManager typeManager;
    GlobalOptions opts;
    ImportManager importManager(diag, typeManager, opts);

    // First call should be safe on fresh instance.
    importManager.ClearASTReferences();

    // Second call should also be safe (idempotent).
    importManager.ClearASTReferences();
}

// ═══════════════════════════════════════════════════
// CompilerInstance state flag tests
// ═══════════════════════════════════════════════════

TEST(CompilerInstanceStateTest, ASTReleasedDefaultsFalse)
{
    DiagnosticEngine diag;
    GlobalOptions opts;
    CompilerInvocation invocation;
    invocation.globalOptions = opts;
    CompilerInstance ci(invocation, diag);
    EXPECT_FALSE(ci.IsASTReleased());
}

TEST(CompilerInstanceStateTest, ResultsSavedAccessors)
{
    DiagnosticEngine diag;
    GlobalOptions opts;
    CompilerInvocation invocation;
    invocation.globalOptions = opts;
    CompilerInstance ci(invocation, diag);

    EXPECT_FALSE(ci.IsResultsSaved());
    ci.SetResultsSaved(true);
    EXPECT_TRUE(ci.IsResultsSaved());
}

TEST(CompilerInstanceStateTest, CacheWrittenDefaultsFalse)
{
    DiagnosticEngine diag;
    GlobalOptions opts;
    CompilerInvocation invocation;
    invocation.globalOptions = opts;
    CompilerInstance ci(invocation, diag);
    EXPECT_FALSE(ci.IsCacheWritten());
}

TEST(CompilerInstanceStateTest, CodeGenPkgInfosEmptyByDefault)
{
    DiagnosticEngine diag;
    GlobalOptions opts;
    CompilerInvocation invocation;
    invocation.globalOptions = opts;
    CompilerInstance ci(invocation, diag);
    EXPECT_TRUE(ci.GetCodeGenPkgInfos().empty());
}
