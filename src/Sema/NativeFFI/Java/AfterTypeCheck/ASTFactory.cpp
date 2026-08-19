// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "ASTFactory.h"
#include "NativeFFI/Java/AfterTypeCheck/AfterTypeCheckContext.h"
#include "NativeFFI/Java/AfterTypeCheck/InteropLibBridge.h"
#include "NativeFFI/Java/AfterTypeCheck/JniBridge.h"
#include "NativeFFI/Java/CachingApi/JFieldIdCache.h"
#include "NativeFFI/Java/JavaMemberSignature.h"
#include "cangjie/AST/Create.h"
#include "cangjie/AST/Node.h"
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
    JClassCache& jclassCache, JMethodIdCache& jmethodIdCache, JFieldIdCache& jfieldIdCache)
    : typeManager(typeManager), ilib(ilib), jni(jni),
    jclassCache(jclassCache), jmethodIdCache(jmethodIdCache), jfieldIdCache(jfieldIdCache)
{
}

OwnedPtr<Expr> ASTFactory::CreateNewJavaObjectCall(
    AfterTypeCheckContext& ctx,
    Ptr<Expr> jniEnvPtr, JavaMemberSignature constructor, std::vector<OwnedPtr<Expr>> args) const
{
    CJC_NULLPTR_CHECK(jniEnvPtr->curFile);
    auto curFile = jniEnvPtr->curFile;

    auto block = CreateBlock({}, ilib.GetJavaEntityTy());

    auto jclass = jclassCache.CreateJClassAccess(ctx, constructor.GetClassSignature(), jniEnvPtr);
    auto jmethod = jmethodIdCache.CreateJMethodIdAccess(ctx, constructor, ASTCloner::Clone(jclass.get()), jniEnvPtr);

    if (args.empty()) {
        // call without arguments, no need to pass empty value.
        block->body.push_back(ilib.CreateJavaEntityJobjectCall(jni.interface.CreateNewObjectCall(jniEnvPtr,
            ASTCloner::Clone(jclass.get()), ASTCloner::Clone(jmethod.get()))));
        return block;
    }
    // call with arguments - arguments are constructed as JValue-s within VArray and passed as in-out value into JNI.
    // 1. var jniArgs: VArray<jvalue, $x> = [args...]
    auto& jniArgs = StaticCast<VarDecl&>(
        *block->body.emplace_back(CreateTmpVArrayVarDecl(ConvertToJValues(std::move(args)), *curFile)));

    // 2. let jniCallRes: Java_CFFI_JavaEntity
    auto& callResVar = StaticCast<VarDecl&>(*block->body.emplace_back(CreateTmpVarDecl(nullptr, nullptr)));
    callResVar.SetTy(ilib.GetJavaEntityTy());
    // 3. jniCallRes = Java_CFFI_JavaEntity((*jniEnv)->NewObjectA(jclass, jmethod, inout jniArgs))
    callResVar.initializer = ilib.CreateJavaEntityJobjectCall(
        jni.interface.CreateNewObjectACall(jniEnvPtr,
            CreateFuncArg(ASTCloner::Clone(jclass.get())),
            CreateFuncArg(ASTCloner::Clone(jmethod.get())),
            CreateInoutVArrayArg(WithinFile(CreateRefExpr(jniArgs), curFile), typeManager)));

    // 4. handlePendingException()
    block->body.push_back(ilib.CreateJNIHandlePendingExceptionCall(jniEnvPtr));
    // 5. return jniCallRes
    block->body.push_back(WithinFile(CreateRefExpr(callResVar), curFile));
    return block;
}

OwnedPtr<Expr> ASTFactory::CreateNewJavaObjectCall(
    AfterTypeCheckContext& ctx,
    File& curFile, JavaMemberSignature constructor, std::vector<OwnedPtr<Expr>> args) const
{
    static auto jniEnvPtrDecl = ilib.GetJniEnvPtrDecl();
    auto jniEnvVar = CreateTmpVarDecl(jniEnvPtrDecl->type, ilib.CreateGetJniEnvCall(&curFile));

    auto newObjectCall = CreateNewJavaObjectCall(ctx,
        WithinFile(CreateRefExpr(*jniEnvVar), &curFile),
        constructor, std::move(args));

    // Assume that CreateNewJavaObjectCall always returns block.
    auto block = StaticAs<ASTKind::BLOCK>(newObjectCall.get());
    block->body.insert(block->body.begin(), std::move(jniEnvVar));
    return newObjectCall;
}

OwnedPtr<Expr> ASTFactory::CreateNonvirtualJavaMethodCall(AfterTypeCheckContext& ctx,
    Ptr<Expr> jniEnvPtr, Ptr<Expr> jobjectInstance,
    JavaMemberSignature method, Ty& retTy, std::vector<OwnedPtr<Expr>> javaCompatibleArgs) const
{
    CJC_NULLPTR_CHECK(jobjectInstance);
    CJC_ASSERT(!method.IsStatic());
    return CreateJavaMethodCall(ctx,
        jniEnvPtr, jobjectInstance, method, retTy,
        std::move(javaCompatibleArgs),
        /* isVirtual = */ false);
}

OwnedPtr<Expr> ASTFactory::CreateNonvirtualJavaMethodCall(AfterTypeCheckContext& ctx,
    File& curFile, Ptr<Expr> jobjectInstance,
    JavaMemberSignature method, Ty& retTy, std::vector<OwnedPtr<Expr>> javaCompatibleArgs) const
{
    CJC_NULLPTR_CHECK(jobjectInstance);
    CJC_ASSERT(!method.IsStatic());
    return CreateJavaMethodCall(ctx,
        curFile, jobjectInstance, method, retTy,
        std::move(javaCompatibleArgs),
        /* isVirtual = */ false);
}

OwnedPtr<Expr> ASTFactory::CreateStaticJavaMethodCall(AfterTypeCheckContext& ctx,
    Ptr<Expr> jniEnvPtr,
    JavaMemberSignature method, Ty& retTy, std::vector<OwnedPtr<Expr>> javaCompatibleArgs) const
{
    CJC_ASSERT(method.IsStatic());
    return CreateJavaMethodCall(ctx,
        jniEnvPtr, nullptr, method, retTy,
        std::move(javaCompatibleArgs),
        /* isVirtual = */ false);
}

OwnedPtr<Expr> ASTFactory::CreateStaticJavaMethodCall(AfterTypeCheckContext& ctx,
    File& curFile,
    JavaMemberSignature method, Ty& retTy, std::vector<OwnedPtr<Expr>> javaCompatibleArgs) const
{
    CJC_ASSERT(method.IsStatic());
    return CreateJavaMethodCall(ctx,
        curFile, nullptr, method, retTy,
        std::move(javaCompatibleArgs),
        /* isVirtual = */ false);
}

OwnedPtr<Expr> ASTFactory::CreateVirtualJavaMethodCall(AfterTypeCheckContext& ctx,
    Ptr<Expr> jniEnvPtr, Ptr<Expr> jobjectInstance,
    JavaMemberSignature method, Ty& retTy, std::vector<OwnedPtr<Expr>> javaCompatibleArgs) const
{
    CJC_NULLPTR_CHECK(jobjectInstance);
    CJC_ASSERT(!method.IsStatic());
    return CreateJavaMethodCall(ctx,
        jniEnvPtr, jobjectInstance, method, retTy,
        std::move(javaCompatibleArgs),
        /* isVirtual = */ true);
}

OwnedPtr<Expr> ASTFactory::CreateVirtualJavaMethodCall(AfterTypeCheckContext& ctx,
    File& curFile, Ptr<Expr> jobjectInstance,
    JavaMemberSignature method, Ty& retTy, std::vector<OwnedPtr<Expr>> javaCompatibleArgs) const
{
    CJC_NULLPTR_CHECK(jobjectInstance);
    CJC_ASSERT(!method.IsStatic());
    return CreateJavaMethodCall(ctx,
        curFile, jobjectInstance, method, retTy,
        std::move(javaCompatibleArgs),
        /* isVirtual = */ true);
}

OwnedPtr<Expr> ASTFactory::CreateJavaMethodCall(AfterTypeCheckContext& ctx,
    Ptr<Expr> jniEnvPtr, Ptr<Expr> jobjectInstance,
    JavaMemberSignature method, Ty& retTy, std::vector<OwnedPtr<Expr>> args, bool isVirtual) const
{
    auto curFile = jniEnvPtr->curFile;
    CJC_NULLPTR_CHECK(curFile);
    CJC_ASSERT(method.IsMethod());
    CJC_ASSERT(method.IsStatic() || jobjectInstance); // instance cannot be null if method is non-static.

    auto block = CreateBlock({}, &retTy);

    static auto convertJniValueToJavaCompatible = [this](OwnedPtr<Expr> jniValue, Ty& expectedTy) {
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
        auto createInoutVArrayArg = [this, &curFile](VarDecl& varrayArg) {
            return CreateInoutVArrayArg(WithinFile(CreateRefExpr(varrayArg), curFile), typeManager);
        };
        // 0. var jniArgs: VArray<jvalue, $x> = [args...]
        auto& jniArgs = StaticCast<VarDecl&>(**block->body.emplace(block->body.begin(),
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

    auto jniEnvRef = WithinFile(CreateRefExpr(*jniEnvVar), &curFile);
    auto methodCall = CreateJavaMethodCall(ctx, jniEnvRef, jobjectInstance, method, retTy, std::move(args), isVirtual);

    // Assume that CreateJavaMethodCall always returns block.
    auto block = StaticAs<ASTKind::BLOCK>(methodCall.get());
    block->body.insert(block->body.begin(), std::move(jniEnvVar));
    return methodCall;
}

OwnedPtr<AST::Expr> ASTFactory::CreateInstanceJavaFieldGetCall(AfterTypeCheckContext& ctx,
    AST::File& curFile, Ptr<AST::Expr> jobjectInstance,
    JavaMemberSignature field, AST::Ty& ty) const
{
    return CreateJavaFieldGetCall(ctx, curFile, jobjectInstance, field, ty);
}

OwnedPtr<AST::Expr> ASTFactory::CreateStaticJavaFieldGetCall(AfterTypeCheckContext& ctx,
    AST::File& curFile,
    JavaMemberSignature field, AST::Ty& ty) const
{
    return CreateJavaFieldGetCall(ctx, curFile, nullptr, field, ty);
}

OwnedPtr<AST::Expr> ASTFactory::CreateInstanceJavaFieldSetCall(AfterTypeCheckContext& ctx,
    AST::File& curFile, Ptr<AST::Expr> jobjectInstance,
    JavaMemberSignature field, AST::Ty& ty, OwnedPtr<AST::Expr> value) const
{
    return CreateJavaFieldSetCall(ctx, curFile, jobjectInstance, field, ty, std::move(value));
}

OwnedPtr<AST::Expr> ASTFactory::CreateStaticJavaFieldSetCall(AfterTypeCheckContext& ctx,
    AST::File& curFile,
    JavaMemberSignature field, AST::Ty& ty, OwnedPtr<AST::Expr> value) const
{
    return CreateJavaFieldSetCall(ctx, curFile, nullptr, field, ty, std::move(value));
}

OwnedPtr<AST::Expr> ASTFactory::CreateJavaFieldGetCall(AfterTypeCheckContext& ctx,
    Ptr<AST::Expr> jniEnvPtr, Ptr<Expr> jobjectInstance,
    JavaMemberSignature field, AST::Ty& ty) const
{
    auto curFile = jniEnvPtr->curFile;
    CJC_ASSERT(field.IsField());
    CJC_ASSERT(field.IsStatic() || jobjectInstance); // instance cannot be null if field is non-static.
    auto block = CreateBlock({}, &ty);
    
    static auto convertJniValueToJavaCompatible = [this](OwnedPtr<Expr> jniValue, Ty& expectedTy) {
        auto ret = std::move(jniValue);
        if (!expectedTy.IsPrimitive()) {
            ret = ilib.CreateJavaEntityJobjectCall(std::move(ret));
        }
        return ilib.ConvertJavaResultToCJ(std::move(ret), &expectedTy, *ilib.GetJobjectDecl()); // TODO: fix scope decl
    };
    
    auto jclass = jclassCache.CreateJClassAccess(ctx, field.GetClassSignature(), jniEnvPtr);
    auto jfield = jfieldIdCache.CreateJFieldIdAccess(ctx, field, ASTCloner::Clone(jclass.get()), jniEnvPtr);
    
    // 1. let callRes: retTy
    auto& callResVar = StaticCast<VarDecl&>(*block->body.emplace_back(CreateTmpVarDecl(nullptr, nullptr)));
    callResVar.SetTy(&ty);
    auto jniFieldGetCall = field.IsStatic()
        ? jni.CreateGetStaticJavaFieldCall(ty, jniEnvPtr, jclass, jfield)
        : jni.CreateGetInstanceJavaFieldCall(ty, jniEnvPtr, jobjectInstance, jfield);
    callResVar.initializer = convertJniValueToJavaCompatible(std::move(jniFieldGetCall), ty);
    
    // 3. handlePendingException()
    block->body.push_back(ilib.CreateJNIHandlePendingExceptionCall(jniEnvPtr));
    // 4. return jniCallRes
    block->body.push_back(WithinFile(CreateRefExpr(callResVar), curFile));
    return block;
}

OwnedPtr<AST::Expr> ASTFactory::CreateJavaFieldGetCall(AfterTypeCheckContext& ctx,
    AST::File& curFile, Ptr<Expr> jobjectInstance,
    JavaMemberSignature field, AST::Ty& ty) const
{
    static auto jniEnvPtrDecl = ilib.GetJniEnvPtrDecl();
    auto jniEnvVar = CreateTmpVarDecl(jniEnvPtrDecl->type, ilib.CreateGetJniEnvCall(&curFile));

    auto fieldGetCall = CreateJavaFieldGetCall(ctx,
        WithinFile(CreateRefExpr(*jniEnvVar), &curFile), jobjectInstance, field, ty);

    // Assume that CreateJavaFieldGetCall always returns block.
    auto block = StaticAs<ASTKind::BLOCK>(fieldGetCall.get());
    block->body.insert(block->body.begin(), std::move(jniEnvVar));
    return fieldGetCall;
}

OwnedPtr<AST::Expr> ASTFactory::CreateJavaFieldSetCall(AfterTypeCheckContext& ctx,
    Ptr<AST::Expr> jniEnvPtr, Ptr<Expr> jobjectInstance,
    JavaMemberSignature field, AST::Ty& ty, OwnedPtr<AST::Expr> value) const
{
    CJC_ASSERT(field.IsField());
    CJC_ASSERT(field.IsStatic() || jobjectInstance); // instance cannot be null if field is non-static.
    std::vector<OwnedPtr<Node>> callNodes;
    
    auto jclass = jclassCache.CreateJClassAccess(ctx, field.GetClassSignature(), jniEnvPtr);
    auto jfield = jfieldIdCache.CreateJFieldIdAccess(ctx, field, ASTCloner::Clone(jclass.get()), jniEnvPtr);
    
    // 1. let callRes: retTy
    auto& callResVar = StaticCast<VarDecl&>(*callNodes.emplace_back(CreateTmpVarDecl(nullptr, nullptr)));
    callResVar.SetTy(&ty);
    auto jniFieldSetCall = field.IsStatic()
        ? jni.CreateSetStaticJavaFieldCall(ty, jniEnvPtr, jclass, jfield, ilib.CreateJValueExpr(std::move(value)))
        : jni.CreateSetInstanceJavaFieldCall(ty, jniEnvPtr, jobjectInstance, jfield,
            ilib.CreateJValueExpr(std::move(value)));
    callResVar.initializer = std::move(jniFieldSetCall);
    
    return CreateBlockWithJniHandlePendingException(jniEnvPtr, std::move(callNodes));
}

OwnedPtr<AST::Expr> ASTFactory::CreateJavaFieldSetCall(AfterTypeCheckContext& ctx,
    AST::File& curFile, Ptr<Expr> jobjectInstance,
    JavaMemberSignature field, AST::Ty& ty, OwnedPtr<AST::Expr> value) const
{
    static auto jniEnvPtrDecl = ilib.GetJniEnvPtrDecl();
    auto jniEnvVar = CreateTmpVarDecl(jniEnvPtrDecl->type, ilib.CreateGetJniEnvCall(&curFile));

    auto fieldGetCall = CreateJavaFieldSetCall(ctx,
        WithinFile(CreateRefExpr(*jniEnvVar), &curFile), jobjectInstance, field, ty, std::move(value));

    // Assume that CreateJavaFieldSetCall always returns block.
    auto block = StaticAs<ASTKind::BLOCK>(fieldGetCall.get());
    block->body.insert(block->body.begin(), std::move(jniEnvVar));
    return fieldGetCall;
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

std::vector<OwnedPtr<Expr>> ASTFactory::ExtractArgExprs(const std::vector<OwnedPtr<FuncArg>>& args) const
{
    std::vector<OwnedPtr<Expr>> argExprs;
    for (auto& arg : args) {
        auto argExpr = ASTCloner::Clone(arg->expr.get());
        CJC_NULLPTR_CHECK(argExpr);
        argExprs.push_back(std::move(argExpr));
    }
    return argExprs;
}

OwnedPtr<AST::Expr> ASTFactory::CreateBlockWithJniHandlePendingException(Ptr<Expr> jniEnvPtr,
    std::vector<OwnedPtr<Node>> nodes) const
{
    CJC_ASSERT(!nodes.empty()); // Expected at least 1 node - last node should be an expression.
    auto& lastNode = nodes.back();
    CJC_ASSERT(lastNode->IsExpr());
    auto& expr = StaticCast<Expr&>(*lastNode);
    CJC_NULLPTR_CHECK(expr.GetTy());
    
    auto block = CreateBlock(std::move(nodes));
    auto resVar = CreateTmpVarDecl(nullptr, nullptr);
    resVar->SetTy(expr.GetTy());
    block->SetTy(expr.GetTy());
    resVar->initializer = OwnedPtr<Expr>(StaticCast<Expr>(lastNode.release()));
    
    auto& resVarRef = *resVar;
    lastNode = std::move(resVar);
    CJC_ASSERT(lastNode->astKind == ASTKind::VAR_DECL);
    
    // 3. handlePendingException()
    block->body.push_back(ilib.CreateJNIHandlePendingExceptionCall(jniEnvPtr));
    // 4. return resVar
    block->body.push_back(WithinFile(CreateRefExpr(resVarRef), jniEnvPtr->curFile));
    return block;
}

OwnedPtr<AST::Expr> ASTFactory::CreateBlockWithJniHandlePendingException(Ptr<Expr> jniEnvPtr,
    std::function<void(AST::Block&)> builder) const
{
    auto block = CreateBlock({});
    builder(*block);
    CJC_ASSERT(!block->body.empty()); // Expected at least 1 node - last node should be an expression.
    auto& lastNode = block->body.back();
    CJC_ASSERT(lastNode->IsExpr());
    auto& expr = StaticCast<Expr&>(*lastNode);
    CJC_NULLPTR_CHECK(expr.GetTy());
    auto resVar = CreateTmpVarDecl(nullptr, nullptr);
    resVar->SetTy(expr.GetTy());
    block->SetTy(expr.GetTy());
    resVar->initializer = OwnedPtr<Expr>(StaticCast<Expr>(lastNode.release()));
    
    auto& resVarRef = *resVar;
    lastNode = std::move(resVar);
    CJC_ASSERT(lastNode->astKind == ASTKind::VAR_DECL);
    
    // 3. handlePendingException()
    block->body.push_back(ilib.CreateJNIHandlePendingExceptionCall(jniEnvPtr));
    // 4. return resVar
    block->body.push_back(WithinFile(CreateRefExpr(resVarRef), jniEnvPtr->curFile));
    return block;
}

}
