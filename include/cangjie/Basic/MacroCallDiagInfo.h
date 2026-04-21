// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_BASIC_MACROCALLDIAGINFO_H
#define CANGJIE_BASIC_MACROCALLDIAGINFO_H

#include <map>
#include <memory>
#include <string>

#include "cangjie/Basic/Position.h"

namespace Cangjie {

class Diagnostic;
class DiagnosticEngine;

struct MacroCallDiagInfo {
    Position macroCallBegin;
    Position macroCallEnd;

    Position identifierPos;
    std::string identifier;

    std::string fullName;

    Position mcBegin;
    Position mcEnd;

    bool isForLSP{false};
    bool isCurFile{false};
    bool isCustom{false};
    mutable bool hasShownCode{false};
    std::map<unsigned int, Position, std::greater<unsigned int>> new2macroCallPosMap;
    std::map<unsigned int, Position, std::greater<unsigned int>> new2originPosMap;
    std::map<unsigned int, Position, std::greater<unsigned int>> originPosMap;

    Position MapPos(Position pos, bool isLowerBound = false) const;

    Position GetDiagBegin() const;

    Position GetDiagEnd() const;

    bool IsPureAnnotation() const
    {
        return isCustom && isCurFile;
    }
};

void ApplyMacroCallDiagInfo(const MacroCallDiagInfo& info, Diagnostic& diagnostic, Position diagPos);

} // namespace Cangjie

#endif // CANGJIE_BASIC_MACROCALLDIAGINFO_H
