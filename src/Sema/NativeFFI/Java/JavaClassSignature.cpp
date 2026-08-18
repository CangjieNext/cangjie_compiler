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
#include "cangjie/Utils/CheckUtils.h"
#include <ostream>
#include <string_view>

namespace Cangjie::Native::FFI::Java {

namespace {
std::vector<std::string_view> Split(std::string_view str, std::string_view separator)
{
    std::vector<std::string_view> parts;
    size_t prevPos = 0;
    size_t startPos = 0;
    while ((startPos = str.find(separator, startPos)) != std::string::npos) {
        parts.push_back(str.substr(prevPos, startPos));
        prevPos = startPos;
    }
    if (startPos != std::string::npos) {
        parts.push_back(str.substr(startPos));
    } else {
        parts.push_back(str);
    }
    return parts;
}

template <typename T> std::vector<T> WithoutTail(std::vector<T> vec)
{
    if (vec.size() < 2) {
        return {};
    }

    std::vector<T> res(vec.begin(), vec.end() - 1);
    return res;
}

std::string Join(const std::vector<std::string_view>& vec, const std::string_view delimiter = "")
{
    std::string result;
    for (size_t i = 0; i < vec.size(); ++i) {
        result += vec[i];
        if (i != vec.size() - 1) {
            result += delimiter;
        }
    }
    return result;
}

} // namespace

JavaClassSignature JavaClassSignature::FromDecl(const AST::ClassLikeDecl& decl)
{
    return JavaClassSignature(GetJavaPackage(decl), GetJavaUnqualifiedNestedTypeName(decl, "."));
}

JavaClassSignature::JavaClassSignature(std::string_view package, std::string_view type)
: packageParts(Split(package, ".")), outerTypes(WithoutTail(Split(type, "."))), type(Split(type, ".").back())
{
    CJC_ASSERT(!package.empty());
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
    constexpr static std::string_view packageDelimiter = "/";
    constexpr static std::string_view nestedTypeDelimiter = "$";

    std::string str{Join(packageParts, packageDelimiter)};
    str += packageDelimiter;

    if (IsNested()) {
        str += Join(outerTypes, nestedTypeDelimiter);
        str += nestedTypeDelimiter;
    }

    str += type;
    return str;
}

std::string JavaClassSignature::GetFQName() const
{
    constexpr static std::string_view packageDelimiter = ".";
    constexpr static std::string_view nestedTypeDelimiter = ".";
    std::string str{Join(packageParts, packageDelimiter)};
    str += packageDelimiter;

    if (IsNested()) {
        str += Join(outerTypes, nestedTypeDelimiter);
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

    return Join(outerTypes, nestedDelimiter) + std::string(nestedDelimiter) + std::string(type);
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
