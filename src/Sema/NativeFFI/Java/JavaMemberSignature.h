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
#ifndef CANGJIE_SEMA_NATIVE_FFI_JAVA_MEMBER_SIGNATURE
#define CANGJIE_SEMA_NATIVE_FFI_JAVA_MEMBER_SIGNATURE

#include <sstream>
#include <string>
#include "NativeFFI/Java/JavaClassSignature.h"
#include "cangjie/AST/Node.h"

namespace Cangjie::Native::FFI::Java {

/**
 * Abstraction over Java member (method, constructor, field) declaration
 */
struct JavaMemberSignature {
public:

    /**
     * class type name in JNI form (internal JVM representation).
     * Example: "java/lang/String"
     */
    std::string GetClassTypeJniName() const;

    JavaClassSignature GetClassSignature() const;

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
    
    bool IsField() const;
    
    bool IsMethod() const;
    
    bool IsConstructor() const;

    static JavaMemberSignature FromMethod(const AST::FuncDecl& method);

    static JavaMemberSignature FromConstructor(const AST::FuncDecl& ctor, bool withMarker = false);

    static JavaMemberSignature FromProperty(const AST::PropDecl& member);

    static JavaMemberSignature FromMember(const AST::Decl& member, bool withMarker = false);
    
    bool operator==(const JavaMemberSignature& other) const;
    bool operator!=(const JavaMemberSignature& other) const;

private:
    enum class JavaMemberKind {
        FIELD,
        METHOD,
        CONSTRUCTOR
    };
    
    explicit JavaMemberSignature(JavaClassSignature javaClass, std::string name, std::string signature,
        JavaMemberKind kind, bool isStatic = false);

    const JavaClassSignature classSignature;
    const std::string name;
    const std::string signature;

    const bool isStatic;
    const JavaMemberKind kind;
};

} // namespace Cangjie::Native::FFI::Java

template<>
struct std::hash<Cangjie::Native::FFI::Java::JavaMemberSignature> {
    std::size_t operator()(const Cangjie::Native::FFI::Java::JavaMemberSignature& member) const noexcept
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

#endif // CANGJIE_SEMA_NATIVE_FFI_JAVA_MEMBER_SIGNATURE
