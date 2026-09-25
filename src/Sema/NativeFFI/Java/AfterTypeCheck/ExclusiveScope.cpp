// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "ExclusiveScope.h"
#include "cangjie/AST/Create.h"
#include "cangjie/AST/Types.h"
#include "cangjie/AST/Utils.h"
#include "TypeCheckUtil.h"
#include "NativeFFI/Utils.h"
#include "InteropLibBridge.h"

namespace Cangjie::Native::FFI::Java {

using namespace AST;
using InteropLibBridge = Cangjie::Interop::Java::InteropLibBridge;

OwnedPtr<Expr> WrapWithExclusiveScopeBody(AfterTypeCheckContext& ctx, OwnedPtr<Block> body, Ptr<Ty> retTy)
{
    auto exclusiveDecl = ctx.importManager.GetCoreDecl<FuncDecl>("exclusiveScope");
    CJC_NULLPTR_CHECK(exclusiveDecl);
    auto exclusiveRef = CreateRefExpr(*exclusiveDecl);

    exclusiveRef->instTys.emplace_back(retTy);
    auto mapping = TypeCheckUtil::GenerateTypeMapping(*exclusiveDecl, exclusiveRef->instTys);
    exclusiveRef->SetTy(ctx.typeManager.GetInstantiatedTy(exclusiveDecl->GetTy(), mapping));
    exclusiveRef->typeArguments.emplace_back(CreateType(retTy));

    CJC_NULLPTR_CHECK(body);
    auto curFile = body->curFile;

    auto paramLists = Nodes<FuncParamList>(CreateFuncParamList(Nodes<FuncParam>()));
    auto retTypeNode = CreateType(retTy);
    auto lambdaFuncBody = CreateFuncBody(std::move(paramLists), std::move(retTypeNode), std::move(body), retTy);

    auto lambda = CreateLambdaExpr(std::move(lambdaFuncBody));
    lambda->curFile = curFile;
    lambda->SetTy(ctx.typeManager.GetFunctionTy({}, retTy));

    auto args = Nodes<FuncArg>(CreateFuncArg(std::move(lambda)));
    auto realRetTy = StaticCast<FuncTy>(exclusiveRef->GetTy())->retTy;

    return CreateCallExpr(std::move(exclusiveRef), std::move(args), exclusiveDecl,
        realRetTy, CallKind::CALL_DECLARED_FUNCTION);
}

OwnedPtr<Expr> WrapWithExclusiveScopeExceptionHandling(AfterTypeCheckContext& ctx, InteropLibBridge& lib,
    OwnedPtr<Expr> exclusiveCall, Ptr<Ty> retTy)
{
    CJC_NULLPTR_CHECK(exclusiveCall);
    auto exceptionHandlingDecl = lib.GetWithExclusiveScopeExceptionHandlingDecl();
    CJC_NULLPTR_CHECK(exceptionHandlingDecl);
    auto exceptionHandlingRef = CreateRefExpr(*exceptionHandlingDecl);
    exceptionHandlingRef->instTys.emplace_back(retTy);
    auto exceptionHandlingMapping = TypeCheckUtil::GenerateTypeMapping(*exceptionHandlingDecl,
        exceptionHandlingRef->instTys);
    exceptionHandlingRef->SetTy(ctx.typeManager.GetInstantiatedTy(exceptionHandlingDecl->GetTy(),
        exceptionHandlingMapping));
    exceptionHandlingRef->typeArguments.emplace_back(CreateType(retTy));
    auto curFile = exclusiveCall->curFile;
    auto handlingLambda = WrapReturningLambdaExpr(ctx.typeManager, Nodes(std::move(exclusiveCall)));
    handlingLambda->curFile = curFile;
    auto handlingArgs = Nodes<FuncArg>(CreateFuncArg(std::move(handlingLambda)));
    auto handlingRetTy = StaticCast<FuncTy>(exceptionHandlingRef->GetTy())->retTy;
    return CreateCallExpr(std::move(exceptionHandlingRef), std::move(handlingArgs),
        exceptionHandlingDecl, handlingRetTy, CallKind::CALL_DECLARED_FUNCTION);
}

OwnedPtr<Expr> WrapWithExclusiveScope(AfterTypeCheckContext& ctx, InteropLibBridge& lib,
    OwnedPtr<Block> body, Ptr<Ty> retTy)
{
    auto exclusiveCall = WrapWithExclusiveScopeBody(ctx, std::move(body), retTy);
    return WrapWithExclusiveScopeExceptionHandling(ctx, lib, std::move(exclusiveCall), retTy);
}

} // namespace Cangjie::Native::FFI::Java
