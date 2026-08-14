// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "ASTFactory.h"
#include "NativeFFI/Java/AfterTypeCheck/AfterTypeCheckContext.h"
#include "NativeFFI/Java/AfterTypeCheck/InteropLibBridge.h"
#include "NativeFFI/Java/AfterTypeCheck/JniBridge.h"
#include "NativeFFI/Java/AfterTypeCheck/Utils.h"
#include "NativeFFI/Java/Utils.h"
#include "cangjie/AST/Create.h"
#include "cangjie/AST/Types.h"
#include "cangjie/Sema/TypeManager.h"
#include "cangjie/Utils/CheckUtils.h"


namespace Cangjie::Native::FFI::Java {
using namespace AST;

namespace {

// a: VArray<T, $X> -> inout(a) of type T
OwnedPtr<FuncArg> CreateInoutVArrayArg(OwnedPtr<Expr> expr, TypeManager& typeManager)
{
    CJC_ASSERT(expr->TyKind() == TypeKind::TYPE_VARRAY);

    auto arg = CreateFuncArg(std::move(expr));
    auto* varrayTy = StaticCast<VArrayTy>(arg->GetTy());
    CJC_ASSERT_WITH_MSG(!varrayTy->typeArgs.empty(), "VArray must be generic");
    arg->withInout = true;
    arg->SetTy(typeManager.GetPointerTy(varrayTy->typeArgs[0]));
    return arg;
}
} // namespace

ASTFactory::ASTFactory(
    TypeManager& typeManager, InteropLibBridge& ilib, JniBridge& jni,
    JClassCache& jclassCache, JMethodIdCache& jmethodIdCache)
    : typeManager(typeManager), ilib(ilib), jni(jni), jclassCache(jclassCache), jmethodIdCache(jmethodIdCache)
{
}

OwnedPtr<Expr> ASTFactory::CreateNewJavaObjectCall(
    AfterTypeCheckContext& ctx,
    Ptr<Expr> jniEnvPtr, Ty& javaClassTy, FuncDecl& constructor, std::vector<OwnedPtr<Expr>> args) const
{
    CJC_NULLPTR_CHECK(jniEnvPtr->curFile);
    auto curFile = jniEnvPtr->curFile;

    auto block = CreateBlock({}, ilib.GetJavaEntityTy());

    auto jclass = jclassCache.CreateJClassAccess(ctx, javaClassTy, jniEnvPtr);
    // NOTE: user-defined java-impl constructor requires an extra marker to be passed as an argument to java
    bool withMarker = IsUserDefinedJavaImplConstructor(constructor);
    auto jmethod = jmethodIdCache.CreateJMethodIdAccess(ctx, constructor, ASTCloner::Clone(jclass.get()), jniEnvPtr,
        withMarker);

    if (args.empty() && !withMarker) {
        // call without arguments, no need to pass empty value.
        block->body.push_back(ilib.CreateJavaEntityJobjectCall(jni.interface.CreateNewObjectCall(jniEnvPtr,
            ASTCloner::Clone(jclass.get()), ASTCloner::Clone(jmethod.get()))));
        return block;
    }
    // call with arguments - arguments are constructed as JValue-s within VArray and passed as in-out value into JNI.
    if (withMarker) {
        args.push_back(ilib.CreateJavaEntityNullCall(curFile));
    }
    // 1. var jniArgs: VArray<jvalue, $x> = [args...]
    auto& jniArgs = StaticCast<VarDecl&>(
        *block->body.emplace_back(CreateTmpVArrayVarDecl(ConvertToJValues(std::move(args)), *curFile)));

    // 2. let jniCallRes: Java_CFFI_JavaEntity
    auto& callResVar = StaticCast<VarDecl&>(*block->body.emplace_back(CreateTmpVarDecl(nullptr, nullptr)));
    callResVar.SetTy(ilib.GetJavaEntityTy());
    // 3. jniCallRes = Java_CFFI_JavaEntity((*jniEnv)->NewObjectA(jclass, jmethod, inout jniArgs))
    callResVar.initializer = ilib.CreateJavaEntityJobjectCall(
        jni.interface.CreateNewObjectACall(jniEnvPtr,
            ASTCloner::Clone(jclass.get()),
            ASTCloner::Clone(jmethod.get()),
            CreateInoutVArrayArg(WithinFile(CreateRefExpr(jniArgs), curFile), typeManager)));

    // 4. handlePendingException()
    block->body.push_back(ilib.CreateJNIHandlePendingExceptionCall(jniEnvPtr));
    // 5. return jniCallRes
    block->body.push_back(WithinFile(CreateRefExpr(callResVar), curFile));
    return block;
}

OwnedPtr<Expr> ASTFactory::CreateNewJavaObjectCall(
    AfterTypeCheckContext& ctx,
    File& curFile, Ty& javaClassTy, FuncDecl& constructor, std::vector<OwnedPtr<Expr>> args) const
{
    static auto jniEnvPtrDecl = ilib.GetJniEnvPtrDecl();
    auto jniEnvVar = CreateTmpVarDecl(jniEnvPtrDecl->type, ilib.CreateGetJniEnvCall(&curFile));

    auto newObjectCall = CreateNewJavaObjectCall(ctx,
        WithinFile(CreateRefExpr(*jniEnvVar), &curFile),
        javaClassTy, constructor, std::move(args));

    // Assume that CreateNewJavaObjectCall always returns block.
    auto block = StaticAs<ASTKind::BLOCK>(newObjectCall.get());
    block->body.insert(block->body.begin(), std::move(jniEnvVar));
    return newObjectCall;
}

namespace {
OwnedPtr<Expr> CreateNonvirtualJavaMethodCall(
    JniBridge& jni, const Ty& retTy, Ptr<Expr> jniEnvPtr, Ptr<Expr> jobject, Ptr<Expr> jclass, Ptr<Expr> jmethod)
{
    switch (retTy.kind) {
        case TypeKind::TYPE_UNIT:
            return jni.interface.CreateCallNonvirtualVoidMethodCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod));
        case TypeKind::TYPE_BOOLEAN:
            return jni.interface.CreateCallNonvirtualBooleanMethodCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod));
        case TypeKind::TYPE_INT8:
            return jni.interface.CreateCallNonvirtualByteMethodCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod));
        case TypeKind::TYPE_UINT16:
            return jni.interface.CreateCallNonvirtualCharMethodCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod));
        case TypeKind::TYPE_INT16:
            return jni.interface.CreateCallNonvirtualShortMethodCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod));
        case TypeKind::TYPE_INT32:
            return jni.interface.CreateCallNonvirtualIntMethodCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod));
        case TypeKind::TYPE_INT64:
            return jni.interface.CreateCallNonvirtualLongMethodCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod));
        case TypeKind::TYPE_FLOAT32:
            return jni.interface.CreateCallNonvirtualFloatMethodCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod));
        case TypeKind::TYPE_FLOAT64:
            return jni.interface.CreateCallNonvirtualDoubleMethodCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod));
        default:
            return jni.interface.CreateCallNonvirtualObjectMethodCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod));
    }
}

OwnedPtr<Expr> CreateNonvirtualJavaMethodCall(JniBridge& jni, const Ty& retTy, Ptr<Expr> jniEnvPtr, Ptr<Expr> jobject,
    Ptr<Expr> jclass, Ptr<Expr> jmethod, OwnedPtr<FuncArg> jniArgs)
{
    switch (retTy.kind) {
        case TypeKind::TYPE_UNIT:
            return jni.interface.CreateCallNonvirtualVoidMethodACall(jniEnvPtr, ASTCloner::Clone(jobject),
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod), std::move(jniArgs));
        case TypeKind::TYPE_BOOLEAN:
            return jni.interface.CreateCallNonvirtualBooleanMethodACall(jniEnvPtr, ASTCloner::Clone(jobject),
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod), std::move(jniArgs));
        case TypeKind::TYPE_INT8:
            return jni.interface.CreateCallNonvirtualByteMethodACall(jniEnvPtr, ASTCloner::Clone(jobject),
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod), std::move(jniArgs));
        case TypeKind::TYPE_UINT16:
            return jni.interface.CreateCallNonvirtualCharMethodACall(jniEnvPtr, ASTCloner::Clone(jobject),
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod), std::move(jniArgs));
        case TypeKind::TYPE_INT16:
            return jni.interface.CreateCallNonvirtualShortMethodACall(jniEnvPtr, ASTCloner::Clone(jobject),
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod), std::move(jniArgs));
        case TypeKind::TYPE_INT32:
            return jni.interface.CreateCallNonvirtualIntMethodACall(jniEnvPtr, ASTCloner::Clone(jobject),
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod), std::move(jniArgs));
        case TypeKind::TYPE_INT64:
            return jni.interface.CreateCallNonvirtualLongMethodACall(jniEnvPtr, ASTCloner::Clone(jobject),
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod), std::move(jniArgs));
        case TypeKind::TYPE_FLOAT32:
            return jni.interface.CreateCallNonvirtualFloatMethodACall(jniEnvPtr, ASTCloner::Clone(jobject),
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod), std::move(jniArgs));
        case TypeKind::TYPE_FLOAT64:
            return jni.interface.CreateCallNonvirtualDoubleMethodACall(jniEnvPtr, ASTCloner::Clone(jobject),
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod), std::move(jniArgs));
        default:
            return jni.interface.CreateCallNonvirtualObjectMethodACall(jniEnvPtr, ASTCloner::Clone(jobject),
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod), std::move(jniArgs));
    }
}

OwnedPtr<Expr> CreateVirtualInstanceJavaMethodCall(JniBridge& jni,
    const Ty& retTy, Ptr<Expr> jniEnvPtr, Ptr<Expr> jobject, Ptr<Expr> jmethod)
{
    switch (retTy.kind) {
        case TypeKind::TYPE_UNIT:
            return jni.interface.CreateCallVoidMethodCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jmethod));
        case TypeKind::TYPE_BOOLEAN:
            return jni.interface.CreateCallBooleanMethodCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jmethod));
        case TypeKind::TYPE_INT8:
            return jni.interface.CreateCallByteMethodCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jmethod));
        case TypeKind::TYPE_UINT16:
            return jni.interface.CreateCallCharMethodCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jmethod));
        case TypeKind::TYPE_INT16:
            return jni.interface.CreateCallShortMethodCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jmethod));
        case TypeKind::TYPE_INT32:
            return jni.interface.CreateCallIntMethodCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jmethod));
        case TypeKind::TYPE_INT64:
            return jni.interface.CreateCallLongMethodCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jmethod));
        case TypeKind::TYPE_FLOAT32:
            return jni.interface.CreateCallFloatMethodCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jmethod));
        case TypeKind::TYPE_FLOAT64:
            return jni.interface.CreateCallDoubleMethodCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jmethod));
        default:
            return jni.interface.CreateCallObjectMethodCall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jmethod));
    }
}

OwnedPtr<Expr> CreateVirtualInstanceJavaMethodCall(JniBridge& jni,
    const Ty& retTy, Ptr<Expr> jniEnvPtr, Ptr<Expr> jobject, Ptr<Expr> jmethod, OwnedPtr<FuncArg> jniArgs)
{
    switch (retTy.kind) {
        case TypeKind::TYPE_UNIT:
            return jni.interface.CreateCallVoidMethodACall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jmethod), std::move(jniArgs));
        case TypeKind::TYPE_BOOLEAN:
            return jni.interface.CreateCallBooleanMethodACall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jmethod), std::move(jniArgs));
        case TypeKind::TYPE_INT8:
            return jni.interface.CreateCallByteMethodACall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jmethod), std::move(jniArgs));
        case TypeKind::TYPE_UINT16:
            return jni.interface.CreateCallCharMethodACall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jmethod), std::move(jniArgs));
        case TypeKind::TYPE_INT16:
            return jni.interface.CreateCallShortMethodACall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jmethod), std::move(jniArgs));
        case TypeKind::TYPE_INT32:
            return jni.interface.CreateCallIntMethodACall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jmethod), std::move(jniArgs));
        case TypeKind::TYPE_INT64:
            return jni.interface.CreateCallLongMethodACall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jmethod), std::move(jniArgs));
        case TypeKind::TYPE_FLOAT32:
            return jni.interface.CreateCallFloatMethodACall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jmethod), std::move(jniArgs));
        case TypeKind::TYPE_FLOAT64:
            return jni.interface.CreateCallDoubleMethodACall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jmethod), std::move(jniArgs));
        default:
            return jni.interface.CreateCallObjectMethodACall(jniEnvPtr,
                ASTCloner::Clone(jobject), ASTCloner::Clone(jmethod), std::move(jniArgs));
    }
}

OwnedPtr<Expr> CreateStaticJavaMethodCall(JniBridge& jni,
    const Ty& retTy, Ptr<Expr> jniEnvPtr, Ptr<Expr> jclass, Ptr<Expr> jmethod)
{
    switch (retTy.kind) {
        case TypeKind::TYPE_UNIT:
            return jni.interface.CreateCallStaticVoidMethodCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod));
        case TypeKind::TYPE_BOOLEAN:
            return jni.interface.CreateCallStaticBooleanMethodCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod));
        case TypeKind::TYPE_INT8:
            return jni.interface.CreateCallStaticByteMethodCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod));
        case TypeKind::TYPE_UINT16:
            return jni.interface.CreateCallStaticCharMethodCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod));
        case TypeKind::TYPE_INT16:
            return jni.interface.CreateCallStaticShortMethodCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod));
        case TypeKind::TYPE_INT32:
            return jni.interface.CreateCallStaticIntMethodCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod));
        case TypeKind::TYPE_INT64:
            return jni.interface.CreateCallStaticLongMethodCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod));
        case TypeKind::TYPE_FLOAT32:
            return jni.interface.CreateCallStaticFloatMethodCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod));
        case TypeKind::TYPE_FLOAT64:
            return jni.interface.CreateCallStaticDoubleMethodCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod));
        default:
            return jni.interface.CreateCallStaticObjectMethodCall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod));
    }
}

OwnedPtr<Expr> CreateStaticJavaMethodCall(JniBridge& jni,
    const Ty& retTy, Ptr<Expr> jniEnvPtr, Ptr<Expr> jclass, Ptr<Expr> jmethod, OwnedPtr<FuncArg> jniArgs)
{
    switch (retTy.kind) {
        case TypeKind::TYPE_UNIT:
            return jni.interface.CreateCallStaticVoidMethodACall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod), std::move(jniArgs));
        case TypeKind::TYPE_BOOLEAN:
            return jni.interface.CreateCallStaticBooleanMethodACall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod), std::move(jniArgs));
        case TypeKind::TYPE_INT8:
            return jni.interface.CreateCallStaticByteMethodACall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod), std::move(jniArgs));
        case TypeKind::TYPE_UINT16:
            return jni.interface.CreateCallStaticCharMethodACall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod), std::move(jniArgs));
        case TypeKind::TYPE_INT16:
            return jni.interface.CreateCallStaticShortMethodACall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod), std::move(jniArgs));
        case TypeKind::TYPE_INT32:
            return jni.interface.CreateCallStaticIntMethodACall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod), std::move(jniArgs));
        case TypeKind::TYPE_INT64:
            return jni.interface.CreateCallStaticLongMethodACall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod), std::move(jniArgs));
        case TypeKind::TYPE_FLOAT32:
            return jni.interface.CreateCallStaticFloatMethodACall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod), std::move(jniArgs));
        case TypeKind::TYPE_FLOAT64:
            return jni.interface.CreateCallStaticDoubleMethodACall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod), std::move(jniArgs));
        default:
            return jni.interface.CreateCallStaticObjectMethodACall(jniEnvPtr,
                ASTCloner::Clone(jclass), ASTCloner::Clone(jmethod), std::move(jniArgs));
    }
}
} // namespace

OwnedPtr<Expr> ASTFactory::CreateSuperJavaMethodCall(AfterTypeCheckContext& ctx,
    Ptr<Expr> jniEnvPtr, ClassDecl& child, CallExpr& call, std::vector<OwnedPtr<Expr>> args) const
{
    CJC_ASSERT(call.baseFunc && call.baseFunc->astKind == ASTKind::MEMBER_ACCESS);
    auto& ma = *StaticAs<ASTKind::MEMBER_ACCESS>(call.baseFunc.get());
    CJC_ASSERT(ma.baseExpr && ma.baseExpr->astKind == ASTKind::REF_EXPR);
    auto& ref = *StaticAs<ASTKind::REF_EXPR>(ma.baseExpr.get().get());
    CJC_ASSERT(ref.isSuper);
    CJC_ASSERT(call.resolvedFunction && call.resolvedFunction->outerDecl);
    auto& superFunc = *call.resolvedFunction;

    // Only a class is expected to be a target of super call
    auto& predecessor = StaticCast<ClassDecl&>(*superFunc.outerDecl);
    CJC_ASSERT(predecessor.IsJavaMirror()); // since no Impl <: Impl inheritance is allowed

    auto curFile = call.curFile;
    CJC_NULLPTR_CHECK(curFile);

    auto block = CreateBlock({}, call.GetTy());

    auto jobject = ilib.CreateAsJniJobjectCall(CreateJavaRefCall(child, curFile));
    auto jclass = jclassCache.CreateJClassAccess(ctx, *predecessor.GetTy(), jniEnvPtr);
    auto jmethod = jmethodIdCache.CreateJMethodIdAccess(ctx, superFunc, ASTCloner::Clone(jclass.get()), jniEnvPtr);

    if (call.args.empty()) {
        auto jniMethodCall = CreateNonvirtualJavaMethodCall(jni,
            *call.GetTy(), jniEnvPtr, jobject.get(), jclass.get(), jmethod.get());
        // TODO: exception handling missing?!

        if (!call.GetTy()->IsPrimitive()) {
            jniMethodCall = ilib.CreateJavaEntityJobjectCall(std::move(jniMethodCall));
        }
        block->body.push_back(ilib.ConvertJavaResultToCJ(std::move(jniMethodCall), call.GetTy(), child));
    } else {
        auto& jniArgs = StaticCast<VarDecl&>(
            *block->body.emplace_back(CreateTmpVArrayVarDecl(ConvertToJValues(std::move(args)), *curFile)));
        auto jniMethodCall = CreateNonvirtualJavaMethodCall(jni, *call.GetTy(), jniEnvPtr, jobject.get(), jclass.get(),
            jmethod.get(), CreateInoutVArrayArg(WithinFile(CreateRefExpr(jniArgs), curFile), typeManager));
        // TODO: exception handling missing?!

        if (!call.GetTy()->IsPrimitive()) {
            jniMethodCall = ilib.CreateJavaEntityJobjectCall(std::move(jniMethodCall));
        }
        block->body.push_back(ilib.ConvertJavaResultToCJ(std::move(jniMethodCall), call.GetTy(), child));
    }
    return block;
}

OwnedPtr<AST::Expr> ASTFactory::CreateSuperJavaMethodCall(
    AfterTypeCheckContext& ctx, AST::ClassDecl& child, AST::CallExpr& call, std::vector<OwnedPtr<AST::Expr>> args) const
{
    auto curFile = call.curFile;
    static auto jniEnvPtrDecl = ilib.GetJniEnvPtrDecl();
    auto jniEnvVar = CreateTmpVarDecl(jniEnvPtrDecl->type, ilib.CreateGetJniEnvCall(curFile));

    auto methodCall =
        CreateSuperJavaMethodCall(ctx, WithinFile(CreateRefExpr(*jniEnvVar), curFile), child, call, std::move(args));

    // Assume that CreateSuperJavaMethodCall always returns block.
    auto block = StaticAs<ASTKind::BLOCK>(methodCall.get());
    block->body.insert(block->body.begin(), std::move(jniEnvVar));
    return methodCall;
}

OwnedPtr<Expr> ASTFactory::CreateJavaMethodCall(AfterTypeCheckContext& ctx, Ptr<Expr> jniEnvPtr,
    ClassLikeDecl&
    const MemberJNISignature& member, std::vector<OwnedPtr<Expr>> javaCompatibleArgs) const
{
    auto curFile = jniEnvPtr->curFile;
    CJC_NULLPTR_CHECK(curFile);
    auto& instanceDecl = StaticCast<ClassLikeDecl&>(*method.outerDecl);
    CJC_ASSERT(instanceDecl.IsJavaMirror() || instanceDecl.IsJavaImpl());

    auto& retTy = *StaticCast<FuncTy*>(method.GetTy())->retTy;
    auto block = CreateBlock({}, &retTy);

    static auto getInstanceJobject = [this, &instanceDecl, &curFile]() {
        return ilib.CreateAsJniJobjectCall(CreateJavaRefCall(instanceDecl, curFile));
    };

    static auto convertJniValueToJavaCompatible = [this, &instanceDecl](OwnedPtr<Expr> jniValue, const Ty& expectedTy) {
        auto ret = std::move(jniValue);
        if (!expectedTy.IsPrimitive()) {
            ret = ilib.CreateJavaEntityJobjectCall(std::move(ret));
        }
        return ilib.ConvertJavaResultToCJ(std::move(ret), &expectedTy, instanceDecl);
    };

    auto jclass = jclassCache.CreateJClassAccess(ctx, *instanceDecl.GetTy(), jniEnvPtr);
    auto jmethod = jmethodIdCache.CreateJMethodIdAccess(ctx, method, ASTCloner::Clone(jclass.get()), jniEnvPtr);

    // 1. let callRes: retTy
    auto& callResVar = StaticCast<VarDecl&>(*block->body.emplace_back(CreateTmpVarDecl(nullptr, nullptr)));
    callResVar.SetTy(&retTy);

    if (args.empty()) {
        // 2. jniCallRes = Java_CFFI_JavaEntity((*jniEnv)->CallMethod(jclass/jobject, jmethod))
        auto jniMethodCall = isStatic
        ? CreateStaticJavaMethodCall(jni, retTy, jniEnvPtr, jclass, jmethod)
        : CreateVirtualInstanceJavaMethodCall(jni, retTy, jniEnvPtr, getInstanceJobject(), jmethod);
        callResVar.initializer = convertJniValueToJavaCompatible(std::move(jniMethodCall), retTy);
    } else {
        static auto createInoutVArrayArg = [this, &curFile](VarDecl& varrayArg) {
            return CreateInoutVArrayArg(WithinFile(CreateRefExpr(varrayArg), curFile), typeManager);
        };
        // 0. var jniArgs: VArray<jvalue, $x> = [args...]
        auto& jniArgs = StaticCast<VarDecl&>(*block->body.emplace(block->body.begin(),
            CreateTmpVArrayVarDecl(ConvertToJValues(std::move(args)), *curFile)));

        // 2. jniCallRes = Java_CFFI_JavaEntity((*jniEnv)->CallMethod(jclass/jobject, jmethod, inout jniArgs))
        auto jniMethodCall = isStatic
        ? CreateStaticJavaMethodCall(jni, retTy, jniEnvPtr, jclass, jmethod, createInoutVArrayArg(jniArgs))
        : CreateVirtualInstanceJavaMethodCall(jni, retTy, jniEnvPtr, getInstanceJobject(), jmethod,
            createInoutVArrayArg(jniArgs));
        callResVar.initializer = convertJniValueToJavaCompatible(std::move(jniMethodCall), retTy);
    }
    // 3. handlePendingException()
    block->body.push_back(ilib.CreateJNIHandlePendingExceptionCall(jniEnvPtr));
    // 4. return jniCallRes
    block->body.push_back(WithinFile(CreateRefExpr(callResVar), curFile));
    return block;



}


OwnedPtr<Expr> ASTFactory::CreateJavaMethodCall(AfterTypeCheckContext& ctx, Ptr<Expr> jniEnvPtr,
    FuncDecl& method, bool isStatic, std::vector<OwnedPtr<Expr>> args) const
{
    auto curFile = jniEnvPtr->curFile;
    CJC_NULLPTR_CHECK(curFile);
    auto& instanceDecl = StaticCast<ClassLikeDecl&>(*method.outerDecl);
    CJC_ASSERT(instanceDecl.IsJavaMirror() || instanceDecl.IsJavaImpl());

    auto& retTy = *StaticCast<FuncTy*>(method.GetTy())->retTy;
    auto block = CreateBlock({}, &retTy);

    static auto getInstanceJobject = [this, &instanceDecl, &curFile]() {
        return ilib.CreateAsJniJobjectCall(CreateJavaRefCall(instanceDecl, curFile));
    };

    static auto convertJniValueToJavaCompatible = [this, &instanceDecl](OwnedPtr<Expr> jniValue, const Ty& expectedTy) {
        auto ret = std::move(jniValue);
        if (!expectedTy.IsPrimitive()) {
            ret = ilib.CreateJavaEntityJobjectCall(std::move(ret));
        }
        return ilib.ConvertJavaResultToCJ(std::move(ret), &expectedTy, instanceDecl);
    };

    auto jclass = jclassCache.CreateJClassAccess(ctx, *instanceDecl.GetTy(), jniEnvPtr);
    auto jmethod = jmethodIdCache.CreateJMethodIdAccess(ctx, method, ASTCloner::Clone(jclass.get()), jniEnvPtr);

    // 1. let callRes: retTy
    auto& callResVar = StaticCast<VarDecl&>(*block->body.emplace_back(CreateTmpVarDecl(nullptr, nullptr)));
    callResVar.SetTy(&retTy);

    if (args.empty()) {
        // 2. jniCallRes = Java_CFFI_JavaEntity((*jniEnv)->CallMethod(jclass/jobject, jmethod))
        auto jniMethodCall = isStatic
        ? CreateStaticJavaMethodCall(jni, retTy, jniEnvPtr, jclass, jmethod)
        : CreateVirtualInstanceJavaMethodCall(jni, retTy, jniEnvPtr, getInstanceJobject(), jmethod);
        callResVar.initializer = convertJniValueToJavaCompatible(std::move(jniMethodCall), retTy);
    } else {
        static auto createInoutVArrayArg = [this, &curFile](VarDecl& varrayArg) {
            return CreateInoutVArrayArg(WithinFile(CreateRefExpr(varrayArg), curFile), typeManager);
        };
        // 0. var jniArgs: VArray<jvalue, $x> = [args...]
        auto& jniArgs = StaticCast<VarDecl&>(*block->body.emplace(block->body.begin(),
            CreateTmpVArrayVarDecl(ConvertToJValues(std::move(args)), *curFile)));

        // 2. jniCallRes = Java_CFFI_JavaEntity((*jniEnv)->CallMethod(jclass/jobject, jmethod, inout jniArgs))
        auto jniMethodCall = isStatic
        ? CreateStaticJavaMethodCall(jni, retTy, jniEnvPtr, jclass, jmethod, createInoutVArrayArg(jniArgs))
        : CreateVirtualInstanceJavaMethodCall(jni, retTy, jniEnvPtr, getInstanceJobject(), jmethod,
            createInoutVArrayArg(jniArgs));
        callResVar.initializer = convertJniValueToJavaCompatible(std::move(jniMethodCall), retTy);
    }
    // 3. handlePendingException()
    block->body.push_back(ilib.CreateJNIHandlePendingExceptionCall(jniEnvPtr));
    // 4. return jniCallRes
    block->body.push_back(WithinFile(CreateRefExpr(callResVar), curFile));
    return block;
}

OwnedPtr<Expr> ASTFactory::CreateJavaMethodCall(AfterTypeCheckContext& ctx, File& curFile,
    FuncDecl& method, bool isStatic, std::vector<OwnedPtr<Expr>> args) const
{
    static auto jniEnvPtrDecl = ilib.GetJniEnvPtrDecl();
    auto jniEnvVar = CreateTmpVarDecl(jniEnvPtrDecl->type, ilib.CreateGetJniEnvCall(&curFile));

    auto methodCall = CreateJavaMethodCall(ctx,
        WithinFile(CreateRefExpr(*jniEnvVar), &curFile), method, isStatic, std::move(args));

    // Assume that CreateJavaMethodCall always returns block.
    auto block = StaticAs<ASTKind::BLOCK>(methodCall.get());
    block->body.insert(block->body.begin(), std::move(jniEnvVar));
    return methodCall;
}

OwnedPtr<VarDecl> ASTFactory::CreateTmpVArrayVarDecl(std::vector<OwnedPtr<Expr>> args, File& curFile) const
{
    auto jvalueTy = ilib.GetJValueTy();
    CJC_NULLPTR_CHECK(jvalueTy);
    auto varrayTy = typeManager.GetVArrayTy(*jvalueTy, static_cast<int64_t>(args.size()));
    auto argsArray = CreateArrayLit(std::move(args), varrayTy);
    auto argsVar = CreateTmpVarDecl(nullptr, std::move(argsArray));
    argsVar->isVar = true;
    argsVar->SetTy(argsVar->initializer->GetTy());
    CopyBasicInfo(argsVar->initializer.get(), argsVar.get());
    argsVar->curFile = &curFile;
    return argsVar;
}

std::vector<OwnedPtr<Expr>> ASTFactory::ConvertToJValues(std::vector<OwnedPtr<Expr>> values) const
{
    for (auto& value : values) {
        value = ilib.CreateJValueExpr(std::move(value));
    }
    return values;
}

std::vector<OwnedPtr<Expr>> ASTFactory::CreateParamsUsage(const FuncParamList& paramList) const
{
    auto curFile = paramList.curFile;
    std::vector<OwnedPtr<Expr>> args;
    args.reserve(paramList.params.size());
    for (auto& param : paramList.params) {
        args.emplace_back(WithinFile(CreateRefExpr(*param), curFile));
    }
    return args;
}

std::vector<OwnedPtr<AST::Expr>> ASTFactory::ExtractArgExprs(const std::vector<OwnedPtr<FuncArg>>& args) const
{
    std::vector<OwnedPtr<Expr>> argExprs;
    for (auto& arg : args) {
        auto argExpr = ASTCloner::Clone(arg->expr.get());
        CJC_NULLPTR_CHECK(argExpr);
        argExprs.push_back(std::move(argExpr));
    }
    return argExprs;
}
}
