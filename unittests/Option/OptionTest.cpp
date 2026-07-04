// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "gtest/gtest.h"
#include "cangjie/Option/Option.h"
#include "cangjie/Driver/Driver.h"
#include "cangjie/Utils/FileUtil.h"

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

TEST_F(OptionTest, LinkNameOnStaticLibTest)
{
    std::vector<std::string> argStrs = {
        "cjc", "--no-prelude", "--target=x86_64-unknown-linux-gnu", "--output-type", "staticlib",
        "--link-name", "foo", "--link-name", "bar", srcFile};
    ArgList argList;
    EXPECT_TRUE(optTbl->ParseArgs(argStrs, argList));

    EXPECT_TRUE(gblOpts->ParseFromArgs(argList));
    std::vector<std::string> expected = {"foo", "bar"};
    EXPECT_EQ(gblOpts->GetLinkNameLibs(), expected);
}

TEST_F(OptionTest, LinkNameDeduplicatesTest)
{
    std::vector<std::string> argStrs = {
        "cjc", "--no-prelude", "--target=x86_64-unknown-linux-gnu", "--output-type", "obj",
        "--link-name", "foo", "--link-name", "foo", srcFile};
    ArgList argList;
    EXPECT_TRUE(optTbl->ParseArgs(argStrs, argList));

    EXPECT_TRUE(gblOpts->ParseFromArgs(argList));
    std::vector<std::string> expected = {"foo"};
    EXPECT_EQ(gblOpts->GetLinkNameLibs(), expected);
}

TEST_F(OptionTest, LinkNameRejectedOnExecutableTest)
{
    // Meaningless when the final product is already being linked.
    std::vector<std::string> argStrs = {
        "cjc", "--no-prelude", "--target=x86_64-unknown-linux-gnu", "--link-name", "foo", srcFile};
    ArgList argList;
    EXPECT_TRUE(optTbl->ParseArgs(argStrs, argList));

    EXPECT_FALSE(gblOpts->ParseFromArgs(argList));
}

TEST_F(OptionTest, LinkNameOnMinGWTargetTest)
{
    // COFF records the dependency as a '/DEFAULTLIB:' directive in '.drectve'.
    std::vector<std::string> argStrs = {
        "cjc", "--no-prelude", "--target=x86_64-windows-gnu", "--output-type", "staticlib",
        "--link-name", "foo", srcFile};
    ArgList argList;
    EXPECT_TRUE(optTbl->ParseArgs(argStrs, argList));

    EXPECT_TRUE(gblOpts->ParseFromArgs(argList));
    std::vector<std::string> expected = {"foo"};
    EXPECT_EQ(gblOpts->GetLinkNameLibs(), expected);
}

TEST_F(OptionTest, LinkNameOnAppleTargetTest)
{
    // Mach-O records it as an 'LC_LINKER_OPTION' load command, which lld/MachO parses back out.
    std::vector<std::string> argStrs = {
        "cjc", "--no-prelude", "--target=x86_64-apple-darwin", "--output-type", "staticlib",
        "--link-name", "foo", srcFile};
    ArgList argList;
    EXPECT_TRUE(optTbl->ParseArgs(argStrs, argList));

    EXPECT_TRUE(gblOpts->ParseFromArgs(argList));
    std::vector<std::string> expected = {"foo"};
    EXPECT_EQ(gblOpts->GetLinkNameLibs(), expected);
}

TEST_F(OptionTest, LinkNameRejectsEmptyValueTest)
{
    std::vector<std::string> argStrs = {
        "cjc", "--no-prelude", "--target=x86_64-unknown-linux-gnu", "--output-type", "staticlib",
        "--link-name", "", srcFile};
    ArgList argList;
    EXPECT_TRUE(optTbl->ParseArgs(argStrs, argList));

    EXPECT_FALSE(gblOpts->ParseFromArgs(argList));
}

TEST_F(OptionTest, DisableAutoLinkTest)
{
    // Target pinned: acceptance depends on the object format, so the host must not decide it.
    std::vector<std::string> argStrs = {
        "cjc", "--no-prelude", "--target=x86_64-unknown-linux-gnu", "--disable-auto-link", srcFile};
    ArgList argList;
    EXPECT_TRUE(optTbl->ParseArgs(argStrs, argList));

    EXPECT_TRUE(gblOpts->ParseFromArgs(argList));
    EXPECT_TRUE(gblOpts->GetDisableAutoLink());
}

TEST_F(OptionTest, DisableAutoLinkOnMinGWTargetTest)
{
    // Supported: lld's COFF port honours '/nodefaultlib', forwarded from '-Xlink='.
    std::vector<std::string> argStrs = {
        "cjc", "--no-prelude", "--target=x86_64-windows-gnu", "--disable-auto-link", srcFile};
    ArgList argList;
    EXPECT_TRUE(optTbl->ParseArgs(argStrs, argList));

    EXPECT_TRUE(gblOpts->ParseFromArgs(argList));
    EXPECT_TRUE(gblOpts->GetDisableAutoLink());
}

TEST_F(OptionTest, DisableAutoLinkRejectedOnAppleTargetTest)
{
    // Unsupported: '-ignore_auto_link' is declared in lld's Mach-O port but never implemented.
    std::vector<std::string> argStrs = {
        "cjc", "--no-prelude", "--target=x86_64-apple-darwin", "--disable-auto-link", srcFile};
    ArgList argList;
    EXPECT_TRUE(optTbl->ParseArgs(argStrs, argList));

    EXPECT_FALSE(gblOpts->ParseFromArgs(argList));
}

TEST_F(OptionTest, AutoLinkRejectedOnUnknownObjectFormatTest)
{
    // Rejected by both halves: no channel to write the record into, and no switch to suppress it.
    // Reachable: 'unknown' is a valid OS field.
    {
        // Control: the triple itself parses, so the rejections below come from the options.
        std::vector<std::string> argStrs = {
            "cjc", "--no-prelude", "--target=x86_64-unknown-unknown-gnu", srcFile};
        ArgList argList;
        EXPECT_TRUE(optTbl->ParseArgs(argStrs, argList));

        GlobalOptions opts;
        EXPECT_TRUE(opts.ParseFromArgs(argList));
        EXPECT_EQ(opts.target.GetObjectFormat(), Triple::ObjectFormat::UNKNOWN);
    }
    {
        std::vector<std::string> argStrs = {
            "cjc", "--no-prelude", "--target=x86_64-unknown-unknown-gnu", "--output-type", "staticlib",
            "--link-name", "foo", srcFile};
        ArgList argList;
        EXPECT_TRUE(optTbl->ParseArgs(argStrs, argList));

        GlobalOptions opts;
        EXPECT_FALSE(opts.ParseFromArgs(argList));
    }
    {
        std::vector<std::string> argStrs = {
            "cjc", "--no-prelude", "--target=x86_64-unknown-unknown-gnu", "--disable-auto-link", srcFile};
        ArgList argList;
        EXPECT_TRUE(optTbl->ParseArgs(argStrs, argList));

        GlobalOptions opts;
        EXPECT_FALSE(opts.ParseFromArgs(argList));
    }
}

TEST_F(OptionTest, DisableAutoLinkDoesNotSuppressLinkNameTest)
{
    // Orthogonal: '--disable-auto-link' must never remove what '--link-name' wrote. Checked on
    // MinGW, where the record used to be dropped during codegen.
    std::vector<std::string> argStrs = {
        "cjc", "--no-prelude", "--target=x86_64-windows-gnu", "--output-type", "staticlib",
        "--link-name", "foo", "--disable-auto-link", srcFile};
    ArgList argList;
    EXPECT_TRUE(optTbl->ParseArgs(argStrs, argList));

    EXPECT_TRUE(gblOpts->ParseFromArgs(argList));
    std::vector<std::string> expected = {"foo"};
    EXPECT_EQ(gblOpts->GetLinkNameLibs(), expected);
    EXPECT_TRUE(gblOpts->GetDisableAutoLink());
}

TEST_F(OptionTest, LinkNameParticipatesInIncrementalFingerprintTest)
{
    // ToSerialized() is the incremental fingerprint: when it matches and the sources are unchanged,
    // codegen is skipped entirely, so '--link-name' must be part of it.
    auto serializedFor = [this](const std::vector<std::string>& extraArgs) {
        std::vector<std::string> argStrs = {
            "cjc", "--no-prelude", "--target=x86_64-unknown-linux-gnu", "--output-type", "staticlib"};
        argStrs.insert(argStrs.end(), extraArgs.begin(), extraArgs.end());
        argStrs.push_back(srcFile);
        ArgList argList;
        EXPECT_TRUE(optTbl->ParseArgs(argStrs, argList));

        GlobalOptions opts;
        EXPECT_TRUE(opts.ParseFromArgs(argList));
        return opts.ToSerialized();
    };

    const auto none = serializedFor({});
    const auto foo = serializedFor({"--link-name", "foo"});
    const auto bar = serializedFor({"--link-name", "bar"});
    const auto fooAgain = serializedFor({"--link-name", "foo"});

    EXPECT_NE(foo, none);
    EXPECT_NE(foo, bar);
    EXPECT_EQ(foo, fooAgain);

    // '--disable-auto-link' only alters the link command line, so it stays out of the fingerprint.
    EXPECT_EQ(serializedFor({"--disable-auto-link"}), none);
}

TEST(TripleObjectFormatTest, ObjectFormatByTarget)
{
    Triple::Info linuxGnu = {
        Triple::ArchType::X86_64, Triple::Vendor::UNKNOWN, Triple::OSType::LINUX, Triple::Environment::GNU};
    Triple::Info linuxOhos = {
        Triple::ArchType::AARCH64, Triple::Vendor::UNKNOWN, Triple::OSType::LINUX, Triple::Environment::OHOS};
    Triple::Info linuxAndroid = {
        Triple::ArchType::AARCH64, Triple::Vendor::UNKNOWN, Triple::OSType::LINUX, Triple::Environment::ANDROID};
    Triple::Info windows = {
        Triple::ArchType::X86_64, Triple::Vendor::PC, Triple::OSType::WINDOWS, Triple::Environment::GNU};
    Triple::Info darwin = {
        Triple::ArchType::AARCH64, Triple::Vendor::APPLE, Triple::OSType::DARWIN, Triple::Environment::NOT_AVAILABLE};
    Triple::Info ios = {
        Triple::ArchType::AARCH64, Triple::Vendor::APPLE, Triple::OSType::IOS, Triple::Environment::NOT_AVAILABLE};
    Triple::Info unknown = {
        Triple::ArchType::X86_64, Triple::Vendor::UNKNOWN, Triple::OSType::UNKNOWN, Triple::Environment::NOT_AVAILABLE};

    // OHOS and Android are environments layered on OSType::LINUX, so they must land on ELF.
    EXPECT_EQ(linuxGnu.GetObjectFormat(), Triple::ObjectFormat::ELF);
    EXPECT_EQ(linuxOhos.GetObjectFormat(), Triple::ObjectFormat::ELF);
    EXPECT_EQ(linuxAndroid.GetObjectFormat(), Triple::ObjectFormat::ELF);
    EXPECT_EQ(windows.GetObjectFormat(), Triple::ObjectFormat::COFF);
    EXPECT_EQ(darwin.GetObjectFormat(), Triple::ObjectFormat::MACH_O);
    EXPECT_EQ(ios.GetObjectFormat(), Triple::ObjectFormat::MACH_O);
    // An unrecognised target must not be silently treated as ELF.
    EXPECT_EQ(unknown.GetObjectFormat(), Triple::ObjectFormat::UNKNOWN);

    EXPECT_TRUE(linuxGnu.IsELF());
    EXPECT_TRUE(linuxOhos.IsELF());
    EXPECT_TRUE(linuxAndroid.IsELF());
    EXPECT_FALSE(windows.IsELF());
    EXPECT_FALSE(darwin.IsELF());
    EXPECT_FALSE(ios.IsELF());
    EXPECT_FALSE(unknown.IsELF());
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
