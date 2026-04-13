// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
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
