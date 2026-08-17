// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares implementation of java member abstraction.
 */

#include "NativeFFI/Java/JavaMemberSignature.h"
#include "cangjie/AST/Match.h"
#include "cangjie/Sema/TypeManager.h"
#include "cangjie/Utils/CheckUtils.h"
#include "NativeFFI/Java/Utils.h"
#include "NativeFFI/Utils.h"

namespace Cangjie::Native::FFI::Java {
using namespace AST;

JavaMemberSignature JavaMemberSignature::FromMethod(const FuncDecl& method)
{
    CJC_ASSERT(method.outerDecl && method.outerDecl->IsClassLikeDecl());
    CJC_ASSERT(!method.TestAnyAttr(Attribute::CONSTRUCTOR, Attribute::FINALIZER));
    auto& outerDecl = *StaticAs<ASTKind::CLASS_LIKE_DECL>(method.outerDecl);
    CJC_ASSERT(outerDecl.IsJavaMirror() || outerDecl.IsJavaImpl());

    JavaMemberSignature signature(
        /* .classSignature = */ JavaClassSignature::FromDecl(outerDecl),
        /* .name = */ GetJavaMemberName(method),
        /* .signature = */ GetJniTypeSignature(*method.GetTy()),
        /* .isStatic = */ method.TestAttr(Attribute::STATIC)
    );
    return signature;
}

JavaMemberSignature JavaMemberSignature::FromConstructor(const FuncDecl& ctor, bool withMarker)
{
    static constexpr auto JAVA_CONSTRUCTOR = "<init>";
    CJC_ASSERT(ctor.outerDecl && ctor.outerDecl->IsClassLikeDecl());
    CJC_ASSERT(ctor.TestAttr(Attribute::CONSTRUCTOR));
    auto& outerDecl = *StaticAs<ASTKind::CLASS_LIKE_DECL>(ctor.outerDecl);
    CJC_ASSERT(outerDecl.IsJavaMirror() || outerDecl.IsJavaImpl());

    static auto& voidTy = *TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT);
    std::vector<Ptr<Ty>> paramTys = Native::FFI::GetParamTys(*ctor.funcBody->paramLists[0]);

    JavaMemberSignature signature(
        /* .classSignature = */ JavaClassSignature::FromDecl(outerDecl),
        /* .name = */ JAVA_CONSTRUCTOR,
        /* .signature = */ GetJniTypeSignature(voidTy, paramTys, withMarker)
    );
    return signature;
}

JavaMemberSignature JavaMemberSignature::FromProperty(const PropDecl& member)
{
    CJC_ASSERT(member.outerDecl && member.outerDecl->IsClassLikeDecl());
    auto& outerDecl = *StaticAs<ASTKind::CLASS_LIKE_DECL>(member.outerDecl);
    CJC_ASSERT(outerDecl.IsJavaMirror() || outerDecl.IsJavaImpl());

    JavaMemberSignature signature(
        /* .classSignature = */ JavaClassSignature::FromDecl(outerDecl),
        /* .name = */ GetJavaMemberName(member),
        /* .signature = */ GetJniTypeSignature(*member.GetTy()),
        /* .isStatic = */ member.TestAttr(Attribute::STATIC)
    );
    return signature;
}

JavaMemberSignature JavaMemberSignature::FromMember(const AST::Decl& member, bool withMarker)
{
    switch (member.astKind) {
        case ASTKind::PROP_DECL:
            CJC_ASSERT(!withMarker);
            return FromProperty(StaticCast<const PropDecl&>(member));
        case ASTKind::FUNC_DECL: {
            if (member.TestAttr(Attribute::CONSTRUCTOR)) {
                return FromConstructor(StaticCast<const FuncDecl&>(member), withMarker);
            }
            CJC_ASSERT(!withMarker);
            return FromMethod(StaticCast<const FuncDecl&>(member));
        }
        default: break;
    }
    CJC_ABORT(); // Unexpected branch
    // compile-error on non-returning branch hack here
    return JavaMemberSignature(JavaClassSignature::FromDecl(StaticCast<ClassLikeDecl&>(*member.outerDecl)), "", "");
}

JavaMemberSignature::JavaMemberSignature(
    JavaClassSignature javaClass,
    std::string name,
    std::string signature,
    bool isStatic) : classSignature(javaClass), name(name), signature(signature), isStatic(isStatic)
{
}

std::string JavaMemberSignature::GetClassTypeJniName() const
{
    return classSignature.GetJniClassName();
}

JavaClassSignature JavaMemberSignature::GetClassSignature() const
{
    return classSignature;
}

std::string JavaMemberSignature::GetName() const
{
    return name;
}

std::string JavaMemberSignature::GetSignature() const
{
    return signature;
}

bool JavaMemberSignature::IsStatic() const
{
    return isStatic;
}


} // namespace Cangjie::Cangjie::Native::FFI::Java
