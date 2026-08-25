// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares implementation of java class type abstraction.
 */

#include "NativeFFI/Java/JavaClassSignature.h"
#include "NativeFFI/Java/Utils.h"
#include "cangjie/Basic/Utils.h"
#include "cangjie/Utils/CheckUtils.h"
#include <ostream>
#include <string_view>

namespace Cangjie::Native::FFI::Java {

namespace {
template <typename T> std::vector<T> WithoutTail(std::vector<T> vec)
{
    if (vec.size() < 2) {
        return {};
    }

    std::vector<T> res(vec.begin(), vec.end() - 1);
    return res;
}
} // namespace

JavaClassSignature JavaClassSignature::FromDecl(const AST::ClassLikeDecl& decl)
{
    return JavaClassSignature(GetJavaPackage(decl), GetJavaUnqualifiedTypeName(decl, "."));
}

JavaClassSignature::JavaClassSignature(std::string_view package, std::string_view type)
    : packageParts(Utils::SplitString(std::string(package), ".")),
      outerTypes(WithoutTail(Utils::SplitString(std::string(type), "."))),
      type(Utils::SplitString(std::string(type), ".").back())
{
    // CJC_ASSERT(!package.empty());
    CJC_ASSERT(!type.empty());
}

JavaClassSignature JavaClassSignature::GetConstructorMarkerClass()
{
    constexpr static auto NATIVE_CONSTRUCTOR_MARKER_CLASS_NAME = "$$NativeConstructorMarker";
    constexpr static auto NATIVE_CONSTRUCTOR_MARKER_PACKAGE_NAME = "cangjie.lang.internal";
    static auto marker = JavaClassSignature(
        NATIVE_CONSTRUCTOR_MARKER_PACKAGE_NAME,
        NATIVE_CONSTRUCTOR_MARKER_CLASS_NAME);
    return marker;
}

bool JavaClassSignature::IsNested() const
{
    return !outerTypes.empty();
}

std::ostream& operator<<(std::ostream& out, const JavaClassSignature& fqname)
{
    out << fqname.ToString();
    return out;
}

std::string JavaClassSignature::ToString() const
{
    return GetFQName();
}

std::string JavaClassSignature::GetJniClassName() const
{
    const static std::string& packageDelimiter = "/";
    const static std::string& nestedTypeDelimiter = "$";

    std::string str{Utils::JoinStrings(packageParts, packageDelimiter)};
    str += packageDelimiter;

    if (IsNested()) {
        str += Utils::JoinStrings(outerTypes, nestedTypeDelimiter);
        str += nestedTypeDelimiter;
    }

    str += type;
    return str;
}

std::string JavaClassSignature::GetFQName() const
{
    const static std::string& packageDelimiter = ".";
    const static std::string& nestedTypeDelimiter = ".";
    std::string str{Utils::JoinStrings(packageParts, packageDelimiter)};
    str += packageDelimiter;

    if (IsNested()) {
        str += Utils::JoinStrings(outerTypes, nestedTypeDelimiter);
        str += nestedTypeDelimiter;
    }

    str += type;
    return str;
}

std::string JavaClassSignature::GetUnqualifiedTypeName(const std::string_view nestedDelimiter) const
{
    if (!IsNested()) {
        return std::string(type);
    }

    return Utils::JoinStrings(outerTypes, std::string(nestedDelimiter)) + std::string(nestedDelimiter) +
        std::string(type);
}

bool JavaClassSignature::operator==(const JavaClassSignature& other) const
{
    return ToString() == other.ToString();
}

bool JavaClassSignature::operator!=(const JavaClassSignature& other) const
{
    return !(*this == other);
}

} // namespace Cangjie::Cangjie::Native::FFI::Java
