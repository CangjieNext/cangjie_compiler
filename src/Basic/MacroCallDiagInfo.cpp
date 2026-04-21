// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/Basic/MacroCallDiagInfo.h"

#include "cangjie/Basic/DiagnosticEngine.h"

namespace Cangjie {

namespace {

Position GetMacroCallEndPos(bool isCurFile, uint32_t key, uint32_t first, const Position second)
{
    auto end = second;
    if (!isCurFile) {
        auto columnOffset = (key > first) ? (key - first) : 0;
        return end + columnOffset;
    }
    auto keyPos = Position::RestorePosFromHash(key);
    auto firstPos = Position::RestorePosFromHash(first);
    auto lineOffset = (keyPos.first > firstPos.first) ? (keyPos.first - firstPos.first) : 0;
    if (lineOffset > 0) {
        end.line += lineOffset;
        end.column = keyPos.second;
        return end;
    }
    auto columnOffset = (key > first) ? (key - first) : 0;
    return end + columnOffset;
}

Position GetMacroCallEndPosforLsp(uint32_t newPos, uint32_t first, const Position second)
{
    auto end = second;
    auto keyPos = Position::RestorePosFromHash(newPos);
    auto firstPos = Position::RestorePosFromHash(first);
    auto lineOffset = (keyPos.first > firstPos.first) ? (keyPos.first - firstPos.first) : 0;
    if (lineOffset > 0) {
        end.line += lineOffset;
        end.column = keyPos.second;
        return end;
    }
    auto columnOffset = (newPos > first) ? (newPos - first) : 0;
    return end + columnOffset;
}

} // namespace

Position MacroCallDiagInfo::MapPos(Position pos, bool isLowerBound) const
{
    if (IsPureAnnotation()) {
        return pos;
    }
    auto key = isCurFile ? pos.Hash32() : static_cast<uint32_t>(pos.column);
    if (isLowerBound) {
        if (isForLSP) {
            auto posIt = new2originPosMap.find(pos.Hash32());
            if (posIt != new2originPosMap.end()) {
                ++posIt;
            }
            if (posIt == new2originPosMap.end()) {
                posIt = new2originPosMap.lower_bound(pos.Hash32());
            }
            if (posIt == new2originPosMap.end()) {
                return pos;
            }
            auto sourcePos = GetMacroCallEndPosforLsp(pos.Hash32(), posIt->first, posIt->second);
            if (sourcePos.isCurFile) {
                return sourcePos;
            }
        }
        auto posIt = new2macroCallPosMap.lower_bound(key);
        if (posIt != new2macroCallPosMap.end()) {
            return GetMacroCallEndPos(isCurFile, key, posIt->first, posIt->second);
        }
        return pos;
    }
    if (isForLSP) {
        if (new2originPosMap.find(pos.Hash32()) == new2originPosMap.end()) {
            return pos;
        }
        auto sourcePos = new2originPosMap.at(pos.Hash32());
        if (sourcePos.isCurFile) {
            return sourcePos;
        }
    }
    if (new2macroCallPosMap.find(key) != new2macroCallPosMap.end()) {
        return new2macroCallPosMap.at(key);
    }
    return pos;
}

void ApplyMacroCallDiagInfo(const MacroCallDiagInfo& info, Diagnostic& diagnostic, Position diagPos)
{
    if (!info.isForLSP) {
        Position originPos;
        auto key = info.isCurFile ? diagPos.Hash32() : static_cast<uint32_t>(diagPos.column);
        if (info.isCurFile && info.originPosMap.find(key) != info.originPosMap.end()) {
            originPos = info.originPosMap.at(key);
        } else if (info.new2originPosMap.find(diagPos.Hash32()) != info.new2originPosMap.end()) {
            originPos = info.new2originPosMap.at(diagPos.Hash32());
            if (!originPos.isCurFile) {
                originPos = INVALID_POSITION;
            }
        }
        if (originPos != INVALID_POSITION && originPos != diagnostic.start &&
            originPos != diagnostic.mainHint.range.begin) {
            if (diagnostic.errorMessage.empty()) {
                auto range = MakeRange(diagnostic.start, diagnostic.end);
                (void)diagnostic.subDiags.emplace_back(range, "which is expanded as follows");
                diagnostic.start = originPos;
                diagnostic.end = originPos + 1;
            } else {
                (void)diagnostic.subDiags.emplace_back(diagnostic.mainHint.range, "which is expanded as follows");
            }
            diagnostic.mainHint.range = MakeRange(originPos, originPos + 1);
        }
    }
    auto mcBegin = info.macroCallBegin;
    auto idPosEnd = info.identifierPos + info.identifier.size();
    auto mcEnd = info.isForLSP ? idPosEnd : info.macroCallEnd;
    std::string sevInfo = (diagnostic.diagSeverity == DiagSeverity::DS_ERROR) ? "the error" : "the warning";
    (void)diagnostic.subDiags.emplace_back(MakeRange(mcBegin, mcEnd), sevInfo + " occurs after the macro is expanded");
    if (!info.isForLSP) {
        auto codeRange = MakeRange(info.mcBegin, info.mcEnd);
        (void)diagnostic.subDiags.emplace_back(codeRange, MACROCALL_CODE);
    }
}

} // namespace Cangjie
