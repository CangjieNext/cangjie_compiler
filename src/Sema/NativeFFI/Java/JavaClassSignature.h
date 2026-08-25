// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares java java class type abstraction.
 */
#ifndef CANGJIE_SEMA_NATIVE_FFI_JAVA_CLASS_SIGNATURE
#define CANGJIE_SEMA_NATIVE_FFI_JAVA_CLASS_SIGNATURE

#include <ostream>
#include <string>
#include <string_view>
#include <vector>
#include "cangjie/AST/Node.h"

namespace Cangjie::Native::FFI::Java {

struct JavaClassSignature {
public:
    static JavaClassSignature FromDecl(const AST::ClassLikeDecl& decl);
    static JavaClassSignature GetConstructorMarkerClass();

    friend std::ostream& operator<<(std::ostream& os, const JavaClassSignature& fqname);

    std::string ToString() const;

    /**
     * Returns fully-qualified name of java class in JNI (JVM internal) form:
     * `package/subpackage/OuterClass$NestedClass`
     */
    std::string GetJniClassName() const;

    /**
     * Returns fully-qualified name of java class in form of:
     * `package.subpackage.OuterClass.NestedClass`
     */
    std::string GetFQName() const;

    std::string GetUnqualifiedTypeName(const std::string_view nestedDelimiter = ".") const;

    /**
     * Is this type nested.
     * An example: type `NestedClass` in `OuterClass.NestedClass`
     *
     */
    bool IsNested() const;
    
    bool operator==(const JavaClassSignature& other) const;
    bool operator!=(const JavaClassSignature& other) const;

private:
    explicit JavaClassSignature(std::string_view package, std::string_view type);
    const std::vector<std::string> packageParts;
    const std::vector<std::string> outerTypes;
    const std::string type;
};

} // namespace Cangjie::Cangjie::Native::FFI::Java

template<>
struct std::hash<Cangjie::Native::FFI::Java::JavaClassSignature> {
    std::size_t operator()(const Cangjie::Native::FFI::Java::JavaClassSignature& fqname) const noexcept
    {
        return std::hash<std::string>{}(fqname.ToString());
    }
};

#endif // CANGJIE_SEMA_NATIVE_FFI_JAVA_CLASS_SIGNATURE
