// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/Utils/StdUtils.h"

#include <climits>
namespace Cangjie {
std::optional<unsigned int> Stoui(const std::string& s, int base)
{
    try {
        // std::stoul silently wraps a negative literal around, so a leading '-' has to be
        // rejected here. Relying on the `> UINT_MAX` check below is not enough: it only catches
        // the wrapped value where `unsigned long` is wider than `unsigned int`, which makes the
        // result differ between LP64 and LLP64. Only the sign position is examined, so trailing
        // junk keeps being ignored the same way std::stoul ignores it.
        if (size_t sign = s.find_first_not_of(" \t\n\v\f\r"); sign != std::string::npos && s[sign] == '-') {
            return {};
        }
        unsigned long result = std::stoul(s, nullptr, base);
        if (result > UINT_MAX) {
            return {};
        }
        return static_cast<unsigned int>(result);
    } catch (...) {
        return {};
    }
}
std::optional<int> Stoi(const std::string& s, int base)
{
    try {
        return std::stoi(s, nullptr, base);
    } catch (...) {
        return {};
    }
}
std::optional<long> Stol(const std::string& s, int base)
{
    try {
        return std::stol(s, nullptr, base);
    } catch (...) {
        return {};
    }
}
std::optional<unsigned long> Stoul(const std::string& s, int base)
{
    try {
        return std::stoul(s, nullptr, base);
    } catch (...) {
        return {};
    }
}
std::optional<long long> Stoll(const std::string& s, int base)
{
    try {
        return std::stoll(s, nullptr, base);
    } catch (...) {
        return {};
    }
}
std::optional<unsigned long long> Stoull(const std::string& s, int base)
{
    try {
        return std::stoull(s, nullptr, base);
    } catch (...) {
        return {};
    }
}

std::optional<double> Stod(const std::string& s)
{
    try {
        return std::stod(s, nullptr);
    } catch (...) {
        return {};
    }
}
std::optional<long double> Stold(const std::string& s)
{
    try {
        return std::stold(s, nullptr);
    } catch (...) {
        return {};
    }
}
}
