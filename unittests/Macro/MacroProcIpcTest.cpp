// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// MacroProcIpcTest
//
// Line-coverage tests for the Macro IPC message layer:
//   * src/Macro/MacroEvaluationClient.cpp (MacroProcMsger client half)
//   * src/Macro/MacroEvaluationSrv.cpp   (MacroProcMsger server half)
//
// The conventional LSP macro engine only walks the happy path (pipepair +
// 4KB-aligned single-frame messages inside a real LSPMacroServer child), so the
// partial-write/read slice loops, the pipeError branches, the multi-message
// select loop and the server task-type dispatch stay uncovered. This test
// drives them directly and in-process:
//
//   * The client framing layer (SendMsgToSrv / ReadMsgFromSrv /
//     ReadAllMsgFromSrv) is exercised on a socketpair that stands in for the
//     server peer; the test process itself writes/reads the same byte-level
//     framing into the pipes, reaching the slice loops, the select loop and
//     the error branches without spawning any process.
//   * The server framing layer (SendMsgToClient / ReadMsgFromClient) is driven
//     the same way with the pipe fds bound to the server's own ends.
//   * The real server loop (ExecuteEvalSrvTask, the task-type switch in
//     GetMacroTaskType, FindDef and EvalMacroCall) is driven in-process over a
//     pair of real pipes (mirroring src/main-macrosrv.cpp). The loop breaks on
//     the exit task (or on the first failing dispatch), and the test process
//     exits() normally afterwards so its coverage instrumentation can flush
//     profile data. Forking a server child would lose that child's profile on
//     _exit().
//   * Two non-blocking tests with a tiny pipe buffer reach the partial-write
//     slice loop inside WriteToSrvPipe / WriteToClientPipe.
//   * A second round of tests covers the still-open failure branches: the
//     *header* write failing (pipe pre-filled instead of payload-full), the
//     payload slice read hitting EOF on a truncated peer, the serialized exit
//     task failing to send because pipeError is latched, and (on the server)
//     ReadMsgFromClient truncation plus FindDef's reply failing to write.
//     Together they drive every "if (!...()) { perror/cerr; return false; }"
//     fall-back that the happy path never reaches.

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <list>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "flatbuffers/flatbuffers.h"
#include "flatbuffers/MacroMsgFormat_generated.h"

#include "cangjie/Macro/InvokeUtil.h"
#include "cangjie/Macro/MacroEvaluation.h"

using namespace Cangjie;

namespace {

// ----------------------------------------------------------------------------
// Byte-level framing helpers. MacroProcMsger frames every message as a size_t
// header (total payload size) followed by the payload, sliced into
// msgSliceLen-byte chunks. The helpers below mimic the framing so the test can
// act as the "remote" side of the pipe.
// ----------------------------------------------------------------------------

constexpr size_t kSliceLen = 4096;

// F_SETPIPE_SZ is a Linux-only fcntl command: macOS/BSD <fcntl.h> has no such
// macro, so calling it unconditionally would fail to compile off Linux.
// ShrinkPipeToTiny() tries to trim a non-blocking pipe to a 4KB buffer on Linux
// and tolerates failure (e.g. kernel enforcing a larger minimum); elsewhere it
// is a no-op and the pipe keeps its default capacity. The tests below only need
// the pipe to be far smaller than the 1MB payload (or to be fillable), which
// holds with any default capacity, so this guard never changes the assertion
// logic on any platform.
#ifdef F_SETPIPE_SZ
#define ShrinkPipeToTiny(fd) (void)fcntl((fd), F_SETPIPE_SZ, 4096)
#else
#define ShrinkPipeToTiny(fd) ((void)0)
#endif

bool ReadFully(int fd, uint8_t* buf, size_t size)
{
    size_t total = 0;
    while (total < size) {
        ssize_t n = read(fd, buf + total, size - total);
        if (n <= 0) {
            return false;
        }
        total += static_cast<size_t>(n);
    }
    return true;
}

bool WriteFully(int fd, const uint8_t* buf, size_t size)
{
    size_t total = 0;
    while (total < size) {
        ssize_t n = write(fd, buf + total, size - total);
        if (n <= 0) {
            return false;
        }
        total += static_cast<size_t>(n);
    }
    return true;
}

bool SendFrame(int fd, const std::vector<uint8_t>& msg)
{
    size_t sum = msg.size();
    return WriteFully(fd, reinterpret_cast<const uint8_t*>(&sum), sizeof(sum)) &&
        WriteFully(fd, msg.data(), msg.size());
}

bool RecvFrame(int fd, std::vector<uint8_t>& msg)
{
    size_t sum = 0;
    if (!ReadFully(fd, reinterpret_cast<uint8_t*>(&sum), sizeof(sum)) || sum == 0) {
        return false;
    }
    msg.resize(sum);
    return ReadFully(fd, msg.data(), sum);
}

// Fill a non-blocking pipe until write() reports EAGAIN, then return how many
// bytes were accepted. With an empty pipe and a tiny capacity this typically
// accepts one full buffer, guaranteeing a subsequent non-blocking write to the
// same fd fails instantly.
ssize_t FillPipeUntilFull(int writeFd)
{
    ssize_t total = 0;
    std::vector<uint8_t> chunk(4096, 0x7C);
    for (;;) {
        ssize_t n = write(writeFd, chunk.data(), chunk.size());
        if (n < 0) {
            EXPECT_EQ(EAGAIN, errno);  // pipe is genuinely full
            break;
        }
        total += n;
        if (n < static_cast<ssize_t>(chunk.size())) {
            break;  // truncated write: capacity reached
        }
    }
    return total;
}

// ----------------------------------------------------------------------------
// FlatBuffer message builders (schema: MacroMsgFormat.fbs).
// ----------------------------------------------------------------------------

MacroMsgFormat::Position MakePos(uint32_t fileID, int line, int column)
{
    return MacroMsgFormat::Position(fileID, line, column);
}

flatbuffers::Offset<MacroMsgFormat::IdInfo> BuildIdInfo(
    flatbuffers::FlatBufferBuilder& b, const char* name, uint32_t fileID)
{
    auto nameOff = b.CreateString(name);
    auto pos = MakePos(fileID, 1, 1);
    return MacroMsgFormat::CreateIdInfo(b, nameOff, &pos);
}

std::vector<uint8_t> BuildDefLibMsg(const std::vector<const char*>& paths)
{
    flatbuffers::FlatBufferBuilder b(256);
    std::vector<flatbuffers::Offset<flatbuffers::String>> offs;
    for (auto p : paths) {
        offs.push_back(b.CreateString(p));
    }
    auto pathsV = b.CreateVector(offs);
    auto cont = MacroMsgFormat::CreateDefLib(b, pathsV);
    auto msg = MacroMsgFormat::CreateMacroMsg(b, MacroMsgFormat::MsgContent_defLib, cont.Union());
    b.Finish(msg);
    return std::vector<uint8_t>(b.GetBufferPointer(), b.GetBufferPointer() + b.GetSize());
}

// MacroCall body with a deterministic id name; libPath points at a nonexistent
// library so FindMacroDefMethod fails fast in the server loop.
flatbuffers::Offset<MacroMsgFormat::MacroCall> BuildMacroCallBody(
    flatbuffers::FlatBufferBuilder& b, const char* id, uint32_t fileID)
{
    auto idInfo = BuildIdInfo(b, id, fileID);
    auto argsV = b.CreateVector(std::vector<flatbuffers::Offset<MacroMsgFormat::Token>>{});
    auto attrsV = b.CreateVector(std::vector<flatbuffers::Offset<MacroMsgFormat::Token>>{});
    auto parentsV = b.CreateVector(std::vector<flatbuffers::Offset<flatbuffers::String>>{});
    auto childrenV = b.CreateVector(std::vector<flatbuffers::Offset<MacroMsgFormat::ChildMsg>>{});
    auto methodName = b.CreateString("NoSuchM");
    auto packageName = b.CreateString("unittest");
    auto libPath = b.CreateString("/nonexistent/libmacro.so");
    auto pos = MakePos(fileID, 1, 1);
    return MacroMsgFormat::CreateMacroCall(b, idInfo, false, argsV, attrsV, parentsV, childrenV,
        methodName, packageName, libPath, &pos, &pos);
}

std::vector<uint8_t> BuildMultiCallsMsg(const std::vector<const char*>& ids, uint32_t firstFileID)
{
    flatbuffers::FlatBufferBuilder b(512);
    std::vector<flatbuffers::Offset<MacroMsgFormat::MacroCall>> offs;
    uint32_t fileID = firstFileID;
    for (auto id : ids) {
        offs.push_back(BuildMacroCallBody(b, id, fileID++));
    }
    auto callsV = b.CreateVector(offs);
    auto cont = MacroMsgFormat::CreateMultiMacroCalls(b, callsV);
    auto msg = MacroMsgFormat::CreateMacroMsg(b, MacroMsgFormat::MsgContent_multiCalls, cont.Union());
    b.Finish(msg);
    return std::vector<uint8_t>(b.GetBufferPointer(), b.GetBufferPointer() + b.GetSize());
}

std::vector<uint8_t> BuildExitMsg(bool flag)
{
    flatbuffers::FlatBufferBuilder b(64);
    auto cont = b.CreateStruct(MacroMsgFormat::ExitTask(flag));
    auto msg = MacroMsgFormat::CreateMacroMsg(b, MacroMsgFormat::MsgContent_exitTask, cont.Union());
    b.Finish(msg);
    return std::vector<uint8_t>(b.GetBufferPointer(), b.GetBufferPointer() + b.GetSize());
}

} // namespace

// =============================================================================
// Client-proc framing (MacroEvaluationClient.cpp: WriteToSrvPipe /
// ReadFromSrvPipe / SendMsgToSrv / ReadMsgFromSrv / ReadAllMsgFromSrv).
//
// The client writes server-bound messages into pipefdP2C[1] and reads
// client-bound messages from pipefdC2P[0]. Both are bound to fds_[0]; the test
// impersonates the server on fds_[1]. MacroProcMsger is a singleton, so each
// test rebinds the fds and resets pipeError in SetUp/TearDown.
// =============================================================================

class MacroProcMsgerClientTest : public testing::Test {
protected:
    void SetUp() override
    {
        ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds_));
        auto& instance = MacroProcMsger::GetInstance();
        instance.pipeError.store(false);
        instance.pipefdP2C[1] = fds_[0];
        instance.pipefdC2P[0] = fds_[0];
        peer_ = fds_[1];
    }

    void TearDown() override
    {
        auto& instance = MacroProcMsger::GetInstance();
        instance.pipefdP2C[0] = -1;
        instance.pipefdC2P[1] = -1;
        instance.pipefdP2C[1] = -1;
        instance.pipefdC2P[0] = -1;
        instance.pipeError.store(false);
        if (fds_[0] != -1) {
            close(fds_[0]);
        }
        if (peer_ != -1) {
            close(peer_);
        }
    }

    int fds_[2]{-1, -1};
    int peer_{-1};
};

TEST_F(MacroProcMsgerClientTest, ClientRoundTripLargeFraming)
{
    auto& instance = MacroProcMsger::GetInstance();
    // Larger than one slice: SendMsgToSrv must write header + multiple slices.
    std::vector<uint8_t> big(3 * kSliceLen + 123, 0xAB);
    EXPECT_TRUE(instance.SendMsgToSrv(big));

    std::vector<uint8_t> got;
    ASSERT_TRUE(RecvFrame(peer_, got));
    EXPECT_EQ(got, big);

    // Server -> client direction: frame a reply larger than a slice.
    std::vector<uint8_t> reply(2 * kSliceLen + 7, 0xCD);
    ASSERT_TRUE(SendFrame(peer_, reply));
    std::vector<uint8_t> gotReply;
    EXPECT_TRUE(instance.ReadMsgFromSrv(gotReply));
    EXPECT_EQ(gotReply, reply);
}

TEST_F(MacroProcMsgerClientTest, ReadAllMsgFromSrvMultipleMessages)
{
    auto& instance = MacroProcMsger::GetInstance();
    std::vector<uint8_t> m1(100, 0x11);
    std::vector<uint8_t> m2(100, 0x22);
    ASSERT_TRUE(SendFrame(peer_, m1));
    ASSERT_TRUE(SendFrame(peer_, m2));

    std::list<std::vector<uint8_t>> got;
    EXPECT_TRUE(instance.ReadAllMsgFromSrv(got));
    ASSERT_EQ(got.size(), 2u);
    auto it = got.begin();
    EXPECT_EQ(*it, m1);
    ++it;
    EXPECT_EQ(*it, m2);
}

TEST_F(MacroProcMsgerClientTest, ReadAllMsgFromSrvSingleMessageSelectShorted)
{
    auto& instance = MacroProcMsger::GetInstance();
    std::vector<uint8_t> m1(50, 0x33);
    ASSERT_TRUE(SendFrame(peer_, m1));

    std::list<std::vector<uint8_t>> got;
    EXPECT_TRUE(instance.ReadAllMsgFromSrv(got));
    ASSERT_EQ(got.size(), 1u);
    EXPECT_EQ(got.front(), m1);
}

TEST_F(MacroProcMsgerClientTest, SendMsgToSrvRejectsEmpty)
{
    auto& instance = MacroProcMsger::GetInstance();
    EXPECT_FALSE(instance.SendMsgToSrv({}));
}

TEST_F(MacroProcMsgerClientTest, SendMsgToSrvAfterPipeErrorRejected)
{
    auto& instance = MacroProcMsger::GetInstance();
    instance.pipeError.store(true);
    std::vector<uint8_t> msg(16, 0x42);
    EXPECT_FALSE(instance.SendMsgToSrv(msg));
}

TEST_F(MacroProcMsgerClientTest, ReadMsgFromSrvZeroHeaderSetsPipeError)
{
    auto& instance = MacroProcMsger::GetInstance();
    size_t zero = 0;
    ASSERT_TRUE(WriteFully(peer_, reinterpret_cast<const uint8_t*>(&zero), sizeof(zero)));
    std::vector<uint8_t> msg;
    EXPECT_FALSE(instance.ReadMsgFromSrv(msg));
    EXPECT_TRUE(instance.pipeError.load());
}

TEST_F(MacroProcMsgerClientTest, ReadMsgFromSrvPeerClosedFails)
{
    auto& instance = MacroProcMsger::GetInstance();
    close(peer_);
    peer_ = -1;
    std::vector<uint8_t> msg;
    EXPECT_FALSE(instance.ReadMsgFromSrv(msg));
    EXPECT_TRUE(instance.pipeError.load());
}

TEST_F(MacroProcMsgerClientTest, ReadAllMsgFromSrvPeerClosedFails)
{
    auto& instance = MacroProcMsger::GetInstance();
    close(peer_);
    peer_ = -1;
    std::list<std::vector<uint8_t>> got;
    EXPECT_FALSE(instance.ReadAllMsgFromSrv(got));
}

TEST_F(MacroProcMsgerClientTest, SendMsgToSrvPartialWriteHitBufferLimit)
{
    // A real pipe with a tiny buffer and a payload far larger than the pipe
    // forces partial writes inside WriteToSrvPipe (the loop resumes writing the
    // remaining bytes on each iteration) until the pipe is full, then the
    // writer reports an error and latches pipeError.
    int pipeFds[2];
    ASSERT_EQ(0, pipe(pipeFds));
    int flags = fcntl(pipeFds[1], F_GETFL, 0);
    ASSERT_NE(-1, flags);
    ASSERT_NE(-1, fcntl(pipeFds[1], F_SETFL, flags | O_NONBLOCK));
    ShrinkPipeToTiny(pipeFds[1]);

    auto& instance = MacroProcMsger::GetInstance();
    instance.pipeError.store(false);
    instance.pipefdP2C[1] = pipeFds[1];

    std::vector<uint8_t> huge(static_cast<size_t>(1) << 20, 0x99);
    EXPECT_FALSE(instance.SendMsgToSrv(huge));
    EXPECT_TRUE(instance.pipeError.load());

    close(pipeFds[0]);
    close(pipeFds[1]);
}

TEST_F(MacroProcMsgerClientTest, SendMsgToSrvHeaderWriteFails)
{
    // Pre-fill the pipe to capacity so even the size_t header cannot be
    // written; the first write returns immediately without transferring any
    // bytes and SendMsgToSrv reports the header failure (the branch that the
    // payload-full test above does not reach).
    int pipeFds[2];
    ASSERT_EQ(0, pipe(pipeFds));
    int flags = fcntl(pipeFds[1], F_GETFL, 0);
    ASSERT_NE(-1, flags);
    ASSERT_NE(-1, fcntl(pipeFds[1], F_SETFL, flags | O_NONBLOCK));
    ShrinkPipeToTiny(pipeFds[1]);

    // Pre-fill the pipe so the size_t header itself cannot be written.
    ASSERT_TRUE(FillPipeUntilFull(pipeFds[1]) > 0);

    auto& instance = MacroProcMsger::GetInstance();
    instance.pipeError.store(false);
    instance.pipefdP2C[1] = pipeFds[1];

    std::vector<uint8_t> small(64, 0x6B);
    EXPECT_FALSE(instance.SendMsgToSrv(small));
    EXPECT_TRUE(instance.pipeError.load());

    close(pipeFds[0]);
    close(pipeFds[1]);
}

TEST_F(MacroProcMsgerClientTest, ReadMsgFromSrvPipeErrorGateRejects)
{
    auto& instance = MacroProcMsger::GetInstance();
    instance.pipeError.store(true);
    std::vector<uint8_t> msg;
    EXPECT_FALSE(instance.ReadMsgFromSrv(msg));
}

TEST_F(MacroProcMsgerClientTest, ReadMsgFromSrvPayloadTruncatedFails)
{
    // A message spanning multiple slices: header + first slice are delivered,
    // then the peer closes before the remaining payload slice. The first
    // ReadFromSrvPipe fills a partial buffer and the slice loop re-reads,
    // hitting EOF (0) on the second slice, which trips the fail branch inside
    // the payload loop.
    auto& instance = MacroProcMsger::GetInstance();
    size_t payloadSize = kSliceLen + 64;  // header + slice0(4096) + slice1(64)
    std::vector<uint8_t> payload(payloadSize, 0x2D);
    ASSERT_TRUE(WriteFully(peer_, reinterpret_cast<const uint8_t*>(&payloadSize), sizeof(payloadSize)));
    ASSERT_TRUE(WriteFully(peer_, payload.data(), kSliceLen));  // only first slice
    close(peer_);  // the remaining payload slice hits EOF
    peer_ = -1;
    std::vector<uint8_t> msg;
    EXPECT_FALSE(instance.ReadMsgFromSrv(msg));
    EXPECT_TRUE(instance.pipeError.load());
}

// =============================================================================
// Server-proc framing (MacroEvaluationSrv.cpp: WriteToClientPipe /
// ReadFromClientPipe / SendMsgToClient / ReadMsgFromClient).
// =============================================================================

class MacroProcMsgerSrvTest : public testing::Test {
protected:
    void SetUp() override
    {
        ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds_));
        auto& instance = MacroProcMsger::GetInstance();
        instance.pipeError.store(false);
    }

    void TearDown() override
    {
        auto& instance = MacroProcMsger::GetInstance();
        instance.pipefdP2C[0] = -1;
        instance.pipefdC2P[0] = -1;
        instance.pipefdC2P[1] = -1;
        if (fds_[0] != -1) {
            close(fds_[0]);
        }
        if (fds_[1] != -1) {
            close(fds_[1]);
        }
    }

    int fds_[2]{-1, -1};
};

TEST_F(MacroProcMsgerSrvTest, SrvSendToClientLargeThenReadFromClient)
{
    auto& instance = MacroProcMsger::GetInstance();
    // SendMsgToClient writes into pipefdC2P[1]; bind it so the test reads back
    // the framed payload from the peer.
    instance.pipefdC2P[1] = fds_[0];
    std::vector<uint8_t> big(2 * kSliceLen + 55, 0xEE);
    EXPECT_TRUE(instance.SendMsgToClient(big));

    std::vector<uint8_t> got;
    ASSERT_TRUE(RecvFrame(fds_[1], got));
    EXPECT_EQ(got, big);

    // ReadMsgFromClient reads from pipefdP2C[0]; bind the peer with a request.
    instance.pipefdP2C[0] = fds_[1];
    std::vector<uint8_t> req(6000, 0xFF);
    ASSERT_TRUE(SendFrame(fds_[0], req));
    std::vector<uint8_t> gotReq;
    EXPECT_TRUE(instance.ReadMsgFromClient(gotReq));
    EXPECT_EQ(gotReq, req);
}

TEST_F(MacroProcMsgerSrvTest, SrvRejectsEmptyClientMsg)
{
    auto& instance = MacroProcMsger::GetInstance();
    instance.pipefdC2P[1] = fds_[0];
    EXPECT_FALSE(instance.SendMsgToClient({}));
}

TEST_F(MacroProcMsgerSrvTest, SrvReadFromClientZeroHeaderFails)
{
    auto& instance = MacroProcMsger::GetInstance();
    instance.pipefdP2C[0] = fds_[1];
    size_t zero = 0;
    ASSERT_TRUE(WriteFully(fds_[0], reinterpret_cast<const uint8_t*>(&zero), sizeof(zero)));
    std::vector<uint8_t> msg;
    EXPECT_FALSE(instance.ReadMsgFromClient(msg));
    EXPECT_FALSE(instance.pipeError.load());
}

TEST_F(MacroProcMsgerSrvTest, SrvReadFromClientClosedPeerFails)
{
    auto& instance = MacroProcMsger::GetInstance();
    instance.pipefdP2C[0] = fds_[1];
    close(fds_[0]);
    fds_[0] = -1;
    std::vector<uint8_t> msg;
    EXPECT_FALSE(instance.ReadMsgFromClient(msg));
    EXPECT_FALSE(instance.pipeError.load());
}

TEST_F(MacroProcMsgerSrvTest, SrvSendToClientPartialWriteHitBufferLimit)
{
    // Same partial-write trick for the server writer: tiny pipe buffer,
    // non-blocking, payload far larger than the pipe.
    int pipeFds[2];
    ASSERT_EQ(0, pipe(pipeFds));
    int flags = fcntl(pipeFds[1], F_GETFL, 0);
    ASSERT_NE(-1, flags);
    ASSERT_NE(-1, fcntl(pipeFds[1], F_SETFL, flags | O_NONBLOCK));
    ShrinkPipeToTiny(pipeFds[1]);

    auto& instance = MacroProcMsger::GetInstance();
    instance.pipefdC2P[1] = pipeFds[1];

    std::vector<uint8_t> huge(static_cast<size_t>(1) << 20, 0x77);
    EXPECT_FALSE(instance.SendMsgToClient(huge));

    close(pipeFds[0]);
    close(pipeFds[1]);
}

TEST_F(MacroProcMsgerSrvTest, SrvSendToClientHeaderWriteFails)
{
    // Server reply path: pre-fill a non-blocking pipe so even the size_t
    // header write in SendMsgToClient fails immediately; this is the
    // header-failure branch (perror/return false) that the payload-full test
    // above does not reach.
    int pipeFds[2];
    ASSERT_EQ(0, pipe(pipeFds));
    int flags = fcntl(pipeFds[1], F_GETFL, 0);
    ASSERT_NE(-1, flags);
    ASSERT_NE(-1, fcntl(pipeFds[1], F_SETFL, flags | O_NONBLOCK));
    ShrinkPipeToTiny(pipeFds[1]);
    ASSERT_TRUE(FillPipeUntilFull(pipeFds[1]) > 0);

    auto& instance = MacroProcMsger::GetInstance();
    instance.pipefdC2P[1] = pipeFds[1];

    std::vector<uint8_t> small(64, 0x3C);
    EXPECT_FALSE(instance.SendMsgToClient(small));

    close(pipeFds[0]);
    close(pipeFds[1]);
}

TEST_F(MacroProcMsgerSrvTest, SrvReadFromClientPayloadTruncatedFails)
{
    // A multi-slice message whose first slice is delivered before the write end
    // closes; the second ReadFromClientPipe inside the payload loop hits EOF.
    auto& instance = MacroProcMsger::GetInstance();
    instance.pipefdP2C[0] = fds_[1];

    size_t payloadSize = kSliceLen + 128;
    std::vector<uint8_t> payload(payloadSize, 0x21);
    ASSERT_TRUE(WriteFully(fds_[0], reinterpret_cast<const uint8_t*>(&payloadSize), sizeof(payloadSize)));
    ASSERT_TRUE(WriteFully(fds_[0], payload.data(), kSliceLen));
    close(fds_[0]);
    fds_[0] = -1;

    std::vector<uint8_t> msg;
    EXPECT_FALSE(instance.ReadMsgFromClient(msg));
}

// =============================================================================
// Real server loop (MacroEvaluationSrv.cpp: ExecuteEvalSrvTask /
// GetMacroTaskType / FindDef / EvalMacroCall). Driven in-process: the
// server-side pipe fds (SetSrvPipeHandle) point at real pipes whose other ends
// the test owns, exactly like main-macrosrv.cpp sets them up. The loop reads a
// task, dispatches on GetMacroTaskType, responds when needed and continues;
// each test arranges for the loop to break, and the enclosing gtest process
// then exits() normally so coverage data is flushed.
// =============================================================================

class MacroSrvLoopTest : public testing::Test {
protected:
    void SetUp() override
    {
        ASSERT_EQ(0, pipe(toSrv_));
        ASSERT_EQ(0, pipe(fromSrv_));
        auto& instance = MacroProcMsger::GetInstance();
        instance.pipeError.store(false);
        // Server halves: read requests on toSrv_[0], write responses into fromSrv_[1].
        instance.SetSrvPipeHandle(toSrv_[0], fromSrv_[1]);
    }

    void TearDown() override
    {
        auto& instance = MacroProcMsger::GetInstance();
        instance.SetSrvPipeHandle(-1, -1);
        if (toSrv_[0] != -1) {
            close(toSrv_[0]);
        }
        if (toSrv_[1] != -1) {
            close(toSrv_[1]);
        }
        if (fromSrv_[0] != -1) {
            close(fromSrv_[0]);
        }
        if (fromSrv_[1] != -1) {
            close(fromSrv_[1]);
        }
        toSrv_[0] = toSrv_[1] = fromSrv_[0] = fromSrv_[1] = -1;
    }

    // Build a fresh evaluator and run the real server loop until it breaks.
    void RunSrvLoop()
    {
        DiagnosticEngine diag;
        CompilerInvocation invocation;
        invocation.globalOptions.executablePath = "/tmp";
        CompilerInstance ci(invocation, diag);
        MacroCollector macroCollector;
        MacroEvaluation evaluator(&ci, &macroCollector, false);
        evaluator.ExecuteEvalSrvTask();
    }

    int toSrv_[2]{-1, -1};
    int fromSrv_[2]{-1, -1};
};

TEST_F(MacroSrvLoopTest, SrvLoopDeflibRespondThenExit)
{
    // A real macro library path would be dlopen'd; use a relative one so FindDef
    // fails to open it, responds with the failing lib, and the exit frame that
    // follows breaks the loop.
    ASSERT_TRUE(SendFrame(toSrv_[1], BuildDefLibMsg({"./libunit.a"})));
    ASSERT_TRUE(SendFrame(toSrv_[1], BuildExitMsg(true)));

    RunSrvLoop();

    // FindDef always answers "RespondFindDef <lib>" on failure.
    std::vector<uint8_t> resp;
    ASSERT_TRUE(RecvFrame(fromSrv_[0], resp));
    std::string text(resp.begin(), resp.end());
    EXPECT_NE(text.find("RespondFindDef "), std::string::npos);
}

TEST_F(MacroSrvLoopTest, SrvLoopFindDefReplyWriteFailsExits)
{
    // FindDef dlopens the lib (fails on "./libunit.a") and then replies
    // "RespondFindDef ..." to the client. Pre-fill the server->client pipe so
    // that reply write fails; FindDef returns false and the loop exits with
    // "Macro srv find define fail" (the branch the happy path never hits).
    int flags = fcntl(fromSrv_[1], F_GETFL, 0);
    ASSERT_NE(-1, flags);
    ASSERT_NE(-1, fcntl(fromSrv_[1], F_SETFL, flags | O_NONBLOCK));
    ShrinkPipeToTiny(fromSrv_[1]);
    ASSERT_TRUE(FillPipeUntilFull(fromSrv_[1]) > 0);

    ASSERT_TRUE(SendFrame(toSrv_[1], BuildDefLibMsg({"./libunit.a"})));
    RunSrvLoop();  // exits without delivering a reply
}

TEST_F(MacroSrvLoopTest, SrvLoopEvalMacroCallFailExits)
{
    // multiCalls frame whose libPath does not exist: FindMacroDefMethod fails,
    // EvalMacroCall returns false and ExecuteEvalSrvTask breaks the loop.
    ASSERT_TRUE(SendFrame(toSrv_[1], BuildMultiCallsMsg({"NoSuchMethod1"}, 100)));

    RunSrvLoop();
}

TEST_F(MacroSrvLoopTest, SrvLoopExitStgResetThenExit)
{
    // exitTask flag=false -> EXIT_MACRO_STG: ResetForNextEval, keep looping,
    // then the exit frame breaks it.
    ASSERT_TRUE(SendFrame(toSrv_[1], BuildExitMsg(false)));
    ASSERT_TRUE(SendFrame(toSrv_[1], BuildExitMsg(true)));

    RunSrvLoop();
}

TEST_F(MacroSrvLoopTest, SrvLoopMalformedMsgExits)
{
    // A buffer that fails flatbuffers verification: GetMacroTaskType routes
    // NONE -> EXIT_MACRO_SRV -> break loop (safe exit).
    std::vector<uint8_t> junk = {'x', 'y', 'z', 0x00, 0x01, 0x02, 0x03};
    ASSERT_TRUE(SendFrame(toSrv_[1], junk));

    RunSrvLoop();
}

TEST_F(MacroSrvLoopTest, SrvLoopClosedClientPipeExits)
{
    // Closing the client->server write end makes ReadMsgFromClient fail, print
    // "read message fail" and break the loop.
    close(toSrv_[1]);
    toSrv_[1] = -1;

    RunSrvLoop();
}