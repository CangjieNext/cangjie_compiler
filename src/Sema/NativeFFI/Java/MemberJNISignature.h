// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares java member declaration abstraction.
 */
#ifndef CANGJIE_SEMA_NATIVE_FFI_JAVA_MEMBER_JNI_SIGNATURE
#define CANGJIE_SEMA_NATIVE_FFI_JAVA_MEMBER_JNI_SIGNATURE

#include <sstream>
#include <string>
#include "cangjie/AST/Node.h"

namespace Cangjie::Native::FFI::Java {

/**
 * Abstraction over Java member (method, constructor, field) declaration
 */
struct MemberJNISignature {
public:

    /**
     * class type name in JNI form (internal JVM representation).
     * Example: "java/lang/String"
     */
    std::string GetClassTypeJniName() const;

    /**
     * member name.
     * Example: "toString"
     */
    std::string GetName() const;

    /**
     * member type signature in JNI form.
     * Example: "()V"
     */
    std::string GetSignature() const;

    bool IsStatic() const;

    static MemberJNISignature FromMethod(const AST::FuncDecl& method);

    static MemberJNISignature FromConstructor(const AST::FuncDecl& ctor, bool withMarker = false);

    static MemberJNISignature FromProperty(const AST::PropDecl& member);

    static MemberJNISignature FromMember(const AST::Decl& member, bool withMarker = false);

private:
    MemberJNISignature(std::string classTypeSignature, std::string name, std::string signature, bool isStatic = false);

    const std::string classTypeSignature;
    const std::string name;
    const std::string signature;

    const bool isStatic;
};

} // namespace Cangjie::Native::FFI::Java

template<>
struct std::hash<Cangjie::Native::FFI::Java::MemberJNISignature> {
    std::size_t operator()(const Cangjie::Native::FFI::Java::MemberJNISignature& member) const noexcept
    {
        std::stringstream str{};
        str << member.GetClassTypeJniName();
        str << "$";
        str << member.GetName();
        str << "$$";
        str << member.GetSignature();
        str << std::to_string(member.IsStatic());
        return std::hash<std::string>{}(str.str());
    }
};

#endif // CANGJIE_SEMA_NATIVE_FFI_JAVA_MEMBER_JNI_SIGNATURE
