// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements compiler's version apis.
 */

#include "cangjie/Basic/Print.h"
#include "cangjie/Basic/Version.h"

#include <exception>
#include <regex>

namespace Cangjie {
namespace {
VersionInfo ParseVersion(const std::string& version)
{
    VersionInfo info;
    static const std::regex pattern("^([0-9]+)[.]([0-9]+)(?:[.]([0-9]+))?");
    std::smatch match;
    if (!std::regex_search(version, match, pattern)) {
        return info;
    }
    try {
        info.major = static_cast<uint32_t>(std::stoul(match[1].str()));
        info.minor = static_cast<uint32_t>(std::stoul(match[2].str()));
        info.patch = match[3].matched ? static_cast<uint32_t>(std::stoul(match[3].str())) : 0;
    } catch (const std::exception&) {
        return info;
    }
    info.isValid = true;
    return info;
}
} // namespace

VersionInfo VersionInfo::Current()
{
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
    return ParseVersion(CANGJIE_VERSION);
#else
    return {};
#endif
}

#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
const std::string CANGJIE_VERSION = CJ_SDK_VERSION;

#ifndef VERSION_TAIL
const std::string CANGJIE_COMPILER_VERSION = "Cangjie Compiler: " + CANGJIE_VERSION;
#else
const std::string CANGJIE_COMPILER_VERSION = "Cangjie Compiler: " + CANGJIE_VERSION + VERSION_TAIL;
#endif
#endif
void PrintVersion()
{
    Cangjie::Println(CANGJIE_COMPILER_VERSION);
}
} // namespace Cangjie
