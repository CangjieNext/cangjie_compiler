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

#include <array>
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
// Fields of MacroResult that a caller may want to be absent on purpose. Each
// builder below deliberately omits a subset of these to reach a specific
// "field is a legal default (absent)" guard branch in the deserializers.
struct EmptyResult {
    bool withoutTks{true};
    bool withoutItems{true};
    bool withoutAssertParents{true};
    bool withoutDiags{true};
    bool withoutId{true};
    uint8_t status{0};
};

// A valid FlatBuffer MacroMsg{content=macroResult}, with the fields marked in
// \p ctl left at their (legal) defaults so the Verifier accepts the buffer but
// the corresponding field accessor returns nullptr.
std::vector<uint8_t> BuildMacroResultMsg(const EmptyResult& ctl);

// Build an ItemInfo with the given union payload. \p type selects the union
// branch; the value pointer must match that branch (e.g. a CreateString offset
// for OptionValue_sValue, or a CreateStruct(IntValue) offset for iValue).
Offset<ItemInfo> BuildItemInfo(FlatBufferBuilder& b, const char* key, OptionValue type,
    Offset<void> value)
{
    auto k = b.CreateString(key);
    return CreateItemInfo(b, k, type, value);
}

// MacroResult{id occupied exactly} (identifier + pos), nothing else.
Offset<IdInfo> BuildIdOffsets(FlatBufferBuilder& b)
{
    auto name = b.CreateString("M");
    auto pos = MacroMsgFormat::Position(1, 1, 1);
    return CreateIdInfo(b, name, &pos);
}

Position MakePos(uint32_t fileID, int line, int column)
{
    return Position(fileID, line, column);
}
} // namespace

// A valid MacroMsg{content=macroResult} where \p ctl marks which fields should
// be absent. Fields not mentioned in \p ctl (e.g. id.name) are given a
// concrete value so the buffer stays structurally valid while exercising only
// the intended absent-field branch.
std::vector<uint8_t> BuildMacroResultMsg(const EmptyResult& ctl)
{
    flatbuffers::FlatBufferBuilder b(128);
    Offset<IdInfo> idOff;
    if (!ctl.withoutId) {
        idOff = BuildIdOffsets(b);
    }
    std::vector<Offset<MacroMsgFormat::String>> emptyVec;
    auto resOff = CreateMacroResult(
        b, idOff, ctl.status,
        ctl.withoutTks ? 0 : b.CreateVector(emptyVec),
        ctl.withoutItems ? 0 : b.CreateVector(emptyVec),
        ctl.withoutAssertParents ? 0 : b.CreateVector(emptyVec),
        ctl.withoutDiags ? 0 : b.CreateVector(emptyVec));
    auto msgOff = CreateMacroMsg(b, MsgContent_macroResult, resOff.Union());
    b.Finish(msgOff);
    return std::vector<uint8_t>(b.GetBufferPointer(), b.GetBufferPointer() + b.GetSize());
}

// A valid MacroResult{id absent}
// (a legal default; needs a test because nullable id() must not be dereferenced).
std::vector<uint8_t> BuildIdAbsentMacroMsg()
{
    return BuildMacroResultMsg(EmptyResult{});
}

// A valid MacroResult that carries exactly the given ItemInfo records.
std::vector<uint8_t> BuildMacroResultItemsMsg(const std::vector<Offset<ItemInfo>>& items,
    uint8_t status)
{
    flatbuffers::FlatBufferBuilder b(128);
    auto idOff = BuildIdOffsets(b);
    auto itemsOff = (items.empty()) ? 0 : b.CreateVector(items);
    auto resOff = CreateMacroResult(b, idOff, status, 0, itemsOff, 0, 0);
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

// ---------------------------------------------------------------------------
// P1: absent-field guard coverage for the DeSerialize*FromResult family.
// Each case feeds a structurally-valid MacroMsg buffer (the Verifier passes)
// that is deliberately missing the field the deserializer reads, and asserts
// the deserializer returns without crashing and leaves the out-param in the
// state you would expect from the "field absent" early-return branch.
// ---------------------------------------------------------------------------

// Result with neither tks nor diags nor items nor assertParents nor id:
// every DeSerialize*FromResult helper must survive the GetVerifiedMacroResult
// nullptr early-return (the buffer is valid but the union content is absent).
TEST(LSPDeserGuardTest, DeserializeAllFromResultWithEmptyResult)
{
    auto buf = BuildMacroResultMsg(EmptyResult{});

    std::vector<Cangjie::Token> tks(1, Cangjie::Token(Cangjie::TokenKind::IDENT, "pre"));
    MacroEvalMsgSerializer::DeSerializeTksFromResult(tks, buf); // 540-541
    EXPECT_TRUE(tks.empty());                                   // tks cleared first

    std::vector<Cangjie::Diagnostic> diags(1, Cangjie::Diagnostic());
    MacroEvalMsgSerializer::DeSerializeDiagsFromResult(diags, buf); // 555-556
    EXPECT_TRUE(diags.empty());

    std::vector<Cangjie::ItemInfo> items(1);
    MacroEvalMsgSerializer::DeSerializeItemsFromResult(items, buf); // 579-580
    EXPECT_TRUE(items.empty());

    std::vector<std::string> assertParents(1, "pre");
    MacroEvalMsgSerializer::DeSerializeAssertParentsFromResult(assertParents, buf); // 594-595
    EXPECT_TRUE(assertParents.empty());
}

// Each field individually present as an *empty* vector: the field accessor is
// non-null, so the deserializers go past the nullptr guard and clear again.
TEST(LSPDeserGuardTest, DeserializeFromResultWithPresentEmptyFields)
{
    auto buf = BuildMacroResultMsg(EmptyResult{ .withoutTks = false, .withoutItems = false,
                                               .withoutAssertParents = false, .withoutDiags = false,
                                               .withoutId = false });

    std::vector<Cangjie::Token> tks(1, Cangjie::Token(Cangjie::TokenKind::IDENT, "pre"));
    MacroEvalMsgSerializer::DeSerializeTksFromResult(tks, buf);
    EXPECT_TRUE(tks.empty());

    std::vector<Cangjie::Diagnostic> diags(1, Cangjie::Diagnostic());
    MacroEvalMsgSerializer::DeSerializeDiagsFromResult(diags, buf);
    EXPECT_TRUE(diags.empty());

    std::vector<std::string> assertParents(1, "pre");
    MacroEvalMsgSerializer::DeSerializeAssertParentsFromResult(assertParents, buf);
    EXPECT_TRUE(assertParents.empty());
}

// The absent-item path of DeserializeItemsFromItemsBuf: a structurally-valid
// result whose single item has no key (key() == nullptr). The Verifier accepts
// it -- an absent string field is a legal default -- and the deserializer must
// skip the item instead of returning an out-of-range vector element.
TEST(LSPDeserGuardTest, DeserializeItemsFromResultSkipsKeyAbsentItem)
{
    FlatBufferBuilder b(128);
    // key intentionally omitted from the item.
    auto itemOff = CreateItemInfo(b, 0, OptionValue_sValue,
        b.CreateString("val").Union());
    auto buf = BuildMacroResultItemsMsg({itemOff});

    auto [signaled, signo] = RunInChild([&] {
        std::vector<Cangjie::ItemInfo> items(1);
        MacroEvalMsgSerializer::DeSerializeItemsFromResult(items, buf);
        // key==null item -> Errorln("... key is null") + continue: items[0]
        // keeps its default state ("", STRING default/whatever), never a crash
        // on the absent key.
        (void)items[0].key; // no SEGV is the assertion; see signaled below
    });

    EXPECT_FALSE(signaled) << "DeserializeItemsFromItemsBuf crashed on key-absent item (signal "
                           << signo << "): the key() null guard may have been reverted!";
}

// A MacroResult carrying all four ItemKind value types round-trips through
// DeSerializeItemsFromResult (ItemKind enum -> union selector -> ItemKind back),
// covering the ItemInfo switch in the items deserializer.
TEST(LSPDeserGuardTest, DeserializeItemsFromResultAllKindsRoundtrip)
{
    FlatBufferBuilder b(128);
    auto sVal = b.CreateString("sv");
    auto strItem = BuildItemInfo(b, "s", OptionValue_sValue, sVal.Union());
    auto iItem = BuildItemInfo(b, "i", OptionValue_iValue,
        b.CreateStruct(MacroMsgFormat::IntValue(42)).Union());
    auto boItem = BuildItemInfo(b, "bo", OptionValue_bValue,
        b.CreateStruct(MacroMsgFormat::BoolValue(true)).Union());
    auto tItem = BuildItemInfo(b, "t", OptionValue_tValue,
        CreateTokensValue(b, 0).Union()); // empty token vector -> no contents
    auto buf = BuildMacroResultItemsMsg({strItem, iItem, boItem, tItem});

    std::vector<Cangjie::ItemInfo> items;
    MacroEvalMsgSerializer::DeSerializeItemsFromResult(items, buf);

    ASSERT_EQ(items.size(), 4u);
    EXPECT_EQ(items[0].kind, Cangjie::ItemKind::STRING);
    EXPECT_EQ(items[0].sValue, "sv");
    EXPECT_EQ(items[1].kind, Cangjie::ItemKind::INT);
    EXPECT_EQ(items[1].iValue, 42);
    EXPECT_EQ(items[2].kind, Cangjie::ItemKind::BOOL);
    EXPECT_EQ(items[2].bValue, true);
    EXPECT_EQ(items[3].kind, Cangjie::ItemKind::TKS);
    EXPECT_TRUE(items[3].tValue.empty());
}

// The ItemKind::TKS item also decodes its embedded token vector: two tokens,
// each with delimiterNum and begin/end positions preserved.
TEST(LSPDeserGuardTest, DeserializeItemsFromResultDecodesTksItem)
{
    FlatBufferBuilder b(128);
    auto tOff = CreateToken(b, static_cast<uint8_t>(Cangjie::TokenKind::INTEGER),
        b.CreateString("17"), MakePos(1, 2, 3), MakePos(1, 2, 5), 4);
    auto tokensOff = CreateTokensValue(b, b.CreateVector(std::vector<Offset<MacroMsgFormat::Token>>{tOff}));
    auto tItem = BuildItemInfo(b, "t", OptionValue_tValue, tokensOff.Union());
    auto buf = BuildMacroResultItemsMsg({tItem});

    std::vector<Cangjie::ItemInfo> items;
    MacroEvalMsgSerializer::DeSerializeItemsFromResult(items, buf);

    ASSERT_EQ(items.size(), 1u);
    ASSERT_EQ(items[0].tValue.size(), 1u);
    EXPECT_EQ(items[0].tValue[0].Value(), "17");
    EXPECT_EQ(items[0].tValue[0].kind, Cangjie::TokenKind::INTEGER);
    EXPECT_EQ(items[0].tValue[0].delimiterNum, 4u);
    EXPECT_EQ(items[0].tValue[0].Begin().fileID, 1u);
    EXPECT_EQ(items[0].tValue[0].Begin().line, 2);
    EXPECT_EQ(items[0].tValue[0].Begin().column, 3);
}

// A status value that does not map to a valid MacroEvalStatus enumerator.
// The deserializer does a raw static_cast from the wire byte; the contract is
// that the result is read back bit-for-bit (e.g. a debug build may mask it).
TEST(LSPDeserGuardTest, DeSerializeStatusFromResultReadsRawByte)
{
    auto buf = BuildMacroResultMsg(EmptyResult{ .withoutId = false, .status = 0xFF });
    auto status = MacroEvalMsgSerializer::DeSerializeStatusFromResult(buf);
    EXPECT_EQ(static_cast<uint8_t>(status), 0xFF);
}

// A result whose id.name is absent -> "id.name is absent" guard (not a crash).
TEST(LSPDeserGuardTest, DeSerializeIdInfoFromResultNameAbsent)
{
    FlatBufferBuilder b(128);
    auto idNoName = CreateIdInfo(b, 0, MakePos(1, 1, 1)); // name omitted
    auto resOff = CreateMacroResult(b, idNoName, 0, 0, 0, 0, 0);
    auto msgOff = CreateMacroMsg(b, MsgContent_macroResult, resOff.Union());
    b.Finish(msgOff);
    std::vector<uint8_t> buf(b.GetBufferPointer(), b.GetBufferPointer() + b.GetSize());

    std::string id = "UNTOUCHED";
    Cangjie::Position pos;
    MacroEvalMsgSerializer::DeSerializeIdInfoFromResult(id, pos, buf);
    EXPECT_EQ(id, "UNTOUCHED"); // early-return left it alone
}

// A result whose id.pos is absent -> "id.pos is absent" guard.
TEST(LSPDeserGuardTest, DeSerializeIdInfoFromResultPosAbsent)
{
    FlatBufferBuilder b(128);
    auto idNoPos = CreateIdInfo(b, b.CreateString("M"), 0); // pos omitted
    auto resOff = CreateMacroResult(b, idNoPos, 0, 0, 0, 0, 0);
    auto msgOff = CreateMacroMsg(b, MsgContent_macroResult, resOff.Union());
    b.Finish(msgOff);
    std::vector<uint8_t> buf(b.GetBufferPointer(), b.GetBufferPointer() + b.GetSize());

    std::string id = "UNTOUCHED";
    Cangjie::Position pos;
    MacroEvalMsgSerializer::DeSerializeIdInfoFromResult(id, pos, buf);
    EXPECT_EQ(id, "UNTOUCHED");
}

// A MacroResult carrying a non-empty assertParents list decodes the strings
// (and the assertParents() null guard above it is skipped for a valid buffer).
TEST(LSPDeserGuardTest, DeSerializeAssertParentsFromResultDecodesList)
{
    FlatBufferBuilder b(128);
    auto idOff = BuildIdOffsets(b);
    std::vector<Offset<String>> names;
    names.push_back(b.CreateString("parent.m1"));
    names.push_back(b.CreateString("parent.m2"));
    auto resOff = CreateMacroResult(b, idOff, 0, 0, 0, b.CreateVector(names), 0);
    auto msgOff = CreateMacroMsg(b, MsgContent_macroResult, resOff.Union());
    b.Finish(msgOff);
    std::vector<uint8_t> buf(b.GetBufferPointer(), b.GetBufferPointer() + b.GetSize());

    std::vector<std::string> assertParents;
    MacroEvalMsgSerializer::DeSerializeAssertParentsFromResult(assertParents, buf);

    ASSERT_EQ(assertParents.size(), 2u);
    EXPECT_EQ(assertParents[0], "parent.m1");
    EXPECT_EQ(assertParents[1], "parent.m2");
}
