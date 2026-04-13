// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares the warning group enum class.
 */

#ifndef CANGJIE_BASIC_WARNGROUP_H
#define CANGJIE_BASIC_WARNGROUP_H

#include <cstdint>

namespace Cangjie {
enum class WarnGroup : uint8_t {
#define WARN_GROUP(DESCR, KIND) KIND,
#include "cangjie/Basic/DKRef/WarnGroupKind.def"
#undef WARN_GROUP
    UNGROUPED,
    NONE,
};
} // namespace Cangjie

#endif // CANGJIE_BASIC_WARNGROUP_H
