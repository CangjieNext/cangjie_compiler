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
#ifndef CANGJIE_SEMA_NATIVE_FFI_JAVA_FQ_NAME
#define CANGJIE_SEMA_NATIVE_FFI_JAVA_FQ_NAME

#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace Cangjie::Native::FFI::Java {

enum JavaClassNameKind {
    CLASS, PRIMITIVE, ARRAY, VOID
};

struct JavaTypeName {
public:
    virtual JavaClassNameKind GetKind() const = 0;
    friend std::ostream& operator<<(std::ostream& os, const JavaTypeName& fqname);

    std::string ToString() const;
    std::string ToJniClassNameString() const;

protected:
    virtual ~JavaTypeName() = default;
    std::string type;

private:

};

struct JavaClassName : JavaTypeName {

    const std::vector<std::string_view> packageParts;
    const std::vector<std::string_view> outerTypes;
};


struct PrimitiveClassName : JavaTypeName {

};

struct VoidClassName : JavaTypeName {

};

struct ArrayClassName : JavaTypeName {

    const Cangjie::OwnedPtr<JavaTypeName> inner;
};



struct JavaFQName {
public:
    explicit JavaFQName(const std::string_view package, const std::string_view type) noexcept;
    explicit JavaFQName(
        const std::string_view package,
        const std::vector<std::string_view> outerTypes,
        const std::string_view type) noexcept;

    bool IsJArray() const;
    size_t GetJArrayDimension() const;

    friend std::ostream& operator<<(std::ostream& os, const JavaFQName& fqname);

    std::string GetPackage() const;
    std::string ToString() const;
    std::string ToJniClassNameString() const;

    const std::vector<std::string_view> packageParts;
    // const std::string_view package;
    const std::string_view type;
    const std::vector<std::string_view> outerTypes;

private:
    const size_t arrayDimension;
};

} // namespace Cangjie::Cangjie::Native::FFI::Java

template<>
struct std::hash<Cangjie::Native::FFI::Java::JavaFQName> {
    std::size_t operator()(const Cangjie::Native::FFI::Java::JavaFQName& fqname) const noexcept
    {
        return std::hash<std::string>{}(fqname.ToString());
    }
};

#endif // CANGJIE_SEMA_NATIVE_FFI_JAVA_FQ_NAME
