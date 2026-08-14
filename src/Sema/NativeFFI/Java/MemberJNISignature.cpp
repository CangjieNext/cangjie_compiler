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

#include "NativeFFI/Java/MemberJNISignature.h"
#include "cangjie/AST/Match.h"
#include "cangjie/Sema/TypeManager.h"
#include "cangjie/Utils/CheckUtils.h"
#include <ostream>
#include <string_view>
#include "NativeFFI/Java/Utils.h"

namespace Cangjie::Native::FFI::Java {
using namespace AST;

MemberJNISignature MemberJNISignature::FromMethod(const FuncDecl& method)
{
    CJC_ASSERT(method.outerDecl && method.outerDecl->IsClassLikeDecl());
    CJC_ASSERT(!method.TestAnyAttr(Attribute::CONSTRUCTOR, Attribute::FINALIZER));
    auto& outerDecl = *StaticAs<ASTKind::CLASS_LIKE_DECL>(method.outerDecl);
    CJC_ASSERT(outerDecl.IsJavaMirror() || outerDecl.IsJavaImpl());

    MemberJNISignature signature(
        /* .classTypeSignature = */ GetJavaJniClassName(*outerDecl.GetTy()),
        /* .name = */ GetJavaMemberName(method),
        /* .signature = */ GetJniTypeSignature(*method.GetTy()),
        /* .isStatic = */ method.TestAttr(Attribute::STATIC)
    );
    return signature;
}

MemberJNISignature MemberJNISignature::FromConstructor(const FuncDecl& ctor, bool withMarker)
{
    static constexpr auto JAVA_CONSTRUCTOR = "<init>";
    CJC_ASSERT(ctor.outerDecl && ctor.outerDecl->IsClassLikeDecl());
    CJC_ASSERT(ctor.TestAttr(Attribute::CONSTRUCTOR));
    auto& outerDecl = *StaticAs<ASTKind::CLASS_LIKE_DECL>(ctor.outerDecl);
    CJC_ASSERT(outerDecl.IsJavaMirror() || outerDecl.IsJavaImpl());

    static auto& voidTy = *TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT);
    std::vector<Ptr<Ty>> paramTys = Native::FFI::GetParamTys(*ctor.funcBody->paramLists[0]);
    if (withMarker) {
        paramTys.push_back(CreateConstructorMarkerClassDecl()->GetTy());
    }

    MemberJNISignature signature(
        /* .classTypeSignature = */ GetJavaJniClassName(*outerDecl.GetTy()),
        /* .name = */ JAVA_CONSTRUCTOR,
        /* .signature = */ GetJniTypeSignature(??? get for function voidTy, paramTys) // TODO: for function type overload
    );
    return signature;
}

MemberJNISignature MemberJNISignature::FromProperty(const PropDecl& member)
{
    CJC_ASSERT(member.outerDecl && member.outerDecl->IsClassLikeDecl());
    auto& outerDecl = *StaticAs<ASTKind::CLASS_LIKE_DECL>(member.outerDecl);
    CJC_ASSERT(outerDecl.IsJavaMirror() || outerDecl.IsJavaImpl());

    MemberJNISignature signature(
        /* .classTypeSignature = */ GetJavaJniClassName(*outerDecl.GetTy()),
        /* .name = */ GetJavaMemberName(member),
        /* .signature = */ GetJniTypeSignature(*member.GetTy()),
        /* .isStatic = */ member.TestAttr(Attribute::STATIC)
    );
    return signature;
}

MemberJNISignature MemberJNISignature::FromMember(const AST::Decl& member, bool withMarker)
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
    return MemberJNISignature("", "", ""); // compile-error on non-returning branch hack
}

MemberJNISignature::MemberJNISignature(
    std::string classTypeSignature,
    std::string name,
    std::string signature,
    bool isStatic)
    : classTypeSignature(classTypeSignature), name(name), signature(signature), isStatic(isStatic)
{
}

std::string MemberJNISignature::GetClassTypeJniName() const
{
    return classTypeSignature;
}

std::string MemberJNISignature::GetName() const
{
    return name;
}

std::string MemberJNISignature::GetSignature() const
{
    return signature;
}

bool MemberJNISignature::IsStatic() const
{
    return isStatic;
}


} // namespace Cangjie::Cangjie::Native::FFI::Java
