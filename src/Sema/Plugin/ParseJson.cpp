// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements json parsing functions.
 */
#include "ParseJson.h"

#include <sstream>

#include "cangjie/Utils/StdUtils.h"

namespace Cangjie {
namespace PluginCheck {

inline size_t CountBackslashesBefore(size_t pos, const std::vector<uint8_t>& in)
{
    size_t count = 0;
    while (pos > 0 && in[pos - 1] == '\\') {
        ++count;
        --pos;
    }
    return count;
}

inline bool IsNonEscapedChar(size_t pos, const std::vector<uint8_t>& in, char target)
{
    if (pos >= in.size() || in[pos] != target) {
        return false;
    }
    return (CountBackslashesBefore(pos, in) % 2) == 0;
}

std::string ParseJsonString(size_t& pos, const std::vector<uint8_t>& in)
{
    if (pos >= in.size() || !IsNonEscapedChar(pos, in, '"')) {
        return "";
    }
    ++pos;
    std::stringstream str;
    while (pos < in.size() && in[pos] != '"') {
        str << in[pos];
        ++pos;
    }

    return str.str();
}

uint64_t ParseJsonNumber(size_t& pos, const std::vector<uint8_t>& in)
{
    if (pos >= in.size() || in[pos] < '0' || in[pos] > '9') {
        return 0;
    }
    std::stringstream num;
    while (pos < in.size() && in[pos] >= '0' && in[pos] <= '9') {
        num << in[pos];
        ++pos;
    }
    if (num.str().size()) {
        --pos;
    }
    return Stoull(num.str()).value_or(0);
}

void ParseJsonArray(size_t& pos, const std::vector<uint8_t>& in, Ptr<JsonPair> value)
{
    if (pos >= in.size() || !IsNonEscapedChar(pos, in, '"') || value == nullptr) {
        return;
    }
    ++pos;
    while (pos < in.size()) {
        if (in[pos] == ' ' || in[pos] == '\n') {
            ++pos;
            continue;
        }
        if (IsNonEscapedChar(pos, in, '"')) {
            value->valueStr.emplace_back(ParseJsonString(pos, in));
        }
        if (IsNonEscapedChar(pos, in, '{')) {
            value->valueObj.emplace_back(ParseJsonObject(pos, in));
        }
        if (IsNonEscapedChar(pos, in, ']')) {
            return;
        }
        ++pos;
    }
}

OwnedPtr<JsonObject> ParseJsonObject(size_t& pos, const std::vector<uint8_t>& in)
{
    if (pos >= in.size() || !IsNonEscapedChar(pos, in, '{')) {
        return MakeOwned<JsonObject>();
    }
    ++pos;
    auto ret = MakeOwned<JsonObject>();
    auto mod = StringMod::KEY;
    while (pos < in.size()) {
        if (in[pos] == ' ' || in[pos] == '\n') {
            ++pos;
            continue;
        }
        if (IsNonEscapedChar(pos, in, '}')) {
            return ret;
        }
        if (IsNonEscapedChar(pos, in, ':')) {
            mod = StringMod::VALUE;
            if (ret->pairs.empty()) {
                ret->pairs.emplace_back(MakeOwned<JsonPair>());
            }
        }
        if (IsNonEscapedChar(pos, in, ',')) {
            mod = StringMod::KEY;
        }
        if (IsNonEscapedChar(pos, in, '"')) {
            if (mod == StringMod::KEY) {
                auto newData = MakeOwned<JsonPair>();
                newData->key = ParseJsonString(pos, in);
                ret->pairs.emplace_back(std::move(newData));
            } else {
                ret->pairs.back()->valueStr.emplace_back(ParseJsonString(pos, in));
            }
        }
        if (in[pos] >= '0' && in[pos] <= '9' && mod == StringMod::VALUE) {
            if (ret->pairs.empty()) {
                ret->pairs.emplace_back(MakeOwned<JsonPair>());
            }
            ret->pairs.back()->valueNum.emplace_back(ParseJsonNumber(pos, in));
        }
        if (IsNonEscapedChar(pos, in, '{') && mod == StringMod::VALUE) {
            if (ret->pairs.empty()) {
                ret->pairs.emplace_back(MakeOwned<JsonPair>());
            }
            // The pos will be updated to the pos of matched '}'.
            ret->pairs.back()->valueObj.emplace_back(ParseJsonObject(pos, in));
        }
        if (IsNonEscapedChar(pos, in, '[') && mod == StringMod::VALUE) {
            if (ret->pairs.empty()) {
                ret->pairs.emplace_back(MakeOwned<JsonPair>());
            }
            // The pos will be updated to the pos of matched ']'.
            ParseJsonArray(pos, in, ret->pairs.back().get());
        }
        ++pos;
    }
    return ret;
}

std::vector<std::string> GetJsonString(Ptr<JsonObject> root, const std::string& key)
{
    if (root == nullptr) {
        return {};
    }
    for (auto& v : root->pairs) {
        if (v->key == key) {
            return v->valueStr;
        }
        for (auto& o : v->valueObj) {
            auto ret = GetJsonString(o.get(), key);
            if (!ret.empty()) {
                return ret;
            }
        }
    }
    return {};
}

Ptr<JsonObject> GetJsonObject(Ptr<JsonObject> root, const std::string& key, const size_t index)
{
    if (root == nullptr) {
        return nullptr;
    }
    for (auto& v : root->pairs) {
        if (v->key == key && v->valueObj.size() > index) {
            return v->valueObj[index].get();
        }
        for (auto& o : v->valueObj) {
            auto ret = GetJsonObject(o.get(), key, index);
            if (ret) {
                return ret;
            }
        }
    }
    return nullptr;
}
} // namespace PluginCheck
} // namespace Cangjie
