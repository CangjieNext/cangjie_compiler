// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/IR/AnnoInfo.h"
#include "cangjie/CHIR/Utils/ToStringUtils.h"

#include <iostream>
#include <sstream>

using namespace Cangjie::CHIR;

AnnoPair::AnnoPair(const std::string& className, const std::vector<std::string>& pValues, const DebugLocation& loc)
    : annoClassName(className), paramValues(pValues), loc(loc)
{
}

std::string AnnoPair::ToString(size_t indent) const
{
    std::stringstream ss;
    ss << IndentToString(indent) << "@" << annoClassName;
    ss << "[";
    ss << StringJoin(paramValues, ", ");
    ss << "]";
    if (auto locStr = loc.ToString(); !locStr.empty()) {
        ss << " // " << locStr;
    }
    return ss.str();
}

void AnnoPair::Dump() const
{
    std::cout << ToString(0) << std::endl;
}

bool AnnoInfo::IsAvailable() const
{
    return !mangledName.empty();
}

std::string AnnoInfo::ToString(size_t indent) const
{
    if (!IsAvailable()) {
        return "";
    }
    std::stringstream ss;
    ss << IndentToString(indent) << "@AnnoFactoryFunc: " << mangledName;
    for (const auto& anno : annoPairs) {
        anno.ToString(indent);
    }
    return ss.str();
}