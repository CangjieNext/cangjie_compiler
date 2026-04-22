// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// Single TU: legacy / refactor diagnostic ordinals (enums + name->ordinal maps). Dot-def fragments live under
// src/Basic/diagnostics/{DK,DKRef}/.

#include <cassert>
#include <string>
#include <string_view>
#include <unordered_map>

#include "cangjie/Basic/DiagId.h"
#include "cangjie/Basic/DiagIdOld.h"
#include "cangjie/Utils/CheckUtils.h"

namespace {

enum class LegacyOrdinal : int {
#define NOTE(K, Info) K,
#define ERROR(K, Info) K,
#define WARNING(K, Group, Info) K,
#include "Diagnostics/DK/DiagnosticAll.def"
};

#undef NOTE
#undef ERROR
#undef WARNING

enum class RefactorOrdinal : int {
#define ERROR(K, ...) K,
#define WARNING(K, ...) K,
#include "Diagnostics/DKRef/DiagnosticAll.def"
};

#undef ERROR
#undef WARNING

std::unordered_map<std::string_view, int> gLegacyKindByName;
std::unordered_map<std::string_view, int> gRefactorKindByName;

void BuildDiagKindMaps()
{
    if (gRefactorKindByName.size() > 1) {
        return;
    }
#define NOTE(K, Info) gLegacyKindByName.emplace(#K, static_cast<int>(LegacyOrdinal::K));
#define ERROR(K, Info) gLegacyKindByName.emplace(#K, static_cast<int>(LegacyOrdinal::K));
#define WARNING(K, Group, Info) gLegacyKindByName.emplace(#K, static_cast<int>(LegacyOrdinal::K));
#include "Diagnostics/DK/DiagnosticAll.def"
#undef NOTE
#undef ERROR
#undef WARNING
#define ERROR(K, ...) gRefactorKindByName.emplace(#K, static_cast<int>(RefactorOrdinal::K));
#define WARNING(K, ...) gRefactorKindByName.emplace(#K, static_cast<int>(RefactorOrdinal::K));
#include "Diagnostics/DKRef/DiagnosticAll.def"
#undef ERROR
#undef WARNING
}
} // namespace

namespace Cangjie {

DiagIdOld LookupDiagKind(std::string_view name)
{
    BuildDiagKindMaps();
    auto it = gLegacyKindByName.find(name);
    if (it == gLegacyKindByName.end()) {
        std::string msg = std::string("unknown legacy diagnostic kind: ") + std::string(name);
        CJC_ABORT_WITH_MSG(msg);
    }
    return DiagIdOld(it->second);
}

DiagId LookupDiagKindRefactor(std::string_view name)
{
    BuildDiagKindMaps();
    auto it = gRefactorKindByName.find(name);
    if (it == gRefactorKindByName.end()) {
        std::string msg = std::string("unknown refactor diagnostic kind: ") + std::string(name);
        CJC_ABORT_WITH_MSG(msg);
    }
    return DiagId(it->second);
}

extern const DiagIdOld DEFAULT_KIND = LookupDiagKind("sema_diag_begin");
} // namespace Cangjie
