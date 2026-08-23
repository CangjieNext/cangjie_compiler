// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// LSP FlatBuffers deserialization guard test.
//
// Background: In LSP mode, LSPServer receives a FlatBuffers MacroResult from the
// LSPMacroServer child process over an IPC pipe. The old DeSerializeIdInfoFromResult
// dereferenced result->id()->name() unguarded; when MacroResult.id is absent, id()
// returns nullptr -> nullptr->name() -> SIGSEGV.
//
// This guard builds attack payloads directly and invokes the real
// DeSerializeIdInfoFromResult / Verifier, asserting the fixed behaviour. If the fix
// is ever reverted (back to the bare dereference), the child process SIGSEGVs and the
// test fails.

#include <cstring>
#include <csignal>
#include <string>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>

#include "gtest/gtest.h"
#include "flatbuffers/flatbuffers.h"
#include "flatbuffers/MacroMsgFormat_generated.h"
#include "cangjie/Macro/MacroEvalMsgSerializer.h"
#include "cangjie/Basic/Position.h"

using namespace MacroMsgFormat;
using Cangjie::MacroEvalMsgSerializer;
using Cangjie::Position;

namespace {
// Attack payload: MacroMsg{content=macroResult} with MacroResult.id absent.
// id() returns nullptr; the old code crashes on result->id()->name(), the fixed
// version is blocked by the field null check.
std::vector<uint8_t> BuildIdAbsentMacroMsg()
{
    flatbuffers::FlatBufferBuilder b(64);
    auto resOff = CreateMacroResult(b);  // do not set id -> id() == nullptr
    auto msgOff = CreateMacroMsg(b, MsgContent_macroResult, resOff.Union());
    b.Finish(msgOff);
    return std::vector<uint8_t>(b.GetBufferPointer(), b.GetBufferPointer() + b.GetSize());
}

// Truncated (malformed) buffer: the Verifier must reject it.
std::vector<uint8_t> BuildTruncatedMacroMsg()
{
    auto full = BuildIdAbsentMacroMsg();
    full.resize(full.size() / 2);  // cut in half
    return full;
}

// Run fn in a child process; the parent reports whether it was killed by a signal,
// returning (signaled, signo). fork isolation keeps a SIGSEGV (if the fix is ever
// reverted) confined to the child so the gtest process survives and reports failure.
template <typename Fn>
static std::pair<bool, int> RunInChild(Fn fn)
{
    pid_t pid = fork();
    if (pid == 0) {
        // Child: just run the target; _exit on normal return.
        fn();
        _exit(0);
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    if (WIFSIGNALED(status)) {
        return {true, WTERMSIG(status)};
    }
    return {false, 0};
}
}  // namespace

// Guard 1: id-absent attack payload. The fixed version returns safely (no crash),
// leaving id empty.
TEST(LSPDeserGuardTest, MacroResultIdAbsentIsSafe)
{
    auto buf = BuildIdAbsentMacroMsg();
    ASSERT_FALSE(buf.empty());

    // content must be macroResult (sanity-check the payload was built correctly)
    EXPECT_EQ(MacroEvalMsgSerializer::GetMacroMsgContenType(buf),
              MacroMsgFormat::MsgContent_macroResult);

    auto [signaled, signo] = RunInChild([&] {
        std::string id = "UNTOUCHED";
        Cangjie::Position pos;
        // Fixed:  GetVerifiedMacroResult -> id()=nullptr -> Errorln + return, no crash.
        // Reverted: GetMacroMsg->content_as_macroResult()->id()->name() => SIGSEGV.
        MacroEvalMsgSerializer::DeSerializeIdInfoFromResult(id, pos, buf);
    });

    EXPECT_FALSE(signaled) << "DeSerializeIdInfoFromResult crashed on id-absent buffer (signal "
                           << signo << "), the LSP deserialization guard may have been reverted!";
    if (signaled) {
        GTEST_FAIL() << "LSP regression: nullptr dereference on result->id()->name() -> SIGSEGV. "
                     << "Confirm GetVerifiedMacroResult and the per-field null checks are present.";
    }
}

// Guard 2: truncated buffer. The Verifier must reject it without crashing.
TEST(LSPDeserGuardTest, TruncatedBufferIsRejected)
{
    auto buf = BuildTruncatedMacroMsg();

    auto [signaled, signo] = RunInChild([&] {
        std::string id;
        Cangjie::Position pos;
        MacroEvalMsgSerializer::DeSerializeIdInfoFromResult(id, pos, buf);
    });

    EXPECT_FALSE(signaled) << "Truncated buffer crashed (signal " << signo
                           << "), the Verifier did not intercept it!";
}

// Guard 3: assert the Verifier directly. The fixed VerifyMacroMsgBuffer returns
// false for a truncated buffer.
TEST(LSPDeserGuardTest, VerifyMacroMsgBufferRejectsTruncated)
{
    auto buf = BuildTruncatedMacroMsg();
    flatbuffers::Verifier verifier(buf.data(), buf.size(), 128, 2000000);
    EXPECT_FALSE(VerifyMacroMsgBuffer(verifier))
        << "a truncated buffer should be rejected by the Verifier";
}

// Guard 4: the Verifier returns true for a structurally-valid but id-absent buffer
// (an absent field is a legal default). This is exactly why the fix needs the dual
// defence of "Verifier + per-field null check" and cannot rely on the Verifier alone.
TEST(LSPDeserGuardTest, VerifyMacroMsgBufferAcceptsIdAbsentStruct)
{
    auto buf = BuildIdAbsentMacroMsg();
    flatbuffers::Verifier verifier(buf.data(), buf.size(), 128, 2000000);
    EXPECT_TRUE(VerifyMacroMsgBuffer(verifier))
        << "id-absent is structurally valid; the Verifier lets it through -- the field "
        << "null check is required to back it up";
}
