// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements the Macro evaluation messages serializer for macro evaluation.
 */
#include "cangjie/Macro/MacroEvalMsgSerializer.h"
#include "cangjie/Macro/TokenSerialization.h"

using namespace MacroMsgFormat;
using namespace flatbuffers;
using namespace Cangjie;

namespace {
static const int MACRO_SETITEM_INFO_NUM = 3;


#ifndef NDEBUG
template <typename... Args> static void MacroMsgErrorln(Args&&... args) noexcept
{
    std::cerr << RED_ERROR_MARK;
    ((std::cerr << args), ...);
    std::cerr << std::endl;
}
#else
template <typename... Args> static void MacroMsgErrorln([[maybe_unused]] Args&&... args) noexcept {}
#endif

// FlatBuffers verification limits. The macro message crosses the process
// boundary (LSPServer <-> LSPMacroServer) and is therefore untrusted input.
// Keep aligned with the other untrusted deserializers that share these limits:
//   - AST serialization (ASTLoader)
//   - Incremental compilation (CachedDataSerialization)
// (Those reuse FB_MAX_DEPTH / FB_MAX_TABLES; the values here mirror them.)
static constexpr int MACRO_MSG_MAX_DEPTH = 128;
static constexpr int MACRO_MSG_MAX_TABLES = 2000000;

// Verify the integrity of a MacroMsg buffer before any field is dereferenced.
// Returns the verified root pointer on success, or nullptr if the buffer is
// malformed (truncated / OOB offsets / oversized vectors / absent union
// content). Callers MUST treat nullptr as "drop the message" and never touch
// any field, otherwise a crafted buffer (e.g. MacroResult with an absent id
// field) leads to nullptr dereference and crashes the consumer process.
static const MacroMsgFormat::MacroMsg *GetVerifiedMacroMsg(const std::vector<uint8_t> &bufferData)
{
    if (bufferData.empty()) {
        MacroMsgErrorln("MacroMsg buffer is empty");
        return nullptr;
    }
    flatbuffers::Verifier verifier(bufferData.data(), bufferData.size(),
        MACRO_MSG_MAX_DEPTH, MACRO_MSG_MAX_TABLES);
    if (!VerifyMacroMsgBuffer(verifier)) {
        MacroMsgErrorln("MacroMsg buffer verification failed");
        return nullptr;
    }
    return GetMacroMsg(bufferData.data());
}

// Verify the buffer and unwrap it as a MacroResult. Returns nullptr (with a
// uniform "<caller>: ... is absent" log) whenever the buffer is malformed or
// the content is not a MacroResult, so every DeSerialize*FromResult caller can
// branch on a single nullptr check instead of repeating the verify + unwrap +
// field-null-check boilerplate.
static const MacroMsgFormat::MacroResult *GetVerifiedMacroResult(const std::vector<uint8_t> &bufferData,
                                                                 const char *caller)
{
    auto msg = GetVerifiedMacroMsg(bufferData);
    if (msg == nullptr) {
        return nullptr;
    }
    auto result = msg->content_as_macroResult();
    if (result == nullptr) {
        MacroMsgErrorln(std::string(caller) + ": macroResult is absent");
    }
    return result;
}

static auto CreateTokenVec(FlatBufferBuilder& builder, const std::vector<Cangjie::Token>& tks)
{
    std::vector<Offset<MacroMsgFormat::Token>> offsetVec;
    for (auto& tk : tks) {
        auto tbegin = MacroMsgFormat::Position(tk.Begin().fileID, tk.Begin().line, tk.Begin().column);
        auto tend = MacroMsgFormat::Position(tk.End().fileID, tk.End().line, tk.End().column);
        offsetVec.push_back(CreateToken(
            builder, static_cast<uint8_t>(tk.kind), builder.CreateString(tk.Value()), &tbegin, &tend, tk.delimiterNum));
    }
    return offsetVec;
}

static void GetChildMessages(std::vector<ChildMessage>& childMessages, const Cangjie::MacroCall& macCall)
{
    for (auto& child : macCall.children) {
        if (!child->items.empty()) {
            childMessages.emplace_back(ChildMessage{child->GetFullName(), child->items});
        }
        GetChildMessages(childMessages, *child);
    }
}

static auto CreatItemInfoVec(FlatBufferBuilder& builder, const std::vector<Cangjie::ItemInfo>& items)
{
    std::vector<Offset<MacroMsgFormat::ItemInfo>> itemsVec;
    for (auto& it : items) {
        auto key = builder.CreateString(it.key);
        switch (it.kind) {
            case ItemKind::STRING: {
                auto str = builder.CreateString(it.sValue);
                itemsVec.push_back(CreateItemInfo(builder, key, OptionValue_sValue, str.Union()));
                break;
            }
            case ItemKind::INT: {
                auto i = builder.CreateStruct(IntValue(it.iValue));
                itemsVec.push_back(CreateItemInfo(builder, key, OptionValue_iValue, i.Union()));
                break;
            }
            case ItemKind::BOOL: {
                auto b = builder.CreateStruct(BoolValue(it.bValue));
                itemsVec.push_back(CreateItemInfo(builder, key, OptionValue_bValue, b.Union()));
                break;
            }
            case ItemKind::TKS: {
                auto tksVec = builder.CreateVector(CreateTokenVec(builder, it.tValue));
                auto t = CreateTokensValue(builder, tksVec);
                itemsVec.push_back(CreateItemInfo(builder, key, OptionValue_tValue, t.Union()));
                break;
            }
            default:
                CJC_ABORT();
                break;
        }
    }
    return itemsVec;
}

static auto CreateDiagVec(FlatBufferBuilder& builder, const std::vector<Cangjie::Diagnostic>& diags)
{
    std::vector<Offset<MacroMsgFormat::Diagnostic>> offsetVec;
    for (auto& diag : diags) {
        auto& beginPos = diag.mainHint.range.begin;
        auto& endPos = diag.mainHint.range.end;
        auto begin = MacroMsgFormat::Position(beginPos.fileID, beginPos.line, beginPos.column);
        auto end = MacroMsgFormat::Position(endPos.fileID, endPos.line, endPos.column);

        auto errorMessage = builder.CreateString(diag.errorMessage);
        auto mainHint = builder.CreateString(diag.mainHint.str);

        offsetVec.push_back(
            CreateDiagnostic(builder, static_cast<int>(diag.diagSeverity), &begin, &end, errorMessage, mainHint));
    }
    return offsetVec;
}

static auto CreateMacroCall(FlatBufferBuilder& builder, const Cangjie::MacroCall& macCall)
{
    auto& inv = *(macCall.GetInvocation());
    auto& idPos = inv.macroCallDiagInfo.identifierPos;
    auto pos = MacroMsgFormat::Position(idPos.fileID, idPos.line, idPos.column);
    auto id = CreateIdInfo(builder, builder.CreateString(inv.macroCallDiagInfo.identifier), &pos);

    auto args = builder.CreateVector(CreateTokenVec(builder, inv.args));
    auto attrs = builder.CreateVector(CreateTokenVec(builder, inv.attrs));

    auto parentNames = builder.CreateVectorOfStrings(macCall.parentNames);

    std::vector<Offset<ChildMsg>> childMsgVec;
    std::vector<ChildMessage> childMessages;
    GetChildMessages(childMessages, macCall);
    for (auto& msg : childMessages) {
        auto childName = builder.CreateString(msg.childName);
        childMsgVec.push_back(
            CreateChildMsg(builder, childName, builder.CreateVector(CreatItemInfoVec(builder, msg.items))));
    }

    auto childMsges = builder.CreateVector(childMsgVec);

    auto methodName = builder.CreateString(macCall.methodName);
    auto packageName = builder.CreateString(macCall.packageName);
    auto libPath = builder.CreateString(macCall.libPath);

    auto beginPos = macCall.GetBeginPos();
    auto endPos = macCall.GetEndPos();
    auto bPos = MacroMsgFormat::Position(beginPos.fileID, beginPos.line, beginPos.column);
    auto ePos = MacroMsgFormat::Position(endPos.fileID, endPos.line, endPos.column);

    return CreateMacroCall(builder, id, inv.hasAttr, args, attrs, parentNames, childMsges,
                           methodName, packageName, libPath, &bPos, &ePos);
}

static auto CreateItemsVecFromRecordMacroInfo(
    FlatBufferBuilder& builder, const std::vector<InvokeRuntime::HANDLE>& recordMacroInfo)
{
    std::vector<Offset<MacroMsgFormat::ItemInfo>> itemsVec;
    auto itemNum = recordMacroInfo.size() / MACRO_SETITEM_INFO_NUM;
    for (size_t i = 0; i < itemNum; i++) {
        auto itemid = i * MACRO_SETITEM_INFO_NUM;
        // Item: key.
        Offset<String> key;
        auto pKey = recordMacroInfo[itemid]; // 0: key malloc from ffi, need free in child process.
        if (pKey) {
            key = builder.CreateString(std::string(static_cast<char*>(pKey)));
            free(pKey);
        } else {
            continue;
        }

        auto pValue = recordMacroInfo[itemid + 1];
        auto pType = recordMacroInfo[itemid + 2];
        auto type = static_cast<ItemKind>(*static_cast<uint8_t*>(pType));
        free(pType);

        if (type == ItemKind::STRING) {
            auto val = builder.CreateString(std::string(static_cast<char*>(pValue)));
            itemsVec.push_back(CreateItemInfo(builder, key, OptionValue_sValue, val.Union()));
        } else if (type == ItemKind::INT) {
            auto val = builder.CreateStruct(IntValue(*static_cast<int64_t*>(pValue)));
            itemsVec.push_back(CreateItemInfo(builder, key, OptionValue_iValue, val.Union()));
        } else if (type == ItemKind::BOOL) {
            auto val = builder.CreateStruct(BoolValue(*static_cast<bool*>(pValue)));
            itemsVec.push_back(CreateItemInfo(builder, key, OptionValue_bValue, val.Union()));
        } else if (type == ItemKind::TKS) {
            if (pValue == nullptr) {
                auto tksVec = builder.CreateVector(CreateTokenVec(builder, std::vector<Cangjie::Token>()));
                auto val = CreateTokensValue(builder, tksVec);
                itemsVec.push_back(CreateItemInfo(builder, key, OptionValue_tValue, val.Union()));
            } else {
                auto tokens = TokenSerialization::GetTokensFromBytes(static_cast<uint8_t*>(pValue) + sizeof(uint32_t));
                auto tksVec = builder.CreateVector(CreateTokenVec(builder, tokens));
                auto val = CreateTokensValue(builder, tksVec);
                itemsVec.push_back(CreateItemInfo(builder, key, OptionValue_tValue, val.Union()));
            }
        }

        if (pValue) {
            free(pValue);
        }
    }
    return builder.CreateVector(itemsVec);
}

static void DeserializePositionFromPositionBuf(Cangjie::Position& pos, const MacroMsgFormat::Position& posBuf)
{
    pos.fileID = posBuf.file_id();
    pos.line = posBuf.line();
    pos.column = posBuf.column();
}
static void DeserializeTokenFromTokenBuf(Cangjie::Token& tk, const MacroMsgFormat::Token& tkBuffer)
{
    tk.kind = static_cast<TokenKind>(tkBuffer.kind());
    tk.delimiterNum = tkBuffer.delimiterNum();
    Cangjie::Position begin;
    Cangjie::Position end;
    DeserializePositionFromPositionBuf(begin, *tkBuffer.begin());
    DeserializePositionFromPositionBuf(end, *tkBuffer.end());
    tk.SetValuePos(tkBuffer.value()->str(), begin, end);
}

static void DeserializeTokensFromOffsetTokenVec(
    std::vector<Cangjie::Token>& tks, const Vector<Offset<MacroMsgFormat::Token>>& offsetVec)
{
    tks.clear();
    uoffset_t num = offsetVec.size();
    Cangjie::Token tmp{TokenKind::ILLEGAL};
    for (uoffset_t i = 0; i < num; i++) {
        DeserializeTokenFromTokenBuf(tmp, *offsetVec.Get(i));
        tks.push_back(tmp);
    }
}

static void DeserializeDiagFromDiagBuf(Cangjie::Diagnostic& diag, const MacroMsgFormat::Diagnostic& diagBuffer)
{
    DeserializePositionFromPositionBuf(diag.start, *diagBuffer.begin());
    DeserializePositionFromPositionBuf(diag.end, *diagBuffer.end());
    diag.errorMessage = diagBuffer.errorMessage()->str();
    diag.mainHint.str = diagBuffer.mainHint()->str();
    diag.diagSeverity = static_cast<Cangjie::DiagSeverity>(diagBuffer.diagSeverity());
}

static void DeserializeDiagsFromOffsetDiagVec(
    std::vector<Cangjie::Diagnostic>& diags, const Vector<Offset<MacroMsgFormat::Diagnostic>>& offsetVec)
{
    diags.clear();
    uoffset_t num = offsetVec.size();
    Cangjie::Diagnostic tmp{};
    for (uoffset_t i = 0; i < num; i++) {
        DeserializeDiagFromDiagBuf(tmp, *offsetVec.Get(i));
        diags.push_back(tmp);
    }
}

static void DeserializeItemsFromItemsBuf(
    std::vector<Cangjie::ItemInfo>& items, const Vector<Offset<MacroMsgFormat::ItemInfo>>& itemsBuf)
{
    uoffset_t num = itemsBuf.size();
    items.resize(num);
    for (uoffset_t i = 0; i < num; i++) {
        auto itemBuf = itemsBuf.Get(i);
        if (itemBuf->key() == nullptr) {
            MacroMsgErrorln("DeserializeItemsFromItemsBuf key is null");
            continue; // skip item with empty key
        }
        items[i].key = itemBuf->key()->str();
        switch (itemBuf->value_type()) {
            case OptionValue_sValue:
                items[i].kind = ItemKind::STRING;
                items[i].sValue = itemBuf->value_as_sValue()->str();
                break;
            case OptionValue_iValue:
                items[i].kind = ItemKind::INT;
                items[i].iValue = itemBuf->value_as_iValue()->val();
                break;
            case OptionValue_bValue:
                items[i].kind = ItemKind::BOOL;
                items[i].bValue = itemBuf->value_as_bValue()->val();
                break;
            case OptionValue_tValue:
                items[i].kind = ItemKind::TKS;
                DeserializeTokensFromOffsetTokenVec(items[i].tValue, *(itemBuf->value_as_tValue()->val()));
                break;
            case OptionValue_NONE:
                [[fallthrough]]; // error
            default:
                MacroMsgErrorln("DeserializeItemsFromItemsBuf value type error, key: ", items[i].key);
                break;
        }
    }
}
} // namespace

void MacroEvalMsgSerializer::SerializeDeflibMsg(
    const std::unordered_set<std::string>& macrolibs, std::vector<uint8_t>& bufferData)
{
    builder.Clear();
    std::vector<Offset<String>> pathsVec;
    for (auto& p : macrolibs) {
        pathsVec.push_back(builder.CreateString(p));
    }
    auto paths = builder.CreateVector(pathsVec);
    auto content = CreateDefLib(builder, paths);
    auto macroMsg = CreateMacroMsg(builder, MsgContent_defLib, content.Union());
    builder.Finish(macroMsg);
    bufferData.assign(builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize());
}

void MacroEvalMsgSerializer::SerializeMacroCallMsg(const Cangjie::MacroCall& macCall, std::vector<uint8_t>& bufferData)
{
    builder.Clear();
    std::vector<Offset<MacroMsgFormat::MacroCall>> callsVec;
    if (macCall.GetInvocation() == nullptr) {
        MacroMsgErrorln("SerializeMacroCallMsg nullptr");
        return;
    }
    callsVec.push_back(CreateMacroCall(builder, macCall));
    auto calls = builder.CreateVector(callsVec);
    auto cont = CreateMultiMacroCalls(builder, calls);
    auto macroMsg = CreateMacroMsg(builder, MsgContent_multiCalls, cont.Union());
    builder.Finish(macroMsg);
    bufferData.assign(builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize());
}

void MacroEvalMsgSerializer::SerializeMultiCallsMsg(
    const std::list<Cangjie::MacroCall*>& macCalls, std::vector<uint8_t>& bufferData)
{
    builder.Clear();
    std::vector<Offset<MacroMsgFormat::MacroCall>> callsVec;
    for (auto callPtr : macCalls) {
        if (callPtr->GetInvocation() == nullptr) {
            MacroMsgErrorln("SerializeMultiCallsMsg nullptr");
            return;
        }
        callsVec.push_back(CreateMacroCall(builder, *callPtr));
    }
    auto calls = builder.CreateVector(callsVec);
    auto cont = CreateMultiMacroCalls(builder, calls);
    auto macroMsg = CreateMacroMsg(builder, MsgContent_multiCalls, cont.Union());
    builder.Finish(macroMsg);
    bufferData.assign(builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize());
}

bool MacroEvalMsgSerializer::SerializeMacroCallResultMsg(const MacroCall& macCall, std::vector<uint8_t>& bufferData)
{
    if (macCall.GetInvocation() == nullptr) {
        return false;
    }
    builder.Clear();
    auto& inv = *(macCall.GetInvocation());
    auto& idPos = inv.macroCallDiagInfo.identifierPos;
    auto pos = MacroMsgFormat::Position(idPos.fileID, idPos.line, idPos.column);
    auto id = CreateIdInfo(builder, builder.CreateString(inv.macroCallDiagInfo.identifier), &pos);

    auto tks = builder.CreateVector(CreateTokenVec(builder, inv.newTokens));

    Offset<Vector<Offset<MacroMsgFormat::ItemInfo>>> items;
    if (!macCall.items.empty()) {
        items = builder.CreateVector(CreatItemInfoVec(builder, macCall.items));
    } else if (macCall.recordMacroInfo.size() / MACRO_SETITEM_INFO_NUM > 0) {
        // serialize and free ffi malloc.
        items = CreateItemsVecFromRecordMacroInfo(builder, macCall.recordMacroInfo);
    } else {
        std::vector<Offset<MacroMsgFormat::ItemInfo>> itemsVec;
        items = builder.CreateVector(itemsVec);
    }

    auto assertParents = builder.CreateVectorOfStrings(macCall.assertParents);
    auto diags = macCall.ci->diag.GetCategoryDiagnostic(DiagCategory::PARSE);
    auto diagnostics = builder.CreateVector(CreateDiagVec(builder, diags));

    auto macroResultCont =
        CreateMacroResult(builder, id, static_cast<uint8_t>(macCall.status), tks, items, assertParents, diagnostics);
    auto macroMsg = CreateMacroMsg(builder, MsgContent_macroResult, macroResultCont.Union());
    builder.Finish(macroMsg);
    bufferData.assign(builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize());
    return true;
}

void MacroEvalMsgSerializer::SerializeExitMsg(std::vector<uint8_t>& bufferData, bool flag)
{
    builder.Clear();
    auto cont = builder.CreateStruct(MacroMsgFormat::ExitTask(flag));
    auto macroMsg = CreateMacroMsg(builder, MsgContent_exitTask, cont.Union());
    builder.Finish(macroMsg);
    bufferData.assign(builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize());
}

MacroMsgFormat::MsgContent MacroEvalMsgSerializer::GetMacroMsgContenType(const std::vector<uint8_t>& bufferData)
{
    auto msg = GetVerifiedMacroMsg(bufferData);
    if (msg == nullptr) {
        // Returning NONE lets the caller's switch fall through to the safe
        // exit path instead of dereferencing an unverified buffer. The switch
        // in MacroEvaluationSrv (GetMacroTaskType) routes NONE,
        // macroResult and the default branch all to EXIT_MACRO_SRV; if that
        // switch is ever changed, NONE must keep reaching a safe-exit branch.
        return MacroMsgFormat::MsgContent_NONE;
    }
    return msg->content_type();
}

void MacroEvalMsgSerializer::DeSerializeDeflibMsg(
    std::list<std::string>& macroLibs, const std::vector<uint8_t>& bufferData)
{
    macroLibs.clear();
    auto msg = GetVerifiedMacroMsg(bufferData);
    if (msg == nullptr) {
        return;
    }
    auto defLib = msg->content_as_defLib();
    if (defLib == nullptr || defLib->paths() == nullptr) {
        MacroMsgErrorln("DeSerializeDeflibMsg: defLib or paths is absent");
        return;
    }
    uoffset_t pathNum = defLib->paths()->size();
    for (uoffset_t i = 0; i < pathNum; i++) {
        macroLibs.push_back(defLib->paths()->Get(i)->str());
    }
}

void MacroEvalMsgSerializer::DeSerializeRangeFromCall(
    Position& begin, Position& end, const MacroMsgFormat::MacroCall& callFmt)
{
    DeserializePositionFromPositionBuf(begin, *callFmt.begin());
    DeserializePositionFromPositionBuf(end, *callFmt.end());
}

void MacroEvalMsgSerializer::DeSerializeIdInfoFromCall(
    std::string& id, Position& pos, const MacroMsgFormat::MacroCall& callFmt)
{
    id = callFmt.id()->name()->str();
    DeserializePositionFromPositionBuf(pos, *callFmt.id()->pos());
}


void MacroEvalMsgSerializer::DeSerializeArgsFromCall(std::vector<Token>& args, const MacroMsgFormat::MacroCall& callFmt)
{
    DeserializeTokensFromOffsetTokenVec(args, *callFmt.args());
}

void MacroEvalMsgSerializer::DeSerializeAttrsFromCall(
    std::vector<Token>& attrs, const MacroMsgFormat::MacroCall& callFmt)
{
    DeserializeTokensFromOffsetTokenVec(attrs, *callFmt.attrs());
}

void MacroEvalMsgSerializer::DeSerializeParentNamesFromCall(
    std::vector<std::string>& parentNames, const MacroMsgFormat::MacroCall& callFmt)
{
    uoffset_t num = callFmt.parentNames()->size();
    parentNames.resize(num);
    for (uoffset_t i = 0; i < num; i++) {
        parentNames[i] = callFmt.parentNames()->Get(i)->str();
    }
}

void MacroEvalMsgSerializer::DeSerializeChildMsgesFromCall(
    std::vector<ChildMessage>& childMsges, const MacroMsgFormat::MacroCall& callFmt)
{
    uoffset_t msgNum = callFmt.childMsges()->size();
    childMsges.resize(msgNum);
    for (uoffset_t i = 0; i < msgNum; i++) {
        childMsges[i].childName = callFmt.childMsges()->Get(i)->childName()->str();
        DeserializeItemsFromItemsBuf(childMsges[i].items, *callFmt.childMsges()->Get(i)->items());
    }
}

void MacroEvalMsgSerializer::DeSerializeIdInfoFromResult(
    std::string& id, Position& pos, const std::vector<uint8_t>& bufferData)
{
    auto result = GetVerifiedMacroResult(bufferData, "DeSerializeIdInfoFromResult");
    if (result == nullptr) {
        return;
    }
    // Each access below is one link in result -> id -> name/pos; check them in
    // turn so the log names the first missing link, not a lumped "id/name/pos".
    auto rId = result->id();
    if (rId == nullptr) {
        MacroMsgErrorln("DeSerializeIdInfoFromResult: id is absent");
        return;
    }
    if (rId->name() == nullptr) {
        MacroMsgErrorln("DeSerializeIdInfoFromResult: id.name is absent");
        return;
    }
    if (rId->pos() == nullptr) {
        MacroMsgErrorln("DeSerializeIdInfoFromResult: id.pos is absent");
        return;
    }
    id = rId->name()->str();
    DeserializePositionFromPositionBuf(pos, *rId->pos());
}

void MacroEvalMsgSerializer::DeSerializeTksFromResult(std::vector<Token>& tks, const std::vector<uint8_t>& bufferData)
{
    tks.clear();
    auto result = GetVerifiedMacroResult(bufferData, "DeSerializeTksFromResult");
    if (result == nullptr) {
        return;
    }
    if (result->tks() == nullptr) {
        MacroMsgErrorln("DeSerializeTksFromResult: tks is absent");
        return;
    }
    DeserializeTokensFromOffsetTokenVec(tks, *result->tks());
}

void MacroEvalMsgSerializer::DeSerializeDiagsFromResult(std::vector<Diagnostic>& diags,
                                                        const std::vector<uint8_t>& bufferData)
{
    diags.clear();
    auto result = GetVerifiedMacroResult(bufferData, "DeSerializeDiagsFromResult");
    if (result == nullptr) {
        return;
    }
    if (result->diags() == nullptr) {
        MacroMsgErrorln("DeSerializeDiagsFromResult: diags is absent");
        return;
    }
    DeserializeDiagsFromOffsetDiagVec(diags, *result->diags());
}

MacroEvalStatus MacroEvalMsgSerializer::DeSerializeStatusFromResult(const std::vector<uint8_t>& bufferData)
{
    auto result = GetVerifiedMacroResult(bufferData, "DeSerializeStatusFromResult");
    if (result == nullptr) {
        return MacroEvalStatus::REEVALFAILED;
    }
    return static_cast<MacroEvalStatus>(result->status());
}

void MacroEvalMsgSerializer::DeSerializeItemsFromResult(
    std::vector<ItemInfo>& items, const std::vector<uint8_t>& bufferData)
{
    items.clear();
    auto result = GetVerifiedMacroResult(bufferData, "DeSerializeItemsFromResult");
    if (result == nullptr) {
        return;
    }
    if (result->items() == nullptr) {
        MacroMsgErrorln("DeSerializeItemsFromResult: items is absent");
        return;
    }
    DeserializeItemsFromItemsBuf(items, *result->items());
}

void MacroEvalMsgSerializer::DeSerializeAssertParentsFromResult(
    std::vector<std::string>& assertParents, const std::vector<uint8_t>& bufferData)
{
    assertParents.clear();
    auto result = GetVerifiedMacroResult(bufferData, "DeSerializeAssertParentsFromResult");
    if (result == nullptr) {
        return;
    }
    if (result->assertParents() == nullptr) {
        MacroMsgErrorln("DeSerializeAssertParentsFromResult: assertParents is absent");
        return;
    }
    uoffset_t num = result->assertParents()->size();
    assertParents.resize(num);
    for (uoffset_t i = 0; i < num; i++) {
        assertParents[i] = result->assertParents()->Get(i)->str();
    }
}
