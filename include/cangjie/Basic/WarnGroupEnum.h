// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
#ifndef CANGJIE_BASIC_WARNGROUPENUM_H
#define CANGJIE_BASIC_WARNGROUPENUM_H

#include <cstdint>

namespace Cangjie {
enum class WarnGroup : uint8_t {
    UNUSED,
    UNUSED_MAIN,
    DRIVER_ARG,
    DEPRECATED,
    UNSUPPORT_COMPILE_SOURCE,
    PACKAGE_IMPORT,
    PARSER,
    SEMANTICS,
    INTERPRETER,
    APILEVEL_CHECK,
    UNGROUPED,
    NONE,
};
} // namespace Cangjie

#endif // CANGJIE_BASIC_WARNGROUPENUM_H
