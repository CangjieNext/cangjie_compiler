// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <atomic>
#include <chrono>
#include <climits>
#include <cstdint>
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
#include <sys/types.h>
#include <unistd.h>
#endif

#include "gtest/gtest.h"

#include "cangjie/Utils/FileUtil.h"

using namespace Cangjie;
using namespace Cangjie::FileUtil;

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

std::string MakeTempName(const std::string& prefix)
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
    return JoinPath(TempDirRoot(), oss.str());
}

std::string MakeTempDir(const std::string& prefix)
{
    std::string path = MakeTempName(prefix);
#ifdef _WIN32
    (void)_mkdir(path.c_str());
#else
    (void)mkdir(path.c_str(), S_IRWXU);
#endif
    return path;
}

void WriteRaw(const std::string& path, const std::string& content)
{
    std::ofstream out(path, std::ios::out | std::ios::binary);
    out << content;
    out.close();
}

/**
 * RAII guard so a failing expectation still removes the temporary tree.
 */
class TempDir {
public:
    explicit TempDir(const std::string& prefix) : path(MakeTempDir(prefix))
    {
    }
    ~TempDir()
    {
        (void)RemoveDirectoryRecursively(path);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    const std::string& Path() const
    {
        return path;
    }

private:
    std::string path;
};
} // namespace

TEST(FileUtilExtraTest, GetQuotedEscapesEmbeddedQuotes)
{
    EXPECT_EQ(GetQuoted("plain"), "\"plain\"");
    EXPECT_EQ(GetQuoted(""), "\"\"");
    EXPECT_EQ(GetQuoted("with \"quote\""), "\"with \\\"quote\\\"\"");
    EXPECT_EQ(GetQuoted("back\\slash"), "\"back\\\\slash\"");
}

TEST(FileUtilExtraTest, GetDirNameReturnsTheInnermostDirectory)
{
    EXPECT_EQ(GetDirName("/a/b/c"), "c");
    // A trailing separator is dropped before the name is taken.
    EXPECT_EQ(GetDirName("/a/b/c/"), "c");
    EXPECT_EQ(GetDirName("solo"), "solo");
    // "." and ".." are resolved against the working directory first, so a real name comes back.
    const std::string dot = GetDirName(".");
    EXPECT_FALSE(dot.empty());
    EXPECT_NE(dot, ".");
    const std::string dotdot = GetDirName("..");
    EXPECT_FALSE(dotdot.empty());
    EXPECT_NE(dotdot, "..");
}

TEST(FileUtilExtraTest, HasCJDExtensionRequiresANonEmptyStem)
{
    EXPECT_TRUE(HasCJDExtension("a.cj.d"));
    EXPECT_TRUE(HasCJDExtension("dir/pkg.cj.d"));
    // ".cj.d" alone has no file name in front of the extension.
    EXPECT_FALSE(HasCJDExtension(".cj.d"));
    EXPECT_FALSE(HasCJDExtension("a.cj"));
    EXPECT_FALSE(HasCJDExtension("cj.d"));
    EXPECT_FALSE(HasCJDExtension(""));
    // Shorter than the extension itself.
    EXPECT_FALSE(HasCJDExtension("d"));
}

TEST(FileUtilExtraTest, NormalizeCollapsesSeparatorsAndStopsAtNul)
{
    // Built from pieces so the source carries no "//" inside a string literal.
    EXPECT_EQ(Normalize("a" + std::string(2, '/') + "b" + std::string(3, '/') + "c"), "a/b/c");
    EXPECT_EQ(Normalize(""), "");
    // A NUL terminates the normalised path.
    const std::string withNul = std::string("abc") + '\0' + "def";
    EXPECT_EQ(Normalize(withNul), "abc");
}

TEST(FileUtilExtraTest, NormalizePathKeepsEmptyInputAndPrefixesBareNames)
{
    EXPECT_EQ(NormalizePath(""), "");
    EXPECT_EQ(NormalizePath("./file.cj"), "./file.cj");
    EXPECT_EQ(NormalizePath("a/b.cj"), "a/b.cj");
}

TEST(FileUtilExtraTest, ReadFileContentReportsWhyItFailed)
{
    TempDir dir("cj_fileutil_read");
    std::string failedReason;

    EXPECT_FALSE(ReadFileContent(JoinPath(dir.Path(), "missing.txt"), failedReason).has_value());
    EXPECT_EQ(failedReason, "file not exist");

    const std::string plain = JoinPath(dir.Path(), "plain.txt");
    WriteRaw(plain, "hello");
    failedReason.clear();
    auto content = ReadFileContent(plain, failedReason);
    ASSERT_TRUE(content.has_value()) << failedReason;
    EXPECT_EQ(content.value(), "hello");
    EXPECT_TRUE(failedReason.empty());

    // UTF-8 BOM must be stripped.
    const std::string bom = JoinPath(dir.Path(), "bom.txt");
    WriteRaw(bom, std::string("\xEF\xBB\xBF") + "bom body");
    auto bomContent = ReadFileContent(bom, failedReason);
    ASSERT_TRUE(bomContent.has_value()) << failedReason;
    EXPECT_EQ(bomContent.value(), "bom body");

    // A file shorter than the BOM marker must still be read verbatim.
    const std::string tiny = JoinPath(dir.Path(), "tiny.txt");
    WriteRaw(tiny, "x");
    auto tinyContent = ReadFileContent(tiny, failedReason);
    ASSERT_TRUE(tinyContent.has_value()) << failedReason;
    EXPECT_EQ(tinyContent.value(), "x");
}

TEST(FileUtilExtraTest, ReadBinaryFileToBufferReportsWhyItFailed)
{
    TempDir dir("cj_fileutil_bin");
    std::vector<uint8_t> buffer;
    std::string failedReason;

    EXPECT_FALSE(ReadBinaryFileToBuffer(JoinPath(dir.Path(), "missing.bin"), buffer, failedReason));
    EXPECT_EQ(failedReason, "open file failed");

    const std::string empty = JoinPath(dir.Path(), "empty.bin");
    WriteRaw(empty, "");
    EXPECT_FALSE(ReadBinaryFileToBuffer(empty, buffer, failedReason));
    EXPECT_EQ(failedReason, "empty binary file");

    const std::string good = JoinPath(dir.Path(), "good.bin");
    WriteRaw(good, std::string("\x01\x02\x03", 3));
    ASSERT_TRUE(ReadBinaryFileToBuffer(good, buffer, failedReason)) << failedReason;
    ASSERT_EQ(buffer.size(), 3U);
    EXPECT_EQ(buffer[0], 1);
    EXPECT_EQ(buffer[2], 3);
}

TEST(FileUtilExtraTest, WriteToFileCreatesMissingDirectoriesAndReportsFailures)
{
    TempDir dir("cj_fileutil_write");

    const std::string nested = JoinPath(JoinPath(dir.Path(), "a/b"), "out.txt");
    ASSERT_TRUE(WriteToFile(nested, "payload"));
    std::string failedReason;
    auto content = ReadFileContent(nested, failedReason);
    ASSERT_TRUE(content.has_value()) << failedReason;
    EXPECT_EQ(content.value(), "payload");

    // Overwriting an existing file keeps the happy path.
    ASSERT_TRUE(WriteToFile(nested, "second"));
    EXPECT_EQ(ReadFileContent(nested, failedReason).value(), "second");

    // A regular file standing where a directory is expected must be reported, not thrown.
    const std::string blocker = JoinPath(dir.Path(), "blocker");
    WriteRaw(blocker, "not a directory");
    EXPECT_FALSE(WriteToFile(JoinPath(blocker, "out.txt"), "payload"));
}

TEST(FileUtilExtraTest, WriteBufferToASTFileCreatesTheBaseDirectory)
{
    TempDir dir("cj_fileutil_ast");
    const std::vector<uint8_t> buffer{1, 2, 3, 4};

    const std::string nested = JoinPath(JoinPath(dir.Path(), "gen/ast"), "pkg.cjo");
    ASSERT_TRUE(WriteBufferToASTFile(nested, buffer));
    EXPECT_EQ(GetFileSize(nested), 4U);

    std::vector<uint8_t> readBack;
    std::string failedReason;
    ASSERT_TRUE(ReadBinaryFileToBuffer(nested, readBack, failedReason)) << failedReason;
    EXPECT_EQ(readBack, buffer);

    const std::string blocker = JoinPath(dir.Path(), "blocker");
    WriteRaw(blocker, "not a directory");
    EXPECT_FALSE(WriteBufferToASTFile(JoinPath(blocker, "pkg.cjo"), buffer));
}

TEST(FileUtilExtraTest, GetFileSizeIsZeroForMissingFiles)
{
    TempDir dir("cj_fileutil_size");
    EXPECT_EQ(GetFileSize(JoinPath(dir.Path(), "missing")), 0U);

    const std::string file = JoinPath(dir.Path(), "sized");
    WriteRaw(file, "12345");
    EXPECT_EQ(GetFileSize(file), 5U);
}

TEST(FileUtilExtraTest, DirectoryListingIsEmptyForUnreadablePaths)
{
    TempDir dir("cj_fileutil_list");
    const std::string missing = JoinPath(dir.Path(), "missing_dir");

    EXPECT_TRUE(GetAllDirsUnderCurrentPath(missing).empty());
    EXPECT_TRUE(GetAllFilesUnderCurrentPath(missing, "cj").empty());
    EXPECT_TRUE(GetDirectories(missing).empty());

    const std::string sub = JoinPath(dir.Path(), "sub");
    ASSERT_EQ(CreateDirs(sub + "/"), 0);
    WriteRaw(JoinPath(dir.Path(), "a.cj"), "");
    WriteRaw(JoinPath(dir.Path(), "b.txt"), "");
    // Hidden files are skipped.
    WriteRaw(JoinPath(dir.Path(), ".hidden.cj"), "");

    auto cjFiles = GetAllFilesUnderCurrentPath(dir.Path(), "cj");
    ASSERT_EQ(cjFiles.size(), 1U);
    EXPECT_EQ(cjFiles.front(), "a.cj");

    auto dirs = GetAllDirsUnderCurrentPath(dir.Path());
    ASSERT_EQ(dirs.size(), 1U);
    EXPECT_EQ(GetFileName(dirs.front()), "sub");

    auto directories = GetDirectories(dir.Path());
    ASSERT_EQ(directories.size(), 1U);
    EXPECT_EQ(directories.front().name, "sub");
}

TEST(FileUtilExtraTest, CreateDirsRejectsOverlongPathsAndNames)
{
    TempDir dir("cj_fileutil_mkdir");

    // Whole path above the limit.
    EXPECT_EQ(CreateDirs(std::string(DIR_PATH_MAX_LENGTH + 1, 'a')), -1);

    // A single component above the per-name limit.
    const std::string longName = JoinPath(dir.Path(), std::string(FILE_NAME_MAX_LENGTH + 10, 'b')) + "/";
    EXPECT_EQ(CreateDirs(longName), -1);

    // Creating the same tree twice must stay successful.
    const std::string nested = JoinPath(dir.Path(), "x/y/z") + "/";
    EXPECT_EQ(CreateDirs(nested), 0);
    EXPECT_EQ(CreateDirs(nested), 0);
    EXPECT_TRUE(IsDir(JoinPath(dir.Path(), "x/y/z")));
}

TEST(FileUtilExtraTest, RemoveDirectoryRecursivelyRejectsEmptyPath)
{
    EXPECT_FALSE(RemoveDirectoryRecursively(""));
}

#ifndef _WIN32
TEST(FileUtilExtraTest, RemoveRejectsNonRegularNonDirectoryEntries)
{
    TempDir dir("cj_fileutil_fifo");
    const std::string fifo = JoinPath(dir.Path(), "pipe");
    ASSERT_EQ(mkfifo(fifo.c_str(), S_IRUSR | S_IWUSR), 0);

    // Neither a regular file, a directory nor a symlink, so Remove refuses to touch it.
    EXPECT_FALSE(Remove(fifo));
    EXPECT_EQ(::unlink(fifo.c_str()), 0);

    EXPECT_FALSE(Remove(JoinPath(dir.Path(), "missing")));
}
#endif

TEST(FileUtilExtraTest, IsAbsolutePathAboveLengthLimitDetectsOverlongPaths)
{
    EXPECT_FALSE(IsAbsolutePathAboveLengthLimit("/tmp/short"));
    EXPECT_TRUE(IsAbsolutePathAboveLengthLimit("/" + std::string(PATH_MAX, 'a')));
}

TEST(FileUtilExtraTest, IsIdentifierRejectsMalformedUtf8AndKeywords)
{
    EXPECT_TRUE(IsIdentifier("abc"));
    EXPECT_TRUE(IsIdentifier("_a1"));
    EXPECT_FALSE(IsIdentifier(""));
    EXPECT_FALSE(IsIdentifier("1abc"));
    EXPECT_FALSE(IsIdentifier("a-b"));
    // Keywords are not identifiers unless quoted with backticks.
    EXPECT_FALSE(IsIdentifier("class"));
    EXPECT_TRUE(IsIdentifier("`class`"));
    // A backtick pair with nothing inside is not an identifier.
    EXPECT_FALSE(IsIdentifier("``"));
    EXPECT_FALSE(IsIdentifier("`abc"));
    // Truncated / invalid UTF-8 must be rejected instead of read past the end.
    EXPECT_FALSE(IsIdentifier("\xE4\xBD"));
    EXPECT_FALSE(IsIdentifier("\xFF\xFE"));
    // A valid non-ASCII XID_Start character is accepted.
    EXPECT_TRUE(IsIdentifier("\xE4\xBD\xA0"));
}

TEST(FileUtilExtraTest, ConvertPackageNameToLibCangjieBaseFormat)
{
    EXPECT_EQ(ConvertPackageNameToLibCangjieBaseFormat("std.core"), "cangjie-std-core");
    // A root package has no module prefix to prepend.
    EXPECT_EQ(ConvertPackageNameToLibCangjieBaseFormat("std"), "cangjie-std");
    // Unknown packages are not standard libraries.
    EXPECT_EQ(ConvertPackageNameToLibCangjieBaseFormat("my.package"), "");
    EXPECT_EQ(ConvertPackageNameToLibCangjieBaseFormat(""), "");
}

#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
TEST(FileUtilExtraTest, ConvertFilenameToLibCangjieFormats)
{
    EXPECT_EQ(ConvertFilenameToLibCangjieBaseFormat("std.core.o"), "cangjie-std-core");
    EXPECT_EQ(ConvertFilenameToLibCangjieBaseFormat("dir/std.core.o"), "cangjie-std-core");
    EXPECT_EQ(ConvertFilenameToLibCangjieBaseFormat("my.package.o"), "");

    EXPECT_EQ(ConvertFilenameToLibCangjieFormat("std.core.o", ".a"), "libcangjie-std-core.a");
    EXPECT_EQ(ConvertFilenameToLibCangjieFormat("my.package.o", ".a"), "");
}
#endif

TEST(FileUtilExtraTest, RemovePathPrefixStripsOnlyALeadingPrefix)
{
    EXPECT_EQ(RemovePathPrefix("/a/b/c", {"/a/b"}), "c");
    EXPECT_EQ(RemovePathPrefix("/a/b/c", {"/a"}), "b/c");
    // The prefix has to be at position 0 to be stripped.
    EXPECT_EQ(RemovePathPrefix("/a/b/c", {"b"}), "/a/b/c");
    EXPECT_EQ(RemovePathPrefix("/a/b/c", {}), "/a/b/c");
    // Stripping everything leaves an empty path.
    EXPECT_EQ(RemovePathPrefix("/a/b", {"/a/b/"}), "");
}

TEST(FileUtilExtraTest, SplitStrSkipsRepeatedAndTrailingDelimiters)
{
    auto parts = SplitStr("a,b,c", ',');
    ASSERT_EQ(parts.size(), 3U);
    EXPECT_EQ(parts[0], "a");
    EXPECT_EQ(parts[2], "c");

    // Repeated delimiters do not produce empty entries.
    parts = SplitStr("a,,b", ',');
    ASSERT_EQ(parts.size(), 2U);
    EXPECT_EQ(parts[0], "a");
    EXPECT_EQ(parts[1], "b");

    parts = SplitStr(",lead", ',');
    ASSERT_EQ(parts.size(), 1U);
    EXPECT_EQ(parts[0], "lead");

    EXPECT_TRUE(SplitStr("", ',').empty());
    EXPECT_TRUE(SplitStr(",,,", ',').empty());
}

TEST(FileUtilExtraTest, FindProgramAndFileRejectEmptyNamesAndPaths)
{
    EXPECT_EQ(FindProgramByName("", {"/usr/bin"}), "");
    // A name that already contains a separator is taken as-is.
    EXPECT_EQ(FindProgramByName("dir/prog", {"/usr/bin"}), "dir/prog");
    // Empty search entries are skipped.
    EXPECT_EQ(FindProgramByName("cangjie_no_such_program", {"", "/usr/bin"}), "");

    EXPECT_FALSE(FindFileByName("", {"/usr/bin"}).has_value());
    EXPECT_FALSE(FindFileByName("cangjie_no_such_file", {"", "/usr/bin"}).has_value());

    TempDir dir("cj_fileutil_find");
    WriteRaw(JoinPath(dir.Path(), "target.cj"), "");
    auto found = FindFileByName("target.cj", {"", dir.Path()});
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(GetFileName(found.value()), "target.cj");
}

TEST(FileUtilExtraTest, CanWriteFollowsFilesystemPermissions)
{
    TempDir dir("cj_fileutil_canwrite");
    const std::string file = JoinPath(dir.Path(), "writable.txt");
    WriteRaw(file, "x");

    EXPECT_TRUE(CanWrite(file));
    EXPECT_FALSE(CanWrite(JoinPath(dir.Path(), "missing.txt")));
}

TEST(FileUtilExtraTest, SplitEnvironmentPathsDropsEmptyAndUnresolvableEntries)
{
    TempDir dir("cj_fileutil_env");
#ifdef _WIN32
    const std::string sep = ";";
#else
    const std::string sep = ":";
#endif
    const std::string value = dir.Path() + sep + sep + "/cangjie/no/such/path";
    auto paths = SplitEnvironmentPaths(value);
    ASSERT_EQ(paths.size(), 1U);
    EXPECT_EQ(GetFileName(paths.front()), GetFileName(dir.Path()));

    EXPECT_TRUE(SplitEnvironmentPaths("").empty());
}

TEST(FileUtilExtraTest, CjoFileNameAndPackageNameRoundTrip)
{
    EXPECT_EQ(ToCjoFileName("std.core"), "std.core");
    EXPECT_EQ(ToPackageName("std.core"), "std.core");

    const std::string cjoName = ToCjoFileName("pkg::org");
    EXPECT_EQ(ToPackageName(cjoName), "pkg::org");
}
