// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements the APILevelVersion type methods.
 */

#include "APILevelVersion.h"

#include <optional>
#include <vector>

#include "cangjie/Utils/StdUtils.h"

namespace Cangjie {

std::string APILevelVersion::ToDisplayString() const
{
    if (Patch != 0) {
        return ToString();
    }
    if (Minor != 0) {
        return std::to_string(Major) + "." + std::to_string(Minor);
    }
    return std::to_string(Major);
}

APILevelVersion APILevelVersion::Parse(const std::string& s)
{
    constexpr size_t versionIdxMinor = 1;
    constexpr size_t versionIdxPatch = 2;
    constexpr size_t versionMinPartsMinor = 2;
    constexpr size_t versionMinPartsPatch = 3;

    APILevelVersion version;
    if (s.empty()) {
        return version;
    }

    std::vector<std::string> parts;
    size_t start = 0;
    size_t end = s.find('.');
    while (end != std::string::npos) {
        parts.push_back(s.substr(start, end - start));
        start = end + 1;
        end = s.find('.', start);
    }
    parts.push_back(s.substr(start));

    if (!parts[0].empty()) {
        version.Major = static_cast<uint32_t>(Stoull(parts[0]).value_or(0));
    }
    if (parts.size() >= versionMinPartsMinor && !parts[versionIdxMinor].empty()) {
        version.Minor = static_cast<uint32_t>(Stoull(parts[versionIdxMinor]).value_or(0));
    }
    if (parts.size() >= versionMinPartsPatch && !parts[versionIdxPatch].empty()) {
        version.Patch = static_cast<uint32_t>(Stoull(parts[versionIdxPatch]).value_or(0));
    }

    return version;
}

bool APILevelVersion::IsValidFormat(const std::string& s)
{
    constexpr size_t MAX_VERSION_PARTS = 3;
    if (s.empty()) {
        return false;
    }
    size_t partCount = 0;
    size_t pos = 0;
    const size_t len = s.size();
    while (pos < len) {
        size_t dotPos = s.find('.', pos);
        size_t partEnd = (dotPos == std::string::npos) ? len : dotPos;
        if (partEnd == pos) {
            return false; // empty component (leading or consecutive dot)
        }
        for (size_t i = pos; i < partEnd; ++i) {
            if (s[i] < '0' || s[i] > '9') {
                return false;
            }
        }
        ++partCount;
        if (partCount > MAX_VERSION_PARTS) {
            return false;
        }
        if (dotPos == std::string::npos) {
            pos = len; // last part processed, exit loop
        } else {
            pos = dotPos + 1;
            if (pos == len) {
                return false; // trailing dot (e.g., "1.2.")
            }
        }
    }
    return partCount > 0;
}

} // namespace Cangjie
