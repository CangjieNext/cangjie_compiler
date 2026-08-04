// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares java ...
 */

#include "NativeFFI/Java/JavaFQName.h"
#include "cangjie/Utils/CheckUtils.h"
#include <ostream>
#include <string_view>

namespace Cangjie::Native::FFI::Java {

JavaFQName::JavaFQName(const std::string_view package, const std::string_view type) noexcept
: package(package), type(type)
{
    CJC_ASSERT(!package.empty());
    CJC_ASSERT(!type.empty());
}

std::ostream& operator<<(std::ostream& out, const JavaFQName& fqname)
{
    out << fqname.ToString();
    return out;
}

std::string JavaFQName::ToString() const
{
    std::string str{package};
    str += ".";

    for (auto outerType : outerTypes) {
        str += outerType;
        str += ".";
    }

    str += type;
    return str;
}


} // namespace Cangjie::Cangjie::Native::FFI::Java
