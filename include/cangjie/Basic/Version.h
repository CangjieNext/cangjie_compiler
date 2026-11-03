// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares the cangjie compiler's version.
 */

#ifndef CANGJIE_DRIVER_VERSION_H
#define CANGJIE_DRIVER_VERSION_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace Cangjie {
struct VersionInfo {
    uint32_t major = 0;
    uint32_t minor = 0;
    uint32_t patch = 0;
    bool isValid = false;

    constexpr bool operator==(const VersionInfo& other) const
    {
        return major == other.major && minor == other.minor && patch == other.patch;
    }

    constexpr bool operator<(const VersionInfo& other) const
    {
        if (major != other.major) {
            return major < other.major;
        }
        if (minor != other.minor) {
            return minor < other.minor;
        }
        return patch < other.patch;
    }

    static VersionInfo Current();
};

enum class CompatChange : uint16_t {
#define COMPAT_CHANGE(ID, ...) ID,
#include "cangjie/Basic/CompatChanges.def"
#undef COMPAT_CHANGE
    COUNT
};

struct CompatChangeInfo {
    VersionInfo since;
    bool exempt;
    const char* description;
};

inline constexpr std::array<CompatChangeInfo, static_cast<size_t>(CompatChange::COUNT)> COMPAT_CHANGE_INFOS = {{
#define COMPAT_CHANGE(ID, MAJOR, MINOR, EXEMPT, DESCRIPTION) {{MAJOR, MINOR, 0, true}, EXEMPT, DESCRIPTION},
#include "cangjie/Basic/CompatChanges.def"
#undef COMPAT_CHANGE
}};

#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
extern const std::string CANGJIE_VERSION;
extern const std::string CANGJIE_COMPILER_VERSION;
#endif
void PrintVersion();
} // namespace Cangjie
#endif // CANGJIE_DRIVER_VERSION_H
