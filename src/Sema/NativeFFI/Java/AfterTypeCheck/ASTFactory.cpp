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
#include "NativeFFI/Java/JavaMemberSignature.h"
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
        auto jniMethodCall = jni.CreateNonvirtualJavaMethodCall(*call.GetTy(), jniEnvPtr, jobject.get(), jclass.get(),
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

OwnedPtr<Expr> ASTFactory::CreateJavaMethodCall(AfterTypeCheckContext& ctx,
    Ptr<Expr> jniEnvPtr, Ptr<Expr> jobjectInstance,
    JavaMemberSignature method, Ty& retTy, std::vector<OwnedPtr<Expr>> args, bool isVirtual) const
{
    auto curFile = jniEnvPtr->curFile;
    CJC_NULLPTR_CHECK(curFile);
    CJC_ASSERT(method.IsStatic() || jobjectInstance); // instance cannot be null if method is non-static.

    auto block = CreateBlock({}, &retTy);

    static auto convertJniValueToJavaCompatible = [this](OwnedPtr<Expr> jniValue, const Ty& expectedTy) {
        auto ret = std::move(jniValue);
        if (!expectedTy.IsPrimitive()) {
            ret = ilib.CreateJavaEntityJobjectCall(std::move(ret));
        }
        return ilib.ConvertJavaResultToCJ(std::move(ret), &expectedTy, *ilib.GetJobjectDecl()); // TODO: fix scope decl
    };

    auto jclass = jclassCache.CreateJClassAccess(ctx, method.GetClassSignature(), jniEnvPtr);
    auto jmethod = jmethodIdCache.CreateJMethodIdAccess(ctx, method, ASTCloner::Clone(jclass.get()), jniEnvPtr);

    // 1. let callRes: retTy
    auto& callResVar = StaticCast<VarDecl&>(*block->body.emplace_back(CreateTmpVarDecl(nullptr, nullptr)));
    callResVar.SetTy(&retTy);

    if (args.empty()) {
        // 2. jniCallRes = Java_CFFI_JavaEntity((*jniEnv)->CallMethod(jclass/jobject, jmethod))
        auto jniMethodCall = method.IsStatic()
        ? jni.CreateStaticJavaMethodCall(retTy, jniEnvPtr, jclass, jmethod)
        : isVirtual
            ? jni.CreateVirtualInstanceJavaMethodCall(retTy, jniEnvPtr, jobjectInstance, jmethod)
            : jni.CreateNonvirtualJavaMethodCall(retTy, jniEnvPtr, jobjectInstance, jclass, jmethod);
        callResVar.initializer = convertJniValueToJavaCompatible(std::move(jniMethodCall), retTy);
    } else {
        static auto createInoutVArrayArg = [this, &curFile](VarDecl& varrayArg) {
            return CreateInoutVArrayArg(WithinFile(CreateRefExpr(varrayArg), curFile), typeManager);
        };
        // 0. var jniArgs: VArray<jvalue, $x> = [args...]
        auto& jniArgs = StaticCast<VarDecl&>(*block->body.emplace(block->body.begin(),
            CreateTmpVArrayVarDecl(ConvertToJValues(std::move(args)), *curFile)));

        // 2. jniCallRes = Java_CFFI_JavaEntity((*jniEnv)->CallMethod(jclass/jobject, jmethod, inout jniArgs))
        auto jniMethodCall = method.IsStatic()
        ? jni.CreateStaticJavaMethodCall(retTy, jniEnvPtr, jclass, jmethod, createInoutVArrayArg(jniArgs))
        : isVirtual
            ? jni.CreateVirtualInstanceJavaMethodCall(retTy, jniEnvPtr, jobjectInstance, jmethod,
                createInoutVArrayArg(jniArgs))
            : jni.CreateNonvirtualJavaMethodCall(retTy, jniEnvPtr, jobjectInstance, jclass, jmethod,
                createInoutVArrayArg(jniArgs));
        callResVar.initializer = convertJniValueToJavaCompatible(std::move(jniMethodCall), retTy);
    }
    // 3. handlePendingException()
    block->body.push_back(ilib.CreateJNIHandlePendingExceptionCall(jniEnvPtr));
    // 4. return jniCallRes
    block->body.push_back(WithinFile(CreateRefExpr(callResVar), curFile));
    return block;
}

OwnedPtr<Expr> ASTFactory::CreateJavaMethodCall(AfterTypeCheckContext& ctx,
    File& curFile, Ptr<Expr> jobjectInstance,
    JavaMemberSignature method, Ty& retTy, std::vector<OwnedPtr<Expr>> args, bool isVirtual) const
{
    static auto jniEnvPtrDecl = ilib.GetJniEnvPtrDecl();
    auto jniEnvVar = CreateTmpVarDecl(jniEnvPtrDecl->type, ilib.CreateGetJniEnvCall(&curFile));

    auto methodCall = CreateJavaMethodCall(ctx,
        WithinFile(CreateRefExpr(*jniEnvVar), &curFile), jobjectInstance, method, retTy, std::move(args), isVirtual);

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
