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
#include "cangjie/AST/Node.h"

namespace Cangjie::Native::FFI::Java {

enum JavaTypeNameKind {
    CLASS, PRIMITIVE, ARRAY, VOID
};


struct JavaClassSignature {
public:

    static JavaClassSignature FromDecl(const AST::ClassLikeDecl& decl);

    friend std::ostream& operator<<(std::ostream& os, const JavaClassSignature& fqname);

    std::string ToString() const;
    std::string ToJniClassNameString() const;

    /**
     * Is this type nested.
     * An example: type `NestedClass` in `OuterClass.NestedClass`
     *
     */
    bool IsNested() const;
private:
    JavaClassSignature(std::string_view package, std::string_view type);
    const std::vector<std::string_view> packageParts;
    const std::vector<std::string_view> outerTypes;
    const std::string_view type;
};

} // namespace Cangjie::Cangjie::Native::FFI::Java

template<>
struct std::hash<Cangjie::Native::FFI::Java::JavaClassSignature> {
    std::size_t operator()(const Cangjie::Native::FFI::Java::JavaClassSignature& fqname) const noexcept
    {
        return std::hash<std::string>{}(fqname.ToString());
    }
};

#endif // CANGJIE_SEMA_NATIVE_FFI_JAVA_FQ_NAME
