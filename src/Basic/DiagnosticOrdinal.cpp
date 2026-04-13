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
#include "diagnostics/DK/Chir.def"
#include "diagnostics/DK/InterpConstEval.def"
#include "diagnostics/DK/Macro.def"
#include "diagnostics/DK/Sema.def"
};

#undef NOTE
#undef ERROR
#undef WARNING

enum class RefactorOrdinal : int {
#define ERROR(K, ...) K,
#define WARNING(K, ...) K,
#include "diagnostics/DKRef/Chir.def"
#include "diagnostics/DKRef/ConditionalCompilation.def"
#include "diagnostics/DKRef/Driver.def"
#include "diagnostics/DKRef/Frontend.def"
#include "diagnostics/DKRef/IncrementalCompilation.def"
#include "diagnostics/DKRef/Lexer.def"
#include "diagnostics/DKRef/Module.def"
#include "diagnostics/DKRef/Package.def"
#include "diagnostics/DKRef/Parser.def"
#include "diagnostics/DKRef/ParserQuery.def"
#include "diagnostics/DKRef/Sema.def"
#include "diagnostics/DKRef/Cjmp.def"
#include "diagnostics/DKRef/EffectHandlers.def"
#include "diagnostics/DKRef/Deprecation.def"
#include "diagnostics/DKRef/Test.def"
#include "diagnostics/DKRef/APILevel.def"
};

#undef ERROR
#undef WARNING

std::unordered_map<std::string, int> gLegacyKindByName;
std::unordered_map<std::string, int> gRefactorKindByName;

void BuildLegacyMap()
{
#define NOTE(K, Info) gLegacyKindByName.emplace(#K, static_cast<int>(LegacyOrdinal::K));
#define ERROR(K, Info) gLegacyKindByName.emplace(#K, static_cast<int>(LegacyOrdinal::K));
#define WARNING(K, Group, Info) gLegacyKindByName.emplace(#K, static_cast<int>(LegacyOrdinal::K));
#include "diagnostics/DK/Chir.def"
#include "diagnostics/DK/InterpConstEval.def"
#include "diagnostics/DK/Macro.def"
#include "diagnostics/DK/Sema.def"
#undef NOTE
#undef ERROR
#undef WARNING
}

void BuildRefactorMap()
{
#define ERROR(K, ...) gRefactorKindByName.emplace(#K, static_cast<int>(RefactorOrdinal::K));
#define WARNING(K, ...) gRefactorKindByName.emplace(#K, static_cast<int>(RefactorOrdinal::K));
#include "diagnostics/DKRef/Chir.def"
#include "diagnostics/DKRef/ConditionalCompilation.def"
#include "diagnostics/DKRef/Driver.def"
#include "diagnostics/DKRef/Frontend.def"
#include "diagnostics/DKRef/IncrementalCompilation.def"
#include "diagnostics/DKRef/Lexer.def"
#include "diagnostics/DKRef/Module.def"
#include "diagnostics/DKRef/Package.def"
#include "diagnostics/DKRef/Parser.def"
#include "diagnostics/DKRef/ParserQuery.def"
#include "diagnostics/DKRef/Sema.def"
#include "diagnostics/DKRef/Cjmp.def"
#include "diagnostics/DKRef/EffectHandlers.def"
#include "diagnostics/DKRef/Deprecation.def"
#include "diagnostics/DKRef/Test.def"
#include "diagnostics/DKRef/APILevel.def"
#undef ERROR
#undef WARNING
}

void EnsureMapsBuilt()
{
    static bool done = false;
    if (done) {
        return;
    }
    BuildLegacyMap();
    BuildRefactorMap();
    done = true;
}

} // namespace

namespace Cangjie {

DiagIdOld LookupDiagKind(std::string_view name)
{
    EnsureMapsBuilt();
    auto it = gLegacyKindByName.find(std::string(name));
    if (it == gLegacyKindByName.end()) {
        std::string msg = std::string("unknown legacy diagnostic kind: ") + std::string(name);
        CJC_ABORT_WITH_MSG(msg);
    }
    return DiagIdOld(it->second);
}

DiagId LookupDiagKindRefactor(std::string_view name)
{
    EnsureMapsBuilt();
    auto it = gRefactorKindByName.find(std::string(name));
    if (it == gRefactorKindByName.end()) {
        std::string msg = std::string("unknown refactor diagnostic kind: ") + std::string(name);
        CJC_ABORT_WITH_MSG(msg);
    }
    return DiagId(it->second);
}

extern const DiagIdOld DEFAULT_KIND = LookupDiagKind("sema_diag_begin");

class DiagnosticOrdinalRegistry {
public:
    DiagnosticOrdinalRegistry();
};

DiagnosticOrdinalRegistry::DiagnosticOrdinalRegistry()
{
#if !defined(NDEBUG)
    assert(LookupDiagKind("chir_diag_begin").Val() == 0);
    assert(LookupDiagKindRefactor("chir_diag_begin").Val() == 0);
#endif
}

DiagnosticOrdinalRegistry g_diagnosticOrdinalRegistry;

} // namespace Cangjie
