// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares auxiliary methods for java interop implementation
 */
#include "Utils.h"
#include "NativeFFI/Java/AfterTypeCheck/Utils.h"
#include "cangjie/AST/Node.h"
#include "cangjie/AST/Utils.h"
#include "cangjie/Utils/CheckUtils.h"

namespace Cangjie::Native::FFI::Java {
using namespace Cangjie::AST;
using namespace Cangjie::Interop::Java;

const Ptr<ClassDecl> GetSyntheticClass(const ImportManager& importManager, const ClassLikeDecl& cld)
{
    ClassDecl* synthetic =
        importManager.GetImportedDecl<ClassDecl>(cld.fullPackageName, GetMirrorReferenceWrapperNameFromClassLike(cld));

    CJC_NULLPTR_CHECK(synthetic);

    return Ptr(synthetic);
}

std::string GetMirrorReferenceWrapperNameFromClassLike(const ClassLikeDecl& mirrorDecl)
{
    constexpr auto wrapperNameSuffix = "$impl";
    CJC_ASSERT(mirrorDecl.IsInterfaceDecl() || mirrorDecl.IsAbstractClass());
    CJC_ASSERT(mirrorDecl.IsJavaMirror());
    return mirrorDecl.identifier.Val() + wrapperNameSuffix;
}

std::string GetImplRegistryCompanionClassName(const ClassLikeDecl& javaImplDecl)
{
    constexpr auto wrapperNameSuffix = "$reg";
    CJC_ASSERT(IsImpl(javaImplDecl));
    return javaImplDecl.identifier.Val() + wrapperNameSuffix;
}

std::vector<Ptr<AST::ClassLikeDecl>> GetJavaMirrors(AST::File& file)
{
    std::vector<Ptr<ClassLikeDecl>> mirrors;
    for (auto& decl : file.decls) {
        if (decl->IsJavaMirror()) {
            auto mirror = StaticAs<ASTKind::CLASS_LIKE_DECL>(decl.get());
            mirrors.emplace_back(mirror);
        }
    }
    return mirrors;
}

std::vector<Ptr<AST::ClassLikeDecl>> GetJavaMirrors(AST::Package& pkg)
{
    std::vector<Ptr<ClassLikeDecl>> mirrors;
    for (auto& file : pkg.files) {
        auto fileMirrors = GetJavaMirrors(*file);
        std::move(fileMirrors.begin(), fileMirrors.end(), std::back_inserter(mirrors));
    }
    return mirrors;
}

std::vector<Ptr<AST::ClassDecl>> GetJavaImpls(AST::File& file)
{
    std::vector<Ptr<ClassDecl>> impls;
    for (auto& decl : file.decls) {
        if (IsImpl(*decl)) {
            if (auto impl = As<ASTKind::CLASS_DECL>(decl.get())) {
                impls.push_back(impl);
            }
        }
    }
    return impls;
}

std::vector<Ptr<AST::ClassDecl>> GetJavaImpls(AST::Package& pkg)
{
    std::vector<Ptr<ClassDecl>> impls;
    for (auto& file : pkg.files) {
        auto fileImpls = GetJavaImpls(*file);
        std::move(fileImpls.begin(), fileImpls.end(), std::back_inserter(impls));
    }
    return impls;
}

std::vector<Ptr<AST::ClassDecl>> GetJavaImplRegistryCompanions(AST::File& file)
{
    std::vector<Ptr<ClassDecl>> companions;
    for (auto& decl : file.decls) {
        if (IsImplRegistryCompanion(*decl)) {
            auto companion = StaticAs<ASTKind::CLASS_DECL>(decl.get());
            companions.emplace_back(companion);
        }
    }
    return companions;
}

std::vector<Ptr<AST::ClassDecl>> GetJavaImplRegistryCompanions(AST::Package& pkg)
{
    std::vector<Ptr<ClassDecl>> impls;
    for (auto& file : pkg.files) {
        auto fileImpls = GetJavaImplRegistryCompanions(*file);
        std::move(fileImpls.begin(), fileImpls.end(), std::back_inserter(impls));
    }
    return impls;
}

constexpr std::string_view JAVA_IMPL_REGISTRY_COMPANION_REFERENCE_FIELD_NAME_IN_REFERENCE_WRAPPER = "$reg";

bool IsJavaImplRegistryCompanionReferenceField(const AST::Node& node)
{
    if (node.astKind != ASTKind::VAR_DECL) {
        return false;
    }
    auto field = dynamic_cast<const VarDecl*>(&node);
    if (!field) {
        return false;
    }

    if (!field->outerDecl || !IsImplReferenceWrapper(*field->outerDecl)) {
        return false;
    }

    return field->identifier == JAVA_IMPL_REGISTRY_COMPANION_REFERENCE_FIELD_NAME_IN_REFERENCE_WRAPPER;
}

bool IsUserDefinedJavaImplConstructor(const AST::Decl& implMember)
{
    if (!implMember.TestAttr(Attribute::CONSTRUCTOR)) {
        return false;
    }

    // Skip original primary constructors and only consider its FuncDecl counterpart.
    auto ctor = As<ASTKind::FUNC_DECL>(&implMember);
    if (!ctor) {
        return false;
    }
    auto hasParams = !ctor->funcBody->paramLists.empty() && !ctor->funcBody->paramLists[0]->params.empty();

    // Primary constructor is desugared as fresh FuncDecl with COMPILER_ADD attribute.
    auto isPrimary = implMember.TestAttr(Attribute::PRIMARY_CONSTRUCTOR, Attribute::COMPILER_ADD);
    // Visit implicit constructors too (compiler added without any parameters).
    auto isImplicit = implMember.TestAttr(Attribute::COMPILER_ADD, Attribute::IMPLICIT_ADD) && !hasParams;
    if (!isPrimary && !isImplicit && implMember.TestAttr(Attribute::COMPILER_ADD)) {
        return false;
    }
    return true;
}

namespace {

/**
 * Returns fully-qualified name of target decl in extend
 */
std::string GetJavaFQNameFromExtendDecl(const ExtendDecl& extendDecl)
{
    auto rt = DynamicCast<const RefType *>(extendDecl.extendedType.get());
    CJC_ASSERT(rt);
    return GetJavaFQName(*rt->ref.target);
}

Ptr<std::string> GetJavaMirrorAnnoAttr(const ClassLikeDecl& decl)
{
    for (auto& anno : decl.annotations) {
        if (anno->kind != AnnotationKind::JAVA_MIRROR && anno->kind != AnnotationKind::JAVA_IMPL) {
            continue;
        }

        CJC_ASSERT(anno->args.size() < 2); // It is empty if < 2
        if (anno->args.empty()) {
            break;
        }

        CJC_ASSERT(anno->args[0]->expr->astKind == ASTKind::LIT_CONST_EXPR);
        auto lce = As<ASTKind::LIT_CONST_EXPR>(anno->args[0]->expr.get());
        CJC_ASSERT(lce);
        return &lce->stringValue;
    }
    return nullptr;
}

std::vector<std::string> GetFQNameParts(std::string_view name)
{
    std::vector<std::string> res;
    res.push_back("");

    bool wasBackslash = false;
    for (std::size_t i = 0; i < name.length(); ++i) {
        switch (name[i]) {
            case '\\':
                if (wasBackslash) {
                    res.back() += '\\';
                }
                wasBackslash = true;
                continue;
            case '$':
                if (wasBackslash) {
                    res.back() += '$';
                } else {
                    res.push_back("");
                }
                wasBackslash = false;
                continue;
            default:
                if (wasBackslash) {
                    res.back() += '\\';
                }
                res.back() += name[i];
                wasBackslash = false;
                continue;
        }
    }
    if (wasBackslash) {
        res.back() += '\\';
    }

    return res;
}

template <typename I>
std::string StringJoin(I begin, I end, std::string_view separator)
{
    std::string res;
    for (auto it = begin; it != end; ++it) {
        if (it != begin) {
            res += separator;
        }
        res += *it;
    }
    return res;
}

std::string GetFQNameJoinBy(const std::string& name, std::string_view separator)
{
    auto parts = GetFQNameParts(name);
    return StringJoin(parts.begin(), parts.end(), separator);
}
} // namespace

std::string GetJniTypeSignature(const AST::Ty& javaCompatibleTy, bool withinArray = false)
{
    constexpr static std::string_view javaStringJniName = "java/lang/String";
    static std::map<TypeKind, std::string> primitiveTypeMapper = {
        {TypeKind::TYPE_UNIT, "V"},
        {TypeKind::TYPE_BOOLEAN, "Z"},
        {TypeKind::TYPE_INT8, "B"},
        {TypeKind::TYPE_UINT16, "C"},
        {TypeKind::TYPE_INT16, "S"},
        {TypeKind::TYPE_INT32, "I"},
        {TypeKind::TYPE_INT64, "J"},
        {TypeKind::TYPE_FLOAT32, "F"},
        {TypeKind::TYPE_FLOAT64, "D"},
    };
    static auto wrapAsArrayElementSignature = [](const std::string_view elementSignature) {
        return std::string{"L"} + std::string{elementSignature} + ";";
    };

    if (javaCompatibleTy.IsPrimitive()) {
        return primitiveTypeMapper[javaCompatibleTy.kind];
    }

    if (javaCompatibleTy.IsString()) {
        return withinArray
            ? wrapAsArrayElementSignature(javaStringJniName)
            : std::string{javaStringJniName};
    }
    // if (javaCompatibleTy.IsStructArray()) {
        // return "[" + GetJniTypeSignature(*javaCompatibleTy.typeArgs[0], /* withinArray = */ true);
    // }

    if (javaCompatibleTy.IsCoreOptionType()) {
        CJC_ASSERT_WITH_MSG(!javaCompatibleTy.typeArgs.empty(), "Option type must be generic");
        auto& argTy = *javaCompatibleTy.typeArgs[0];
        return GetJniTypeSignature(argTy, withinArray);
        // if (IsJArray(argTy)) {
            // jsig = GetJniTypeSignature(argTy);
        // } else {
            // jsig = "L" + NormalizeJavaSignature(GetJavaObjectTypeName(argTy)) + ";";
        // }
    }

    if (javaCompatibleTy.IsClassLike()) {
        if (IsJArray(*StaticCast<ClassLikeTy&>(javaCompatibleTy).commonDecl)) {
            return "[" + GetJniTypeSignature(*javaCompatibleTy.typeArgs[0], /* withinArray = */ true);
        } else {
            NormalizeJavaSignature(GetJavaFQName(*StaticCast<ClassLikeTy&>(javaCompatibleTy).commonDecl));
            // jsig = "L" + NormalizeJavaSignature(GetJavaFQName(*StaticCast<ClassLikeTy&>(cjtype).commonDecl)) + ";";
        }
    }

    if (javaCompatibleTy.IsFunc()) {
        auto& funcTy = StaticCast<FuncTy&>(javaCompatibleTy);
        std::string signature = "(";
        for (auto paramTy : funcTy.paramTys) {
            signature.append(GetJniTypeSignature(*paramTy, withinArray));
        }
        signature.append(")");
        signature.append(GetJniTypeSignature(*funcTy.retTy, withinArray));
        return signature;
    }

    CJC_ABORT(); // Unreachable state. This function should be called only on java-compatible types.
    return "";
}

std::string GetJavaFQName(const Decl& decl)
{
    if (auto extendDecl = DynamicCast<const ExtendDecl*>(&decl)) {
        return GetJavaFQNameFromExtendDecl(*extendDecl);
    }
    if (auto classlikeDecl = DynamicCast<const ClassLikeDecl*>(&decl)) {
        auto attr = GetJavaMirrorAnnoAttr(*classlikeDecl);
        if (attr) {
            return GetFQNameJoinBy(*attr, "$");
        }
    }
    return decl.GetFullPackageName() + "." + decl.identifier;
}

std::string GetJavaFQSourceCodeName(const ClassLikeDecl& decl)
{
    auto attr = GetJavaMirrorAnnoAttr(decl);
    return attr ? GetFQNameJoinBy(*attr, ".") : (decl.GetFullPackageName() + "." + decl.identifier);
}

DestructedJavaClassName DestructJavaClassName(const ClassLikeDecl& decl)
{
    auto attr = GetJavaMirrorAnnoAttr(decl);
    if (!attr) {
        return {decl.GetFullPackageName(), decl.identifier, decl.identifier};
    }

    auto parts = GetFQNameParts(*attr);
    CJC_ASSERT(parts.size() > 0);
    auto ind = parts[0].find_last_of('.');
    if (ind == std::string::npos) {
        return {.packageName = std::nullopt,
            .topLevelClassName = parts[0],
            .fullClassName = StringJoin(parts.begin(), parts.end(), ".")};
    }
    auto package = parts[0].substr(0, ind);
    parts[0] = parts[0].substr(ind + 1);
    return {.packageName = package,
        .topLevelClassName = parts[0],
        .fullClassName = StringJoin(parts.begin(), parts.end(), ".")};
}

} // namespace Cangjie::Native::FFI::Java
