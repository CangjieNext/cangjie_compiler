// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#include <process.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "gtest/gtest.h"

#include "cangjie/Utils/FileUtil.h"
#include "cangjie/Utils/ProfileRecorder.h"

#include "UserCodeInfo.h"
#include "UserMemoryUsage.h"
#include "UserTimer.h"

using namespace Cangjie;
using namespace Cangjie::Utils;

namespace {
std::string TempDirRoot()
{
#ifdef _WIN32
    char buffer[MAX_PATH] = {0};
    DWORD len = GetTempPathA(MAX_PATH, buffer);
    if (len == 0 || len > MAX_PATH) {
        return ".";
    }
    return std::string(buffer, len);
#else
    const char* tmp = std::getenv("TMPDIR");
    if (tmp == nullptr || *tmp == '\0') {
        return "/tmp";
    }
    return tmp;
#endif
}

std::string MakeTempDir(const std::string& prefix)
{
    static std::atomic<uint64_t> counter{0};
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
#ifdef _WIN32
    int pid = _getpid();
#else
    int pid = getpid();
#endif
    std::ostringstream oss;
    oss << prefix << "_" << static_cast<unsigned long long>(now) << "_" << pid << "_" << ++counter;
    std::string path = FileUtil::JoinPath(TempDirRoot(), oss.str());
#ifdef _WIN32
    (void)_mkdir(path.c_str());
#else
    (void)mkdir(path.c_str(), S_IRWXU);
#endif
    return path;
}

/**
 * Minimal concrete UserBase, so the UserBase behaviour (enable gate, package name sanitising,
 * output directory selection) can be exercised without touching the process-wide singletons.
 */
class FakeUserRecord : public UserBase {
public:
    FakeUserRecord() = default;
    ~FakeUserRecord() override
    {
        // UserBase-derived destructors of the real records call OutputResult(); make sure this
        // helper never writes anything on the way out.
        Enable(false);
    }

private:
    std::string GetSuffix() const override
    {
        return ".fake.prof";
    }
    std::string GetJson() const override
    {
        return "{\"fake\": 1}";
    }
};

/**
 * The recorders are process-wide singletons; every test must hand them back disabled so that the
 * remaining tests in this binary - and the singleton destructors at exit - stay side-effect free.
 */
class ProfileRecorderTest : public testing::Test {
protected:
    /// Temporary directory owned by the fixture, so a failing ASSERT_* still gets it removed.
    /// Returns by value: a reference into the vector would dangle on the next call.
    std::string TempDir(const std::string& prefix)
    {
        tempDirs.push_back(MakeTempDir(prefix));
        return tempDirs.back();
    }

    void TearDown() override
    {
        ProfileRecorder::Enable(false, ProfileRecorder::Type::ALL);
        // The output directory is about to disappear. Note SetOutputDir("") does not clear the
        // field - UserBase falls back to GetDirPath("") == "." - but any path is better than one
        // we just deleted, and the records are disabled above anyway.
        ProfileRecorder::SetOutputDir("");
        ProfileRecorder::SetPackageName("");
        for (const auto& dir : tempDirs) {
            (void)FileUtil::RemoveDirectoryRecursively(dir);
        }
        tempDirs.clear();
    }

private:
    std::vector<std::string> tempDirs;
};
} // namespace

TEST_F(ProfileRecorderTest, DisabledRecorderProducesNoResult)
{
    ProfileRecorder::Enable(false, ProfileRecorder::Type::ALL);

    EXPECT_FALSE(UserTimer::Instance().IsEnable());
    EXPECT_FALSE(UserMemoryUsage::Instance().IsEnable());
    EXPECT_FALSE(UserCodeInfo::Instance().IsEnable());
    EXPECT_EQ(ProfileRecorder::GetResult(ProfileRecorder::Type::ALL), "");

    // Start/Stop on a disabled recorder must be a no-op rather than a crash.
    ProfileRecorder::Start("Main Stage", "disabled");
    ProfileRecorder::Stop("Main Stage", "disabled");
    EXPECT_EQ(ProfileRecorder::GetResult(ProfileRecorder::Type::ALL), "");
}

TEST_F(ProfileRecorderTest, EnableTimerOnlyLeavesMemoryDisabled)
{
    ProfileRecorder::Enable(true, ProfileRecorder::Type::TIMER);

    EXPECT_TRUE(UserTimer::Instance().IsEnable());
    EXPECT_FALSE(UserMemoryUsage::Instance().IsEnable());
    // Code info follows "timer or memory is on".
    EXPECT_TRUE(UserCodeInfo::Instance().IsEnable());

    ProfileRecorder::Start("Main Stage", "Parse");
    ProfileRecorder::Stop("Main Stage", "Parse");

    const std::string timerResult = ProfileRecorder::GetResult(ProfileRecorder::Type::TIMER);
    EXPECT_NE(timerResult.find("Main Stage"), std::string::npos);
    EXPECT_NE(timerResult.find("Parse"), std::string::npos);
    // Memory is off, so the memory record itself contributes nothing.
    EXPECT_EQ(UserMemoryUsage::Instance().GetResult(), "");
}

TEST_F(ProfileRecorderTest, EnableMemoryOnlyLeavesTimerDisabled)
{
    ProfileRecorder::Enable(true, ProfileRecorder::Type::MEMORY);

    EXPECT_FALSE(UserTimer::Instance().IsEnable());
    EXPECT_TRUE(UserMemoryUsage::Instance().IsEnable());
    EXPECT_TRUE(UserCodeInfo::Instance().IsEnable());

    ProfileRecorder::Start("Main Stage", "Sema");
    ProfileRecorder::Stop("Main Stage", "Sema");

    EXPECT_NE(ProfileRecorder::GetResult(ProfileRecorder::Type::MEMORY).find("Main Stage"), std::string::npos);
    EXPECT_EQ(UserTimer::Instance().GetResult(), "");
}

TEST_F(ProfileRecorderTest, DisablingAllAlsoDisablesCodeInfo)
{
    ProfileRecorder::Enable(true, ProfileRecorder::Type::ALL);
    EXPECT_TRUE(UserCodeInfo::Instance().IsEnable());

    ProfileRecorder::Enable(false, ProfileRecorder::Type::ALL);
    EXPECT_FALSE(UserTimer::Instance().IsEnable());
    EXPECT_FALSE(UserMemoryUsage::Instance().IsEnable());
    EXPECT_FALSE(UserCodeInfo::Instance().IsEnable());
}

TEST_F(ProfileRecorderTest, ScopedRecorderTimesItsOwnScope)
{
    ProfileRecorder::Enable(true, ProfileRecorder::Type::ALL);
    {
        ProfileRecorder recorder("Main Stage", "scoped", "desc");
    }
    const std::string result = ProfileRecorder::GetResult(ProfileRecorder::Type::ALL);
    EXPECT_NE(result.find("scoped"), std::string::npos);
}

TEST_F(ProfileRecorderTest, RecordCodeInfoOnlyEvaluatesClosureWhenEnabled)
{
    ProfileRecorder::Enable(false, ProfileRecorder::Type::ALL);
    int calls = 0;
    ProfileRecorder::RecordCodeInfo("skipped item", [&calls]() -> int64_t {
        ++calls;
        return 1;
    });
    EXPECT_EQ(calls, 0);

    ProfileRecorder::Enable(true, ProfileRecorder::Type::TIMER);
    ProfileRecorder::RecordCodeInfo("evaluated item", [&calls]() -> int64_t {
        ++calls;
        return 42;
    });
    EXPECT_EQ(calls, 1);

    // The non-closure overload records unconditionally.
    ProfileRecorder::RecordCodeInfo("direct item", 7);

    const std::string result = ProfileRecorder::GetResult(ProfileRecorder::Type::ALL);
    EXPECT_NE(result.find("evaluated item"), std::string::npos);
    EXPECT_NE(result.find("direct item"), std::string::npos);
}

TEST_F(ProfileRecorderTest, OutputResultWritesProfFilesAndSanitisesPackageName)
{
    const std::string dir = TempDir("cj_profile_out");
    ASSERT_TRUE(FileUtil::IsDir(dir));

    ProfileRecorder::SetOutputDir(dir);
    // A slash in the package name would otherwise be read as a path separator; UserBase turns it
    // into '-' so the profile lands directly in the output directory.
    ProfileRecorder::SetPackageName("demo/pkg");
    ProfileRecorder::Enable(true, ProfileRecorder::Type::ALL);
    ProfileRecorder::Start("Main Stage", "write");
    ProfileRecorder::Stop("Main Stage", "write");

    UserTimer::Instance().OutputResult();
    UserMemoryUsage::Instance().OutputResult();
    UserCodeInfo::Instance().OutputResult();

    const std::string timeProf = FileUtil::JoinPath(dir, "demo-pkg.time.prof");
    EXPECT_TRUE(FileUtil::FileExist(timeProf));
    EXPECT_TRUE(FileUtil::FileExist(FileUtil::JoinPath(dir, "demo-pkg.mem.prof")));
    EXPECT_TRUE(FileUtil::FileExist(FileUtil::JoinPath(dir, "demo-pkg.info.prof")));

    std::string failedReason;
    auto content = FileUtil::ReadFileContent(timeProf, failedReason);
    ASSERT_TRUE(content.has_value()) << failedReason;
    EXPECT_NE(content->find("Main Stage"), std::string::npos);

    ProfileRecorder::Enable(false, ProfileRecorder::Type::ALL);
    // A disabled record writes nothing at all.
    UserTimer::Instance().OutputResult();
}

// A package name is turned into a file name, so every directory separator in it has to go -
// not just the first. Leaving one behind would aim the write at a subdirectory that does not
// exist, and WriteJson would then fail silently.
TEST_F(ProfileRecorderTest, WriteJsonReplacesEverySeparatorInThePackageName)
{
    const std::string dir = TempDir("cj_profile_sep");
    ASSERT_TRUE(FileUtil::IsDir(dir));

    FakeUserRecord record;
    record.SetOutputDir(dir);
    record.Enable(true);

    record.SetPackageName("a/b/c");
    record.OutputResult();
    EXPECT_TRUE(FileUtil::FileExist(FileUtil::JoinPath(dir, "a-b-c.fake.prof")));

#ifdef _WIN32
    // On Windows a backslash is a separator too.
    record.SetPackageName("d\\e");
    record.OutputResult();
    EXPECT_TRUE(FileUtil::FileExist(FileUtil::JoinPath(dir, "d-e.fake.prof")));
#endif
}

TEST_F(ProfileRecorderTest, SetOutputDirFallsBackToTheParentOfAFilePath)
{
    const std::string dir = TempDir("cj_profile_dir");
    ASSERT_TRUE(FileUtil::IsDir(dir));

    FakeUserRecord record;
    record.SetOutputDir(dir);
    record.SetPackageName("pkg");
    record.Enable(true);
    record.OutputResult();
    EXPECT_TRUE(FileUtil::FileExist(FileUtil::JoinPath(dir, "pkg.fake.prof")));

    // Handed a file path instead of a directory, UserBase uses the containing directory.
    record.SetOutputDir(FileUtil::JoinPath(dir, "some_file.txt"));
    record.SetPackageName("from_file_path");
    record.OutputResult();
    EXPECT_TRUE(FileUtil::FileExist(FileUtil::JoinPath(dir, "from_file_path.fake.prof")));
}

TEST_F(ProfileRecorderTest, DisabledRecordReturnsEmptyResultAndWritesNothing)
{
    FakeUserRecord record;
    record.Enable(false);
    EXPECT_FALSE(record.IsEnable());
    EXPECT_EQ(record.GetResult(), "");

    record.Enable(true);
    EXPECT_TRUE(record.IsEnable());
    EXPECT_EQ(record.GetResult(), "{\"fake\": 1}");

    // WriteJson bails out when the stream cannot be opened, so nothing is written and no
    // directory is created along the way. SetOutputDir takes the parent of a non-directory
    // argument, so the *parent* has to be the missing one.
    const std::string missingDir = FileUtil::JoinPath(TempDirRoot(), "cj_no_such_dir_for_profile");
    ASSERT_FALSE(FileUtil::FileExist(missingDir));
    record.SetOutputDir(FileUtil::JoinPath(missingDir, "inner"));
    record.SetPackageName("pkg");
    record.OutputResult();
    EXPECT_FALSE(FileUtil::FileExist(FileUtil::JoinPath(missingDir, "pkg.fake.prof")));
    EXPECT_FALSE(FileUtil::FileExist(missingDir));
}

TEST_F(ProfileRecorderTest, CodeInfoJsonHandlesEmptyAndPopulatedRecords)
{
    UserCodeInfo empty;
    empty.Enable(true);
    // No item recorded: the trailing-comma trim must be skipped.
    EXPECT_EQ(empty.GetResult(), "{\n}\n");

    empty.RecordInfo("lines", 10);
    empty.RecordInfo("files", 2);
    const std::string json = empty.GetResult();
    EXPECT_NE(json.find("\"lines\": 10"), std::string::npos);
    EXPECT_NE(json.find("\"files\": 2"), std::string::npos);
    // Only the final entry loses its comma.
    EXPECT_EQ(json.find(",\n}"), std::string::npos);
    empty.Enable(false);
}
