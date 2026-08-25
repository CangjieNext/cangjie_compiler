// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "JniBridge.h"
#include "Utils.h"
#include "NativeFFI/Utils.h"
#include "NativeFFI/Java/Utils.h"
#include "cangjie/AST/Node.h"
#include "cangjie/AST/Types.h"
#include "cangjie/Utils/CheckUtils.h"
#include <string>

namespace Cangjie::Native::FFI::Java {

using namespace Cangjie::AST;
using namespace Interop::Java;

namespace {
Ty& GetUnderlyingTy(Ty& ty)
{
    if (Is<TypeAliasTy>(ty)) {
        return GetUnderlyingTy(*StaticCast<TypeAliasTy&>(ty).declPtr->type->GetTy());
    }
    return ty;
}

/**
 * Java Native Method Names Resolution scheme:
 * 1. `_` -> `_1`
 * 2. `.` -> '_' | (in fully-quialified names)
 */
std::string ApplyJavaNativeNameEscaping(const std::string& name)
{
    constexpr std::string_view underscopeSequence1 = "_1";

    std::string escapedName(name);
    size_t startPos = 0;
    // "_" in name should be replaced with `_1` since `_` is reserved symbol in JNI signatures
    while ((startPos = escapedName.find("_", startPos)) != std::string::npos) {
        escapedName.replace(startPos, 1, underscopeSequence1);
        startPos += underscopeSequence1.size();
        // Continue after inserted "_1" substring (2 characters).
    }

    std::replace(escapedName.begin(), escapedName.end(), '.', '_');
    return escapedName;
}

}

JniBridge::JniBridge(
    TypeManager& typeManager,
    const ImportManager& importManager,
    const BaseMangler& mangler,
    Interop::Java::Utils& utils,
    Decl& jniEnvPtrDecl,
    Decl& jniJobjectDecl,
    StructDecl& jniInterfaceDecl) : interface(typeManager, importManager, jniInterfaceDecl),
    typeManager(typeManager), mangler(mangler), utils(utils),
    jniEnvPtrDecl(jniEnvPtrDecl), jniJobjectDecl(jniJobjectDecl)
{}

OwnedPtr<FuncParam> JniBridge::CreateJniEnvParam(const std::string& name) const
{
    return CreateFuncParam(name, CreateType(&GetJniEnvPtrTy()), nullptr, &GetJniEnvPtrTy());
}

OwnedPtr<FuncParam> JniBridge::CreateJniJobjectOrJclassParam(const std::string& name) const
{
    return CreateFuncParam(name, CreateType(&GetJniJobjectDeclTy()), nullptr, &GetJniJobjectDeclTy());
}

OwnedPtr<FuncParam> JniBridge::CreateRegistryIdParam(const std::string& name) const
{
    return CreateFuncParam(name, CreateType(&GetRegistryIdJavaTy()), nullptr, &GetRegistryIdJavaTy());
}

std::string JniBridge::GetJniMethodName(const FuncDecl& method) const
{
    auto sampleJavaName = GetJavaMemberName(method);
    std::string fqname = GetJavaFQName(*method.outerDecl);
    CJC_ASSERT_WITH_MSG(!method.funcBody->paramLists.empty(), "paramLists cannot be empty");
    auto mangledFuncName = GetMangledMethodName(mangler, method.funcBody->paramLists[0]->params, sampleJavaName);

    return GetJavaNativeFunctionName(fqname, mangledFuncName);
}

std::string JniBridge::GetJniMethodNameForProp(
    const PropDecl& propDecl,
    bool isSet) const
{
    std::string varDecl = GetJavaMemberName(propDecl);
    std::string varDeclSuffix = varDecl;
    CJC_ASSERT_WITH_MSG(!varDeclSuffix.empty(), "identifier cannot be an empty string");
    varDeclSuffix[0] = static_cast<char>(toupper(varDeclSuffix[0]));
    std::string fqname = Native::FFI::Java::GetJavaFQName(*(propDecl.outerDecl));

    return GetJavaNativeFunctionName(fqname, (isSet ? "set" : "get") + varDeclSuffix + "Impl");
}

std::string JniBridge::GetJniInitCjObjectFuncName(
    const FuncDecl& ctor,
    bool isGeneratedCtor) const
{
    std::string fqname = GetJavaFQName(*(ctor.outerDecl));
    auto mangledFuncName = GetMangledJniInitCjObjectFuncName(mangler,
        ctor.funcBody->paramLists[0]->params, isGeneratedCtor);

    if (Is<EnumDecl>(ctor.outerDecl)) {
        mangledFuncName = ctor.identifier + mangledFuncName;
    }

    return GetJavaNativeFunctionName(fqname, mangledFuncName);
}

std::string JniBridge::GetJniInitCjObjectFuncNameForVarDecl(const VarDecl& ctor) const
{
    std::string fqname = GetJavaFQName(*(ctor.outerDecl));
    auto mangledFuncName = ctor.identifier.Val();
    return GetJavaNativeFunctionName(fqname, mangledFuncName + "initCJObject");
}

std::string JniBridge::GetJniDeleteCjObjectFuncName(const Decl& decl) const
{
    std::string fqname = GetJavaFQName(decl);
    return GetJavaNativeFunctionName(fqname, "deleteCJObject");
}

OwnedPtr<FuncDecl> JniBridge::CreateNativeJavaABIFunc(
    const std::string& name,
    std::vector<OwnedPtr<FuncParam>> userParams, Ptr<Ty> retTy,
    File& curFile, std::string& moduleName, std::string& fullPackageName,
    std::function<void(
        FuncDecl& f, FuncParam& jniEnv, FuncParam& objOrClass, std::vector<Ptr<FuncParam>> userParams)> builder) const
{
    std::vector<Ptr<FuncParam>> userParamsView;
    for (auto& userParam : userParams) {
        userParamsView.push_back(userParam.get());
    }

    auto params = std::move(userParams);
    auto& jniEnvParam = **params.insert(params.begin(), CreateJniEnvParam());
    auto& objOrClassParam = **params.insert(params.begin() + 1, CreateJniJobjectOrJclassParam());

    auto func = utils.CreateNativeFunc(name, std::move(params), retTy, {}, curFile, moduleName,
        fullPackageName);

    builder(*func, jniEnvParam, objOrClassParam, userParamsView);

    return func;
}

Ty& JniBridge::GetJniEnvPtrTy() const
{
    static auto& ty = GetUnderlyingTy(*jniEnvPtrDecl.GetTy());
    return ty;
}

Ty& JniBridge::GetJniJobjectDeclTy() const
{
    static auto& ty = GetUnderlyingTy(*jniJobjectDecl.GetTy());
    return ty;
}

Ty& JniBridge::GetRegistryIdJavaTy() const
{
    static auto& ty = *typeManager.GetPrimitiveTy(TypeKind::TYPE_INT64);
    return ty;
}

std::string JniBridge::GetJavaNativeFunctionName(const std::string& fqTypeName, const std::string& memberName) const
{
    return "Java_" + ApplyJavaNativeNameEscaping(fqTypeName) + "_" + ApplyJavaNativeNameEscaping(memberName);
}

/**
 * Map Cangjie type to corresponding JNI-level type used in generated native method.
 */
Ty& JniBridge::ConvertCangjieToJniTy(Ty& javaCompatibleTy) const
{
    if (javaCompatibleTy.IsString()) {
        // String is passed as jobject.
        return GetJniJobjectDeclTy();
    }
    if (javaCompatibleTy.IsCoreOptionType()) {
        CJC_ASSERT(!javaCompatibleTy.typeArgs.empty());
        auto argTy = javaCompatibleTy.typeArgs[0];
        CJC_ASSERT(IsMirror(*argTy) || IsImpl(*argTy) || argTy->IsString());
        return GetJniJobjectDeclTy();
    }
    if (IsMirror(javaCompatibleTy) || IsImpl(javaCompatibleTy)) {
        return GetJniJobjectDeclTy();
    }

    if (javaCompatibleTy.IsBuiltin()) {
        return javaCompatibleTy;
    }

    CJC_ABORT();
    return javaCompatibleTy; // never succeeding fallback
}

namespace {

using JniNonVirtualCallTy = OwnedPtr<Expr> (JniNativeInterfaceBridge::*)(
    Ptr<Expr>, OwnedPtr<Expr>&&, OwnedPtr<Expr>&&, OwnedPtr<Expr>&&) const;
using JniNonVirtualCallATy = OwnedPtr<Expr> (JniNativeInterfaceBridge::*)(
    Ptr<Expr>, OwnedPtr<Expr>&&, OwnedPtr<Expr>&&, OwnedPtr<Expr>&&, OwnedPtr<FuncArg>&&) const;

using JniVirtualInstanceCallTy = OwnedPtr<Expr> (JniNativeInterfaceBridge::*)(
    Ptr<Expr>, OwnedPtr<Expr>&&, OwnedPtr<Expr>&&) const;
using JniVirtualInstanceCallATy = OwnedPtr<Expr> (JniNativeInterfaceBridge::*)(
    Ptr<Expr>, OwnedPtr<Expr>&&, OwnedPtr<Expr>&&, OwnedPtr<FuncArg>&&) const;
using Interface = JniNativeInterfaceBridge;

JniNonVirtualCallTy MatchNonVirtualCallMethod(const Ty& retTy)
{
    switch (retTy.kind) {
        case TypeKind::TYPE_UNIT: return &Interface::CreateCallNonvirtualVoidMethodCall<Expr, Expr, Expr>;
        case TypeKind::TYPE_BOOLEAN: return &Interface::CreateCallNonvirtualBooleanMethodCall<Expr, Expr, Expr>;
        case TypeKind::TYPE_INT8: return &Interface::CreateCallNonvirtualByteMethodCall<Expr, Expr, Expr>;
        case TypeKind::TYPE_UINT16: return &Interface::CreateCallNonvirtualCharMethodCall<Expr, Expr, Expr>;
        case TypeKind::TYPE_INT16: return &Interface::CreateCallNonvirtualShortMethodCall<Expr, Expr, Expr>;
        case TypeKind::TYPE_INT32: return &Interface::CreateCallNonvirtualIntMethodCall<Expr, Expr, Expr>;
        case TypeKind::TYPE_INT64: return &Interface::CreateCallNonvirtualLongMethodCall<Expr, Expr, Expr>;
        case TypeKind::TYPE_FLOAT32: return &Interface::CreateCallNonvirtualFloatMethodCall<Expr, Expr, Expr>;
        case TypeKind::TYPE_FLOAT64: return &Interface::CreateCallNonvirtualDoubleMethodCall<Expr, Expr, Expr>;
        default: return &Interface::CreateCallNonvirtualObjectMethodCall<Expr, Expr, Expr>;
    }
}

JniNonVirtualCallATy MatchNonVirtualCallMethodA(const Ty& retTy)
{
    switch (retTy.kind) {
        case TypeKind::TYPE_UNIT: return &Interface::CreateCallNonvirtualVoidMethodACall<Expr, Expr, Expr, FuncArg>;
        case TypeKind::TYPE_BOOLEAN: return &Interface::CreateCallNonvirtualBooleanMethodACall<Expr, Expr, Expr, FuncArg>;
        case TypeKind::TYPE_INT8: return &Interface::CreateCallNonvirtualByteMethodACall<Expr, Expr, Expr, FuncArg>;
        case TypeKind::TYPE_UINT16: return &Interface::CreateCallNonvirtualCharMethodACall<Expr, Expr, Expr, FuncArg>;
        case TypeKind::TYPE_INT16: return &Interface::CreateCallNonvirtualShortMethodACall<Expr, Expr, Expr, FuncArg>;
        case TypeKind::TYPE_INT32: return &Interface::CreateCallNonvirtualIntMethodACall<Expr, Expr, Expr, FuncArg>;
        case TypeKind::TYPE_INT64: return &Interface::CreateCallNonvirtualLongMethodACall<Expr, Expr, Expr, FuncArg>;
        case TypeKind::TYPE_FLOAT32: return &Interface::CreateCallNonvirtualFloatMethodACall<Expr, Expr, Expr, FuncArg>;
        case TypeKind::TYPE_FLOAT64: return &Interface::CreateCallNonvirtualDoubleMethodACall<Expr, Expr, Expr, FuncArg>;
        default: return &Interface::CreateCallNonvirtualObjectMethodACall<Expr, Expr, Expr, FuncArg>;
    }
}

JniVirtualInstanceCallTy MatchVirtualInstanceCallMethod(const Ty& retTy)
{
    switch (retTy.kind) {
        case TypeKind::TYPE_UNIT: return &Interface::CreateCallVoidMethodCall<Expr, Expr>;
        case TypeKind::TYPE_BOOLEAN: return &Interface::CreateCallBooleanMethodCall<Expr, Expr>;
        case TypeKind::TYPE_INT8: return &Interface::CreateCallByteMethodCall<Expr, Expr>;
        case TypeKind::TYPE_UINT16: return &Interface::CreateCallCharMethodCall<Expr, Expr>;
        case TypeKind::TYPE_INT16: return &Interface::CreateCallShortMethodCall<Expr, Expr>;
        case TypeKind::TYPE_INT32: return &Interface::CreateCallIntMethodCall<Expr, Expr>;
        case TypeKind::TYPE_INT64: return &Interface::CreateCallLongMethodCall<Expr, Expr>;
        case TypeKind::TYPE_FLOAT32: return &Interface::CreateCallFloatMethodCall<Expr, Expr>;
        case TypeKind::TYPE_FLOAT64: return &Interface::CreateCallDoubleMethodCall<Expr, Expr>;
        default: return &Interface::CreateCallObjectMethodCall<Expr, Expr>;
    }
}

JniVirtualInstanceCallATy MatchVirtualInstanceCallAMethod(const Ty& retTy)
{
    switch (retTy.kind) {
        case TypeKind::TYPE_UNIT: return &Interface::CreateCallVoidMethodACall<Expr, Expr, FuncArg>; // TODO: could be just <>?
        // case TypeKind::TYPE_UNIT: return &Interface::CreateCallVoidMethodACall<>;
        case TypeKind::TYPE_BOOLEAN: return &Interface::CreateCallBooleanMethodACall<Expr, Expr, FuncArg>;
        case TypeKind::TYPE_INT8: return &Interface::CreateCallByteMethodACall<Expr, Expr, FuncArg>;
        case TypeKind::TYPE_UINT16: return &Interface::CreateCallCharMethodACall<Expr, Expr, FuncArg>;
        case TypeKind::TYPE_INT16: return &Interface::CreateCallShortMethodACall<Expr, Expr, FuncArg>;
        case TypeKind::TYPE_INT32: return &Interface::CreateCallIntMethodACall<Expr, Expr, FuncArg>;
        case TypeKind::TYPE_INT64: return &Interface::CreateCallLongMethodACall<Expr, Expr, FuncArg>;
        case TypeKind::TYPE_FLOAT32: return &Interface::CreateCallFloatMethodACall<Expr, Expr, FuncArg>;
        case TypeKind::TYPE_FLOAT64: return &Interface::CreateCallDoubleMethodACall<Expr, Expr, FuncArg>;
        default: return &Interface::CreateCallObjectMethodACall<Expr, Expr, FuncArg>;
    }
}
} // namespace

OwnedPtr<Expr> JniBridge::CreateNonvirtualJavaMethodCall(const Ty& retTy,
    Ptr<Expr> jniEnvPtr, Ptr<Expr> jobject, Ptr<Expr> jclass, Ptr<Expr> jmethod) const
{
    return (interface.*MatchNonVirtualCallMethod(retTy))
        (jniEnvPtr, ASTCloner::Clone(jobject), ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod));
}

OwnedPtr<Expr> JniBridge::CreateNonvirtualJavaMethodCall(const Ty& retTy,
    Ptr<Expr> jniEnvPtr, Ptr<Expr> jobject, Ptr<Expr> jclass, Ptr<Expr> jmethod, OwnedPtr<FuncArg> jniArgs) const
{
    return (interface.*MatchNonVirtualCallMethodA(retTy))
        (jniEnvPtr, ASTCloner::Clone(jobject), ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod), std::move(jniArgs));
}

OwnedPtr<Expr> JniBridge::CreateVirtualInstanceJavaMethodCall(const Ty& retTy,
    Ptr<Expr> jniEnvPtr, Ptr<Expr> jobject, Ptr<Expr> jmethod) const
{
    return (interface.*MatchVirtualInstanceCallMethod(retTy))
        (jniEnvPtr, ASTCloner::Clone(jobject), ASTCloner::Clone(jmethod));
}

OwnedPtr<Expr> JniBridge::CreateVirtualInstanceJavaMethodCall(const Ty& retTy,
    Ptr<Expr> jniEnvPtr, Ptr<Expr> jobject, Ptr<Expr> jmethod, OwnedPtr<FuncArg> jniArgs) const
{
    return (interface.*MatchVirtualInstanceCallAMethod(retTy))
        (jniEnvPtr, ASTCloner::Clone(jobject), ASTCloner::Clone(jmethod), std::move(jniArgs));
}

OwnedPtr<Expr> JniBridge::CreateStaticJavaMethodCall(const Ty& retTy,
    Ptr<Expr> jniEnvPtr, Ptr<Expr> jclass, Ptr<Expr> jmethod) const
{
    // TODO: shorten
    switch (retTy.kind) {
        case TypeKind::TYPE_UNIT:
            return interface.CreateCallStaticVoidMethodCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod));
        case TypeKind::TYPE_BOOLEAN:
            return interface.CreateCallStaticBooleanMethodCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod));
        case TypeKind::TYPE_INT8:
            return interface.CreateCallStaticByteMethodCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod));
        case TypeKind::TYPE_UINT16:
            return interface.CreateCallStaticCharMethodCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod));
        case TypeKind::TYPE_INT16:
            return interface.CreateCallStaticShortMethodCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod));
        case TypeKind::TYPE_INT32:
            return interface.CreateCallStaticIntMethodCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod));
        case TypeKind::TYPE_INT64:
            return interface.CreateCallStaticLongMethodCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod));
        case TypeKind::TYPE_FLOAT32:
            return interface.CreateCallStaticFloatMethodCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod));
        case TypeKind::TYPE_FLOAT64:
            return interface.CreateCallStaticDoubleMethodCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod));
        default:
            return interface.CreateCallStaticObjectMethodCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod));
    }
}

OwnedPtr<Expr> JniBridge::CreateStaticJavaMethodCall(const Ty& retTy,
    Ptr<Expr> jniEnvPtr, Ptr<Expr> jclass, Ptr<Expr> jmethod, OwnedPtr<FuncArg> jniArgs) const
{
    switch (retTy.kind) {
        case TypeKind::TYPE_UNIT:
            return interface.CreateCallStaticVoidMethodACall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod), std::move(jniArgs));
        case TypeKind::TYPE_BOOLEAN:
            return interface.CreateCallStaticBooleanMethodACall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod), std::move(jniArgs));
        case TypeKind::TYPE_INT8:
            return interface.CreateCallStaticByteMethodACall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod), std::move(jniArgs));
        case TypeKind::TYPE_UINT16:
            return interface.CreateCallStaticCharMethodACall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod), std::move(jniArgs));
        case TypeKind::TYPE_INT16:
            return interface.CreateCallStaticShortMethodACall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod), std::move(jniArgs));
        case TypeKind::TYPE_INT32:
            return interface.CreateCallStaticIntMethodACall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod), std::move(jniArgs));
        case TypeKind::TYPE_INT64:
            return interface.CreateCallStaticLongMethodACall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod), std::move(jniArgs));
        case TypeKind::TYPE_FLOAT32:
            return interface.CreateCallStaticFloatMethodACall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod), std::move(jniArgs));
        case TypeKind::TYPE_FLOAT64:
            return interface.CreateCallStaticDoubleMethodACall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod), std::move(jniArgs));
        default:
            return interface.CreateCallStaticObjectMethodACall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod), std::move(jniArgs));
    }
}

OwnedPtr<Expr> JniBridge::CreateGetStaticJavaFieldCall(const Ty& ty,
    Ptr<Expr> jniEnvPtr, Ptr<Expr> jclass, Ptr<Expr> jfield) const
{
    switch (ty.kind) {
        case TypeKind::TYPE_BOOLEAN:
            return interface.CreateGetStaticBooleanFieldCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jfield));
        case TypeKind::TYPE_INT8:
            return interface.CreateGetStaticByteFieldCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jfield));
        case TypeKind::TYPE_UINT16:
            return interface.CreateGetStaticCharFieldCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jfield));
        case TypeKind::TYPE_INT16:
            return interface.CreateGetStaticShortFieldCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jfield));
        case TypeKind::TYPE_INT32:
            return interface.CreateGetStaticIntFieldCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jfield));
        case TypeKind::TYPE_INT64:
            return interface.CreateGetStaticLongFieldCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jfield));
        case TypeKind::TYPE_FLOAT32:
            return interface.CreateGetStaticFloatFieldCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jfield));
        case TypeKind::TYPE_FLOAT64:
            return interface.CreateGetStaticDoubleFieldCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jfield));
        default:
            return interface.CreateGetStaticObjectFieldCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jfield));
    }
}

OwnedPtr<Expr> JniBridge::CreateGetInstanceJavaFieldCall(const Ty& ty,
    Ptr<Expr> jniEnvPtr, Ptr<Expr> jobject, Ptr<Expr> jfield) const
{
    switch (ty.kind) {
        case TypeKind::TYPE_BOOLEAN:
            return interface.CreateGetBooleanFieldCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jfield));
        case TypeKind::TYPE_INT8:
            return interface.CreateGetByteFieldCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jfield));
        case TypeKind::TYPE_UINT16:
            return interface.CreateGetCharFieldCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jfield));
        case TypeKind::TYPE_INT16:
            return interface.CreateGetShortFieldCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jfield));
        case TypeKind::TYPE_INT32:
            return interface.CreateGetIntFieldCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jfield));
        case TypeKind::TYPE_INT64:
            return interface.CreateGetLongFieldCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jfield));
        case TypeKind::TYPE_FLOAT32:
            return interface.CreateGetFloatFieldCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jfield));
        case TypeKind::TYPE_FLOAT64:
            return interface.CreateGetDoubleFieldCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jfield));
        default:
            return interface.CreateGetObjectFieldCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jfield));
    }
}

OwnedPtr<Expr> JniBridge::CreateSetStaticJavaFieldCall(const Ty& ty,
    Ptr<Expr> jniEnvPtr, Ptr<Expr> jclass, Ptr<Expr> jfield, Ptr<Expr> value) const
{
    switch (ty.kind) {
        case TypeKind::TYPE_BOOLEAN:
            return interface.CreateSetStaticBooleanFieldCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jfield), ASTCloner::Clone(value));
        case TypeKind::TYPE_INT8:
            return interface.CreateSetStaticByteFieldCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jfield), ASTCloner::Clone(value));
        case TypeKind::TYPE_UINT16:
            return interface.CreateSetStaticCharFieldCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jfield), ASTCloner::Clone(value));
        case TypeKind::TYPE_INT16:
            return interface.CreateSetStaticShortFieldCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jfield), ASTCloner::Clone(value));
        case TypeKind::TYPE_INT32:
            return interface.CreateSetStaticIntFieldCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jfield), ASTCloner::Clone(value));
        case TypeKind::TYPE_INT64:
            return interface.CreateSetStaticLongFieldCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jfield), ASTCloner::Clone(value));
        case TypeKind::TYPE_FLOAT32:
            return interface.CreateSetStaticFloatFieldCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jfield), ASTCloner::Clone(value));
        case TypeKind::TYPE_FLOAT64:
            return interface.CreateSetStaticDoubleFieldCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jfield), ASTCloner::Clone(value));
        default:
            return interface.CreateSetStaticObjectFieldCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jfield), ASTCloner::Clone(value));
    }
}

OwnedPtr<Expr> JniBridge::CreateSetInstanceJavaFieldCall(const Ty& ty,
    Ptr<Expr> jniEnvPtr, Ptr<Expr> jobject, Ptr<Expr> jfield, Ptr<Expr> value) const
{
    switch (ty.kind) {
        case TypeKind::TYPE_BOOLEAN:
            return interface.CreateSetBooleanFieldCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jfield), ASTCloner::Clone(value));
        case TypeKind::TYPE_INT8:
            return interface.CreateSetByteFieldCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jfield), ASTCloner::Clone(value));
        case TypeKind::TYPE_UINT16:
            return interface.CreateSetCharFieldCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jfield), ASTCloner::Clone(value));
        case TypeKind::TYPE_INT16:
            return interface.CreateSetShortFieldCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jfield), ASTCloner::Clone(value));
        case TypeKind::TYPE_INT32:
            return interface.CreateSetIntFieldCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jfield), ASTCloner::Clone(value));
        case TypeKind::TYPE_INT64:
            return interface.CreateSetLongFieldCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jfield), ASTCloner::Clone(value));
        case TypeKind::TYPE_FLOAT32:
            return interface.CreateSetFloatFieldCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jfield), ASTCloner::Clone(value));
        case TypeKind::TYPE_FLOAT64:
            return interface.CreateSetDoubleFieldCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jfield), ASTCloner::Clone(value));
        default:
            return interface.CreateSetObjectFieldCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jfield), ASTCloner::Clone(value));
    }
}

JniNativeInterfaceBridge::JniNativeInterfaceBridge(
    TypeManager& typeManager, const ImportManager& importManager, StructDecl& jniInterfaceDecl)
: typeManager(typeManager), importManager(importManager), jniInterfaceDecl(jniInterfaceDecl) {}

OwnedPtr<Expr> JniNativeInterfaceBridge::CreateJNIEnvReadCall(Ptr<Expr> env) const
{
    // readPointer<CPointer<JNINativeInterface_>>(env,0)
    auto read1 = CreateReadPointerCall(importManager, typeManager, env);
    // readPointer<JNINativeInterface_>(read1,0)
    auto read2 = CreateReadPointerCall(importManager, typeManager, read1);
    return read2;
}

} // namespace Cangjie::Native::FFI::Java
