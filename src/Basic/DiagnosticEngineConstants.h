// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_BASIC_DIAGNOSTIC_ENGINE_CONSTANTS_H
#define CANGJIE_BASIC_DIAGNOSTIC_ENGINE_CONSTANTS_H

#include <string>
#include <string_view>
#include <vector>

namespace Cangjie {
const std::string MACROCALL_CODE = "the code after the macro is expanded as follows";

/** Entry count for legacy DK fragment tables (\c DIAG_KIND_STR, \c DiagSeveritys, \c DiagMessages, \c warnGroups). */
extern const size_t DIAG_KIND_STR_SIZE;
/** Entry count for refactor DKRef fragment tables (\c errorData, \c rDiagSeveritys, \c rWarnGroups, \c
 * RE_DIAG_KIND_STR). */
extern const size_t RE_DIAG_KIND_STR_SIZE;
/** Entry count for \c WarnGroupKind.def (\c warnGroupDescrs). */
extern const size_t WARN_GROUP_DESCRS_SIZE;

extern const std::vector<std::string_view> DIAG_KIND_STR;
extern const std::vector<std::string_view> warnGroupDescrs;
extern const std::vector<std::string_view> RE_DIAG_KIND_STR;
} // namespace Cangjie

#endif // CANGJIE_BASIC_DIAGNOSTIC_ENGINE_CONSTANTS_H
