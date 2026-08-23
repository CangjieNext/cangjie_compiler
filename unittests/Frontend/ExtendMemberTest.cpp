// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * Covers the extension-lookup entry points used by the LSP: `GetExtendDecls` and
 * `GetAllVisibleExtendMembers`. Both need a semantically analysed package, so this file carries
 * the heavier fixture (standard library on CANGJIE_HOME/CANGJIE_PATH) on its own.
 */

#include <cstdlib>
#include <memory>
#include <string>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#define CJ_ENVIRON _environ
#elif defined(__APPLE__)
#include <crt_externs.h>
#define CJ_ENVIRON (*_NSGetEnviron())
#else
#include <unistd.h>
#define CJ_ENVIRON environ
#endif

#include "gtest/gtest.h"

#include "TestCompilerInstance.h"
#include "cangjie/AST/Match.h"
#include "cangjie/AST/Searcher.h"
#include "cangjie/Frontend/CompilerInstance.h"
#include "cangjie/Macro/InvokeUtil.h"
#include "cangjie/Utils/FileUtil.h"

using namespace Cangjie;
using namespace Cangjie::AST;

namespace {
std::string ProjectPath()
{
#ifdef PROJECT_SOURCE_DIR
    return PROJECT_SOURCE_DIR;
#else
    return "..";
#endif
}

std::unordered_map<std::string, std::string> GetEnvironmentVars()
{
    std::unordered_map<std::string, std::string> envVars;
    char** env = CJ_ENVIRON;
    while (env && *env) {
        std::string entry(*env);
        size_t pos = entry.find('=');
        if (pos != std::string::npos) {
            envVars[entry.substr(0, pos)] = entry.substr(pos + 1);
        }
        ++env;
    }
    return envVars;
}
} // namespace

/**
 * `GetExtendDecls` / `GetAllVisibleExtendMembers` are the LSP-facing entry points for extension
 * lookup. They need a semantically analysed package, so this fixture points the instance at the
 * built standard library the same way the Sema unit tests do.
 */
class ExtendMemberTest : public ::testing::Test {
protected:
    void SetUp() override
    {
#ifdef __x86_64__
        invocation.globalOptions.target.arch = Cangjie::Triple::ArchType::X86_64;
#else
        invocation.globalOptions.target.arch = Cangjie::Triple::ArchType::AARCH64;
#endif
#ifdef _WIN32
        invocation.globalOptions.target.os = Cangjie::Triple::OSType::WINDOWS;
#elif defined(__APPLE__)
        invocation.globalOptions.target.os = Cangjie::Triple::OSType::DARWIN;
#elif defined(__unix__)
        invocation.globalOptions.target.os = Cangjie::Triple::OSType::LINUX;
#endif
        const std::string cangjieHome = ProjectPath() + "/output";
        const std::string cangjiePath =
            cangjieHome + "/modules/" + invocation.globalOptions.GetCangjieLibTargetPathName();
        if (const char* oldHome = getenv("CANGJIE_HOME"); oldHome != nullptr) {
            savedHome = oldHome;
        }
        if (const char* oldPath = getenv("CANGJIE_PATH"); oldPath != nullptr) {
            savedPath = oldPath;
        }
#ifdef _WIN32
        _putenv_s("CANGJIE_HOME", cangjieHome.c_str());
        _putenv_s("CANGJIE_PATH", cangjiePath.c_str());
#else
        setenv("CANGJIE_HOME", cangjieHome.c_str(), 1);
        setenv("CANGJIE_PATH", cangjiePath.c_str(), 1);
#endif
        // A library build, so the package under test does not need a `main`.
        invocation.globalOptions.compilePackage = true;
        invocation.globalOptions.outputMode = GlobalOptions::OutputMode::STATIC_LIB;
        invocation.globalOptions.ReadPathsFromEnvironmentVars(GetEnvironmentVars());
        Cangjie::MacroProcMsger::GetInstance().CloseMacroSrv();
        instance = std::make_unique<TestCompilerInstance>(invocation, diag);
        // Extension lookup needs a semantically analysed package, which needs the core package.
        // Skip rather than fail where the standard library has not been built yet.
        stdlibAvailable = FileUtil::FileExist(FileUtil::JoinPath(cangjiePath, "std.cjo"));
    }

    void TearDown() override
    {
        instance.reset();
        // A variable that was not set before must be removed again, not set to "" - other tests
        // in this binary derive paths from it and the two are not the same thing.
#ifdef _WIN32
        _putenv_s("CANGJIE_HOME", savedHome.c_str());
        _putenv_s("CANGJIE_PATH", savedPath.c_str());
#else
        if (savedHome.empty()) {
            unsetenv("CANGJIE_HOME");
        } else {
            setenv("CANGJIE_HOME", savedHome.c_str(), 1);
        }
        if (savedPath.empty()) {
            unsetenv("CANGJIE_PATH");
        } else {
            setenv("CANGJIE_PATH", savedPath.c_str(), 1);
        }
#endif
    }

    CompilerInvocation invocation;
    DiagnosticEngine diag;
    std::unique_ptr<TestCompilerInstance> instance;
    std::string savedHome;
    std::string savedPath;
    bool stdlibAvailable = false;
};

TEST_F(ExtendMemberTest, VisibleExtendMembersComeFromExtensionsAndTheirInterfaces)
{
    if (!stdlibAvailable) {
        GTEST_SKIP() << "standard library not built under " << ProjectPath() << "/output/modules";
    }
    instance->code = R"(
        package pkg1
        interface Eqq {
            func g(): Unit
        }
        extend Int64 <: Eqq {
            public func g(): Unit {}
        }
        class A <: Eqq {
            public func g(): Unit {}
        }
        extend A {
            public func h(): Unit {}
        }
    )";
    instance->Compile(CompileStage::SEMA);
    auto pkgs = instance->GetSourcePackages();
    ASSERT_EQ(pkgs.size(), 1U);
    ASTContext* ctx = instance->GetASTContextByPackage(pkgs[0]);
    ASSERT_NE(ctx, nullptr);

    Searcher searcher;
    auto extendSyms = searcher.Search(*ctx, "ast_kind:extend_decl", Sort::posAsc);
    ASSERT_FALSE(extendSyms.empty());
    auto extendDecl = StaticAs<ASTKind::EXTEND_DECL>(extendSyms[0]->node);
    ASSERT_NE(extendDecl->extendedType, nullptr);

    // Looked up by Ty: `extend Int64 <: Eqq` contributes `g`.
    auto byTy = instance->GetAllVisibleExtendMembers(
        extendDecl->extendedType->GetTy(), *extendSyms[0]->node->curFile);
    bool foundG = false;
    for (auto member : byTy) {
        EXPECT_TRUE(member->astKind == ASTKind::FUNC_DECL || member->astKind == ASTKind::PROP_DECL);
        foundG = foundG || member->identifier.Val() == "g";
    }
    EXPECT_TRUE(foundG);
    EXPECT_FALSE(instance->GetExtendDecls(extendDecl->extendedType->GetTy()).empty());

    // Looked up by declaration: `extend A` contributes `h`.
    auto classSyms = searcher.Search(*ctx, "(ast_kind:class_decl && name:A)");
    ASSERT_FALSE(classSyms.empty());
    auto classDecl = RawStaticCast<InheritableDecl*>(classSyms[0]->node);
    auto byDecl = instance->GetAllVisibleExtendMembers(classDecl, *extendSyms[0]->node->curFile);
    bool foundH = false;
    for (auto member : byDecl) {
        EXPECT_TRUE(member->astKind == ASTKind::FUNC_DECL || member->astKind == ASTKind::PROP_DECL);
        foundH = foundH || member->identifier.Val() == "h";
    }
    EXPECT_TRUE(foundH);
    EXPECT_FALSE(instance->GetExtendDecls(classDecl).empty());
}
