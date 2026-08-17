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
#ifndef CANGJIE_SEMA_NATIVE_FFI_JAVA_UTILS
#define CANGJIE_SEMA_NATIVE_FFI_JAVA_UTILS

#include "cangjie/AST/Node.h"
#include "cangjie/Modules/ImportManager.h"

namespace Cangjie::Native::FFI::Java {

const Ptr<AST::ClassDecl> GetSyntheticClass(const ImportManager& importManager, const AST::ClassLikeDecl& cld);

std::string GetMirrorReferenceWrapperNameFromClassLike(const AST::ClassLikeDecl &mirror);

std::string GetImplRegistryCompanionClassName(const AST::ClassLikeDecl& javaImplDecl);

std::vector<Ptr<AST::ClassLikeDecl>> GetJavaMirrors(AST::File& file);

std::vector<Ptr<AST::ClassLikeDecl>> GetJavaMirrors(AST::Package& pkg);

std::vector<Ptr<AST::ClassDecl>> GetJavaImpls(AST::File& file);

std::vector<Ptr<AST::ClassDecl>> GetJavaImpls(AST::Package& pkg);

std::vector<Ptr<AST::ClassDecl>> GetJavaImplRegistryCompanions(AST::File& file);

std::vector<Ptr<AST::ClassDecl>> GetJavaImplRegistryCompanions(AST::Package& pkg);

bool IsJavaImplRegistryCompanionReferenceField(const AST::Node& node);

bool IsUserDefinedJavaImplConstructor(const AST::Decl& implMember);

/**
 * Returns fully-qualified JNI name (in form of internal JVM representation) of `javaCompatibleTy`
 * or type JNI signature for array and function types.
 *
 * @param `withObjectTypeSeparator` specifies if an object type signature should be prefixed with `L`
 * and suffixed with `;` - like `L<signature>;`. Such escaped representation is used for:
 * - function type signatures like: `(Ljava/lang/String;)Ljava/lang/Object;`;
 * - array signatures like: `[[Ljava/lang/String;`.
 */
std::string GetJniTypeSignature(const AST::Ty& javaCompatibleTy,
    bool withObjectTypeSeparator = false,
    bool withMarker = false);
std::string GetJniTypeSignature(const AST::Ty& javaCompatibleRetTy,
    const std::vector<Ptr<AST::Ty>> paramTys,
    bool withMarker = false);

/**
 * Returns fully-qualified name of the decl or fq-name specified in @JavaMirror/@JavaImpl as attribute,
 * which is used as target class name for JNI calls
 */
std::string GetJavaFQName(const AST::Decl& decl);

/**
 * Returns fully-qualified name of the decl or fq-name specified in @JavaMirror as attribute,
 * which is suitable for using Java source code:
 * - For specifying nested class '.' is used
 */
std::string GetJavaFQSourceCodeName(const AST::ClassLikeDecl& decl);

std::string GetJavaTopLevelClassName(const AST::ClassLikeDecl& decl);

std::string GetJavaUnqualifiedNestedTypeName(const AST::ClassLikeDecl& decl,
    const std::string_view nestedSeparator = "$");

/**
 * Returns package of the decl or package specified in @JavaMirror/@JavaImpl as parameter (omitting class name)
 */
std::string GetJavaPackage(const AST::Decl& decl);

/**
 * Returns name of corresponding Java method or field with respect to @ForeignName annotation
 */
std::string GetJavaMemberName(const AST::Decl& member);

std::string NormalizeJavaSignature(const std::string& sig);

} // namespace Cangjie::Native::FFI::Java

#endif // CANGJIE_SEMA_NATIVE_FFI_JAVA_UTILS
