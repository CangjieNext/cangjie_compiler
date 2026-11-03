// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "gtest/gtest.h"
#include "cangjie/Option/Option.h"
#include "cangjie/Driver/Driver.h"
#include "cangjie/Utils/FileUtil.h"

#include <algorithm>
#include <memory>

using namespace Cangjie;

class OptionTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        optTbl = CreateOptionTable();
        gblOpts = std::make_unique<GlobalOptions>();
#ifdef PROJECT_SOURCE_DIR
        // Gets the absolute path of the project from the compile parameter.

        std::string projectPath = PROJECT_SOURCE_DIR;
#else
        // Just in case, give it a default value.
        // Assume the initial is in the build directory.
        std::string projectPath = "..";
#endif
#ifdef _WIN32
        srcFile = FileUtil::JoinPath(projectPath, "unittests\\Option\\main.cj");
#else
        srcFile = FileUtil::JoinPath(projectPath, "unittests/Option/main.cj");
#endif
    }
    std::unique_ptr<OptionTable> optTbl;
    std::unique_ptr<GlobalOptions> gblOpts;
    std::string srcFile;
};

TEST_F(OptionTest, GlobalOptionParseFromArgsTest)
{
    std::vector<std::string> argStrs = {"cjc", "--no-prelude", "--output-type", "staticlib", srcFile};
    ArgList argList;
    bool succ = optTbl->ParseArgs(argStrs, argList);
    EXPECT_TRUE(succ);

    succ = gblOpts->ParseFromArgs(argList);
    EXPECT_TRUE(succ);
    EXPECT_EQ(gblOpts->outputMode, GlobalOptions::OutputMode::STATIC_LIB);
}

TEST_F(OptionTest, CompatVersionOptionTest)
{
    std::vector<std::string> argStrs = {"cjc", "--compat-version", "1.3", srcFile};
    ArgList argList;
    bool succ = optTbl->ParseArgs(argStrs, argList);
    EXPECT_TRUE(succ);

    succ = gblOpts->ParseFromArgs(argList);
    EXPECT_TRUE(succ);
    ASSERT_TRUE(gblOpts->compatVersion.has_value());
    EXPECT_EQ(gblOpts->compatVersion->major, 1U);
    EXPECT_EQ(gblOpts->compatVersion->minor, 3U);
    EXPECT_TRUE(gblOpts->IsCompatModeEnabled());
    EXPECT_TRUE(gblOpts->IsCompatAtLeast(1, 3));
    EXPECT_FALSE(gblOpts->IsCompatBefore(1, 3));
    EXPECT_TRUE(gblOpts->IsChangeEnabled(CompatChange::TYPE_ALIAS_ILLEGAL_TYPE_ARG_CHECK));
}

TEST_F(OptionTest, InvalidCompatVersionOptionTest)
{
    const std::vector<std::string> invalidVersions = {"1.1", "1.2", "1.3.0", "1.4", "abc"};
    for (const auto& version : invalidVersions) {
        ArgList argList;
        EXPECT_FALSE(optTbl->ParseArgs({"cjc", "--compat-version", version, srcFile}, argList));
    }
    ArgList argList;
    EXPECT_FALSE(optTbl->ParseArgs({"cjc", "--compat-version"}, argList));
}

TEST_F(OptionTest, SupportedCompatVersionsTest)
{
    const auto& versions = GlobalOptions::GetSupportedCompatVersions();
    ASSERT_EQ(versions.size(), 1U);
    EXPECT_EQ(versions[0], (VersionInfo{1, 3, 0, true}));
    EXPECT_TRUE(GlobalOptions::IsSupportedCompatVersion(1, 3));
    EXPECT_FALSE(GlobalOptions::IsSupportedCompatVersion(1, 2));
}

TEST_F(OptionTest, DuplicateCompatVersionOptionTest)
{
    std::vector<std::string> argStrs = {
        "cjc", "--compat-version", "1.3", "--compat-version", "1.3", srcFile};
    ArgList argList;
    bool succ = optTbl->ParseArgs(argStrs, argList);
    EXPECT_TRUE(succ);
    EXPECT_TRUE(gblOpts->ParseFromArgs(argList));

    auto frontendOptions = gblOpts->GenerateFrontendOptions();
    EXPECT_EQ(std::count(frontendOptions.begin(), frontendOptions.end(), "--compat-version"), 2U);
}

TEST_F(OptionTest, CompatChangePolicyTest)
{
    EXPECT_TRUE(gblOpts->IsChangeEnabled(CompatChange::TYPE_ALIAS_ILLEGAL_TYPE_ARG_CHECK));

    gblOpts->compatVersion = VersionInfo{1, 2, 0, true};
    EXPECT_FALSE(gblOpts->IsChangeEnabled(CompatChange::TYPE_ALIAS_ILLEGAL_TYPE_ARG_CHECK));

    gblOpts->compatVersion = VersionInfo{1, 3, 0, true};
    EXPECT_TRUE(gblOpts->IsChangeEnabled(CompatChange::TYPE_ALIAS_ILLEGAL_TYPE_ARG_CHECK));
}

TEST_F(OptionTest, CompatVersionChangesCachePath)
{
    gblOpts->compilationCachedPath = ".";
    gblOpts->srcFiles = {srcFile};
    const auto defaultPath = gblOpts->GenerateNamesOfCachedDirAndFile("test");

    gblOpts->compatVersion = VersionInfo{1, 3, 0, true};
    const auto compatPath = gblOpts->GenerateNamesOfCachedDirAndFile("test");
    EXPECT_NE(defaultPath, compatPath);
}

TEST_F(OptionTest, PgoOptionsTest01)
{
    std::vector<std::string> argStrs = {"cjc", "--pgo-instr-gen=cj.profraw", srcFile};
    ArgList argList;
    bool succ = optTbl->ParseArgs(argStrs, argList);
    EXPECT_TRUE(succ);

    succ = gblOpts->ParseFromArgs(argList);
    EXPECT_TRUE(succ);
    EXPECT_EQ(gblOpts->pgoProfileFile, "cj.profraw");
}

TEST_F(OptionTest, PgoOptionsTest02)
{
    std::vector<std::string> argStrs = {"cjc", "--pgo-instr-gen", "--target=aarch64-linux-ohos"};
    ArgList argList;
    bool succ = optTbl->ParseArgs(argStrs, argList);
    EXPECT_TRUE(succ);

    succ = gblOpts->ParseFromArgs(argList);
    EXPECT_FALSE(succ);
}

TEST_F(OptionTest, NoArgsTest)
{
    // Nothing input.
    std::vector<std::string> argStrs = {"cjc", "--no-prelude"};
    DiagnosticEngine diag;
    std::unique_ptr<Driver> driver = std::make_unique<Driver>(argStrs, diag, "cjc");
    bool succ = driver->ParseArgs();
    EXPECT_TRUE(succ);

    succ = driver->ExecuteCompilation();
    EXPECT_FALSE(succ);
}

TEST_F(OptionTest, InvalidArgsTest)
{
    // Something doesn't exist.
    std::vector<std::string> argStrs = {"cjc", "--no-prelude", "c"};
    ArgList argList;
    bool succ = optTbl->ParseArgs(argStrs, argList);
    EXPECT_TRUE(succ);

    succ = gblOpts->ParseFromArgs(argList);
    EXPECT_FALSE(succ);
}

#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
TEST_F(OptionTest, StackTraceFormatTest)
{
    {
        std::vector<std::string> argStrs = {"cjc"};
        ArgList argList;
        bool succ = optTbl->ParseArgs(argStrs, argList);
        EXPECT_TRUE(succ);
        succ = gblOpts->ParseFromArgs(argList);
        EXPECT_TRUE(gblOpts->stackTraceFmt == GlobalOptions::StackTraceFormat::DEFAULT);
    }
    {
        std::vector<std::string> argStrs = {"cjc", "--stack-trace-format", "default"};
        ArgList argList;
        bool succ = optTbl->ParseArgs(argStrs, argList);
        EXPECT_TRUE(succ);
        succ = gblOpts->ParseFromArgs(argList);
        EXPECT_TRUE(gblOpts->stackTraceFmt == GlobalOptions::StackTraceFormat::DEFAULT);
    }
    {
        std::vector<std::string> argStrs = {"cjc", "--stack-trace-format", "simple"};
        ArgList argList;
        bool succ = optTbl->ParseArgs(argStrs, argList);
        EXPECT_TRUE(succ);
        succ = gblOpts->ParseFromArgs(argList);
        EXPECT_TRUE(gblOpts->stackTraceFmt == GlobalOptions::StackTraceFormat::SIMPLE);
    }
    {
        std::vector<std::string> argStrs = {"cjc", "--stack-trace-format", "all"};
        ArgList argList;
        bool succ = optTbl->ParseArgs(argStrs, argList);
        EXPECT_TRUE(succ);
        succ = gblOpts->ParseFromArgs(argList);
        EXPECT_TRUE(gblOpts->stackTraceFmt == GlobalOptions::StackTraceFormat::ALL);
    }
    {
        std::vector<std::string> argStrs = {"cjc", "--stack-trace-format"};
        ArgList argList;
        bool succ = optTbl->ParseArgs(argStrs, argList);
        EXPECT_FALSE(succ);
    }
}
#endif

TEST_F(OptionTest, EmptyModuleNameTest)
{
    // NOTE: "--module-name" is deprecated.
    std::vector<std::string> argStrs = {"cjc", "--module-name"};
    ArgList argList;
    bool succ = optTbl->ParseArgs(argStrs, argList);
    EXPECT_FALSE(succ);
}

TEST(IsPossibleMatchingTripleNameTest, VaildTripleName1)
{
    Triple::Info t = {
        Triple::ArchType::X86_64, Triple::Vendor::UNKNOWN, Triple::OSType::LINUX, Triple::Environment::GNU};
    std::string s = "x86_64-pc-linux-gnu";
    bool res = Triple::IsPossibleMatchingTripleName(t, s);

    EXPECT_TRUE(res);
}

TEST(IsPossibleMatchingTripleNameTest, VaildTripleName2)
{
    Triple::Info t = {
        Triple::ArchType::X86_64, Triple::Vendor::UNKNOWN, Triple::OSType::LINUX, Triple::Environment::GNU};
    std::string s = "x86_64-unknown-linux-gnu";
    bool res = Triple::IsPossibleMatchingTripleName(t, s);

    EXPECT_TRUE(res);
}

TEST(IsPossibleMatchingTripleNameTest, VaildTripleName3)
{
    Triple::Info t = {
        Triple::ArchType::X86_64, Triple::Vendor::UNKNOWN, Triple::OSType::LINUX, Triple::Environment::GNU};
    std::string s = "x86_64-linux-gnu";
    bool res = Triple::IsPossibleMatchingTripleName(t, s);

    EXPECT_TRUE(res);
}

TEST(IsPossibleMatchingTripleNameTest, InvaildTripleName1)
{
    Triple::Info t = {
        Triple::ArchType::X86_64, Triple::Vendor::UNKNOWN, Triple::OSType::LINUX, Triple::Environment::GNU};
    std::string s = "aarch64-linux-gnu";
    bool res = Triple::IsPossibleMatchingTripleName(t, s);

    EXPECT_FALSE(res);
}

TEST(IsPossibleMatchingTripleNameTest, InvaildTripleName2)
{
    Triple::Info t = {
        Triple::ArchType::X86_64, Triple::Vendor::UNKNOWN, Triple::OSType::LINUX, Triple::Environment::GNU};
    std::string s = "notatriplename";
    bool res = Triple::IsPossibleMatchingTripleName(t, s);

    EXPECT_FALSE(res);
}

TEST(ToFullTripleStringTest, ArchNameTest)
{
    std::vector<std::pair<Triple::Info, std::string>> pairs = {
        {{Triple::ArchType::X86_64, Triple::Vendor::UNKNOWN, Triple::OSType::LINUX, Triple::Environment::GNU},
            "x86_64-unknown-linux-gnu"},
        {{Triple::ArchType::AARCH64, Triple::Vendor::UNKNOWN, Triple::OSType::LINUX, Triple::Environment::GNU},
            "aarch64-unknown-linux-gnu"},
        {{Triple::ArchType::UNKNOWN, Triple::Vendor::UNKNOWN, Triple::OSType::LINUX, Triple::Environment::GNU},
            "unknown-unknown-linux-gnu"}};
    for (size_t i = 0; i < pairs.size(); i++) {
        auto actualName = pairs[i].first.ToFullTripleString();
        EXPECT_EQ(actualName, pairs[i].second);
    }
}

TEST(ToFullTripleStringTest, VendorNameTest)
{
    std::vector<std::pair<Triple::Info, std::string>> pairs = {
        {{Triple::ArchType::X86_64, Triple::Vendor::UNKNOWN, Triple::OSType::LINUX, Triple::Environment::GNU},
            "x86_64-unknown-linux-gnu"},
        {{Triple::ArchType::X86_64, Triple::Vendor::PC, Triple::OSType::UNKNOWN, Triple::Environment::NOT_AVAILABLE},
            "x86_64-pc-unknown"},
    };
    for (size_t i = 0; i < pairs.size(); i++) {
        auto actualName = pairs[i].first.ToFullTripleString();
        EXPECT_EQ(actualName, pairs[i].second);
    }
}

TEST(ToFullTripleStringTest, OSNameTest)
{
    std::vector<std::pair<Triple::Info, std::string>> pairs = {
        {{Triple::ArchType::X86_64, Triple::Vendor::UNKNOWN, Triple::OSType::LINUX, Triple::Environment::GNU},
            "x86_64-unknown-linux-gnu"}};
    for (size_t i = 0; i < pairs.size(); i++) {
        auto actualName = pairs[i].first.ToFullTripleString();
        EXPECT_EQ(actualName, pairs[i].second);
    }
}

TEST(ToFullTripleStringTest, EnvironmentNameTest)
{
    std::vector<std::pair<Triple::Info, std::string>> pairs = {
        {{Triple::ArchType::X86_64, Triple::Vendor::UNKNOWN, Triple::OSType::LINUX, Triple::Environment::GNU},
            "x86_64-unknown-linux-gnu"},
        {{Triple::ArchType::X86_64, Triple::Vendor::UNKNOWN, Triple::OSType::LINUX, Triple::Environment::OHOS},
            "x86_64-unknown-linux-ohos"},
    };
    for (size_t i = 0; i < pairs.size(); i++) {
        auto actualName = pairs[i].first.ToFullTripleString();
        EXPECT_EQ(actualName, pairs[i].second);
    }
}
