// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "ASTFactory.h"
#include "NativeFFI/Java/AfterTypeCheck/AfterTypeCheckContext.h"
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

OwnedPtr<AST::Expr> ASTFactory::CreateNewJavaObjectCall(
    AfterTypeCheckContext& ctx,
    Ptr<Expr> jniEnvPtr, Ty& javaClassTy, FuncDecl& constructor, std::vector<OwnedPtr<Expr>> args) const
{
    CJC_NULLPTR_CHECK(jniEnvPtr->curFile);
#ifdef NDEBUG
    for (auto& arg: args) {
        CJC_ASSERT(ilib.IsJavaEntityTy(arg->GetTy()));
    }
#endif // NDEBUG
    auto curFile = jniEnvPtr->curFile;

    auto jclass = jclassCache.CreateJClassAccess(ctx, javaClassTy, jniEnvPtr);
    auto jmethod = jmethodIdCache.CreateJMethodIdAccess(ctx, constructor, ASTCloner::Clone(jclass.get()), jniEnvPtr);

    // NOTE: user-defined java-impl constructor requires an extra
    if (args.empty() && !IsUserDefinedJavaImplConstructor(constructor)) {
        // call without arguments, no need to pass empty value.
        return ilib.CreateJavaEntityJobjectCall(jni.interface.CreateNewObjectCall(jniEnvPtr,
            ASTCloner::Clone(jclass.get()), ASTCloner::Clone(jmethod.get())));
    }

    // call with arguments - arguments are constructed as JValue-s within VArray and passed as in-out value into JNI.
    auto block = CreateBlock({}, ilib.GetJavaEntityTy());

    auto jniArgs = CreateTmpVArrayVarDecl(ConvertToJValues(std::move(args)), *curFile);

    jni.interface.CreateNewObjectACall(jniEnvPtr, ASTCloner::Clone(jclass.get()), ASTCloner::Clone(jmethod.get()),
        )


    // TODO:...


    // auto newObjectVar = GetJNINativeInterfaceField(argsExpr ? "NewObjectA" : "NewObject");
    // if (!newObjectVar) {
        // return nullptr;
    // }
    // auto envCJ = CreateJNIEnvReadCall(ASTCloner::Clone(env));
    // if (!envCJ) {
        // return nullptr;
    // }
    // envCJ->curFile = pcurFile;

    // auto newObjectAccess = WithinFile(CreateMemberAccess(std::move(envCJ), *newObjectVar), pcurFile);
    // newObjectAccess->EnableAttr(Attribute::UNSAFE);
    // std::vector<OwnedPtr<FuncArg>> callArgs;
    // callArgs.emplace_back(CreateFuncArg(ASTCloner::Clone(env)));
    // callArgs.emplace_back(CreateFuncArg(std::move(javaClass)));
    // callArgs.emplace_back(CreateFuncArg(std::move(methodId)));
    // if (argsExpr) {
        callArgs.emplace_back(PrepareJNIArgsVArray(std::move(argsExpr)));
    // }
    // auto funcTy = StaticCast<FuncTy*>(newObjectVar->GetTy());
    // auto callExpr = CreateCallExpr(std::move(newObjectAccess), std::move(callArgs), nullptr,
        // funcTy->retTy, CallKind::CALL_FUNCTION_PTR);
    // callExpr->EnableAttr(Attribute::UNSAFE);
    // return CreateJavaEntityJobjectCall(std::move(callExpr));
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

}
