// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements factory class for creating AST nodes.
 */

#include "ASTFactory.h"
#include "ASTQuery.h"
#include "Desugar/AfterTypeCheck.h"
#include "NativeFFI/Utils.h"
#include "TypeCheckUtil.h"
#include "TypeMapper.h"
#include "cangjie/AST/ASTCasting.h"
#include "cangjie/AST/AttributePack.h"
#include "cangjie/AST/Create.h"
#include "cangjie/AST/Node.h"
#include "cangjie/AST/Types.h"
#include "cangjie/AST/Walker.h"
#include "cangjie/Sema/TypeManager.h"
#include "cangjie/Utils/CheckUtils.h"
#include "cangjie/Utils/ConstantsUtils.h"
#include "cangjie/Utils/SafePointer.h"
#include <vector>

using namespace Cangjie::AST;
using namespace Cangjie::TypeCheckUtil;
using namespace Cangjie::Interop::ObjC;
using namespace Cangjie::Native::FFI;

namespace {

constexpr auto VALUE_IDENT = "value";
constexpr auto FINALIZER_IDENT = "~init";
/**
 * Number of leading params (`$registryId`, `$nativeHandle`) that every non-static @ObjCImpl
 * member wrapper carries to restore the Cangjie instance; they are not forwarded to the member.
 */
constexpr size_t SELF_INFO_PARAMS_COUNT = 2;

} // namespace

std::vector<OwnedPtr<FuncParamList>> ASTFactory::CreateParamLists(std::vector<OwnedPtr<FuncParam>>&& params)
{
    std::vector<OwnedPtr<FuncParamList>> paramLists;
    paramLists.emplace_back(CreateFuncParamList(std::move(params)));
    return paramLists;
}

std::vector<OwnedPtr<FuncParam>>& ASTFactory::GetParams(const FuncDecl& fn)
{
    auto& fnBody = fn.funcBody;
    CJC_NULLPTR_CHECK(fnBody);
    auto& paramLists = fnBody->paramLists;
    CJC_ASSERT(paramLists.size() == 1);
    return paramLists[0]->params;
}

OwnedPtr<VarDecl> ASTFactory::CreateVar(const std::string& name, ModalTy ty, bool isVar, OwnedPtr<Expr> initializer)
{
    auto ret = MakeOwned<VarDecl>();
    ret->identifier = name;
    ret->SetTy(ty);
    ret->isVar = isVar;
    ret->EnableAttr(Attribute::COMPILER_ADD, Attribute::IMPLICIT_ADD, Attribute::NO_REFLECT_INFO);
    if (initializer) {
        ret->initializer = std::move(initializer);
    }
    ret->toBeCompiled = true;
    return ret;
}

OwnedPtr<FuncDecl> ASTFactory::CreateFunc(const std::string& name, Ptr<FuncTy> fnTy,
    std::vector<OwnedPtr<FuncParam>>&& params, std::vector<OwnedPtr<Node>>&& nodes)
{
    auto retTy = fnTy->retTy;
    auto ty = nodes.empty() ? retTy : nodes.back()->GetTy();
    auto body = Cangjie::AST::CreateBlock(std::move(nodes), ty);
<<<<<<< HEAD
    auto fnBody =
        Cangjie::AST::CreateFuncBody(CreateParamLists(std::move(params)), CreateType(retTy), std::move(body), retTy);
    auto fn = Cangjie::AST::CreateFuncDecl(name, std::move(fnBody), fnTy);
||||||| constructed merge base
    auto fnBody = Cangjie::AST::CreateFuncBody(CreateParamLists(std::move(params)), CreateType(retTy), std::move(body), retTy);
    auto fn = Cangjie::AST::CreateFuncDecl(name, std::move(fnBody), fnTy);
=======
    auto fnBody = Cangjie::AST::CreateFuncBody(
        CreateParamLists(std::move(params)), CreateType(retTy.Ty()), std::move(body), retTy);
    auto fn = Cangjie::AST::CreateFuncDecl(name, std::move(fnBody), {fnTy});
>>>>>>> feat: local mode
    fn->funcBody->funcDecl = fn.get();
    return fn;
}

OwnedPtr<ParenExpr> ASTFactory::CreateParenExpr(OwnedPtr<Expr> expr)
{
    auto parenExpr = MakeOwnedNode<ParenExpr>();
    parenExpr->SetTy(expr->GetTy());
    parenExpr->expr = std::move(expr);
    return std::move(parenExpr);
}

OwnedPtr<Expr> ASTFactory::CreateNativeHandleExpr(OwnedPtr<Expr> entity)
{
<<<<<<< HEAD
    CJC_ASSERT(IsObjCForeign(*entity->GetTy()) || IsObjCExport(*entity->GetTy()) ||
        IsObjCForeignInterfaceHandleWrapper(*entity->GetTy()));
    auto entityTy = StaticCast<ClassLikeTy>(entity->GetTy());
||||||| constructed merge base
    CJC_ASSERT(typeMapper.IsObjCMirror(*entity->GetTy()) || typeMapper.IsObjCImpl(*entity->GetTy()) ||
        typeMapper.IsSyntheticWrapper(*entity->GetTy()));
    auto entityTy = StaticCast<ClassLikeTy>(entity->GetTy());
=======
    CJC_ASSERT(typeMapper.IsObjCMirror(*entity->GetTy()) || typeMapper.IsObjCImpl(*entity->GetTy()) ||
        typeMapper.IsSyntheticWrapper(*entity->GetTy()));
    auto entityTy = StaticCast<ClassLikeTy>(entity->DataTy());
>>>>>>> feat: local mode
    auto curFile = entity->curFile;
    auto getter = GetNativeHandleGetter(*entityTy->commonDecl);
    CJC_ASSERT_WITH_MSG(getter, "expected the `$obj` getter in the root of the @ObjCMirror hierarchy");
    auto getterMemberAccess = WithinFile(CreateMemberAccess(std::move(entity), *getter), curFile);
    auto getterCall = CreateCallExpr(std::move(getterMemberAccess), {}, getter,
        StaticCast<FuncTy>(getter->DataTy())->retTy, CallKind::CALL_DECLARED_FUNCTION);
    return WithinFile(std::move(getterCall), curFile);
}

OwnedPtr<Expr> ASTFactory::CreateNativeHandleExpr(ClassLikeTy& ty, Ptr<File> curFile)
{
    CJC_ASSERT(IsObjCForeign(ty) || IsObjCExport(ty) || IsObjCForeignInterfaceHandleWrapper(ty));
    auto thisRef = CreateThisRef(ty.commonDecl, &ty, curFile);

    return CreateNativeHandleExpr(std::move(thisRef));
}

OwnedPtr<Expr> ASTFactory::CreateNativeHandleExpr(ClassLikeDecl& decl, bool isStatic, Ptr<File> curFile)
{
    if (!isStatic) {
        auto& ty = *StaticCast<ClassLikeTy>(decl.DataTy());

        return CreateNativeHandleExpr(ty, curFile);
    }

    return CreateGetClassCall(decl, curFile);
}

OwnedPtr<Expr> ASTFactory::UnwrapEntity(OwnedPtr<Expr> expr)
{
    if (IsObjCForeign(*expr->GetTy()) || IsObjCExport(*expr->GetTy())) {
        return CreateNativeHandleExpr(std::move(expr));
    }

    if (expr->GetTy()->IsCoreOptionType()) {
<<<<<<< HEAD
        auto innerTy = expr->GetTy()->typeArgs[0];
        if (IsObjCForeign(*innerTy) || IsObjCExport(*innerTy)) {
||||||| constructed merge base
        auto innerTy = expr->GetTy()->typeArgs[0];
        if (typeMapper.IsValidObjCMirror(*innerTy) || typeMapper.IsObjCImpl(*innerTy)) {
=======
        auto innerTy = expr->DataTy()->TyArg(0);
        if (typeMapper.IsValidObjCMirror(*innerTy) || typeMapper.IsObjCImpl(*innerTy)) {
>>>>>>> feat: local mode
            return UnwrapObjCMirrorOption(std::move(expr), innerTy);
        }
    }
<<<<<<< HEAD
    if (IsObjCPointer(*expr->GetTy())) {
        CJC_ASSERT(expr->GetTy()->typeArgs.size() == 1);
        auto elementType = expr->GetTy()->typeArgs[0];
||||||| constructed merge base
    if (typeMapper.IsObjCPointer(*expr->GetTy())) {
        CJC_ASSERT(expr->GetTy()->typeArgs.size() == 1);
        auto elementType = expr->GetTy()->typeArgs[0];
=======
    if (typeMapper.IsObjCPointer(*expr->GetTy())) {
        CJC_ASSERT(expr->DataTy()->TyArgs().size() == 1);
        auto elementType = expr->DataTy()->TyArg(0);
>>>>>>> feat: local mode
        auto field = bridge.GetObjCPointerPointerField();
        auto fieldRef = CreateRefExpr(*field, *expr);
        return CreateUnsafePointerCast(CreateMemberAccess(std::move(expr), *field), typeMapper.Cj2CType(elementType));
    }
<<<<<<< HEAD
    if (IsObjCFunc(*expr->GetTy())) {
        CJC_ASSERT(expr->GetTy()->typeArgs.size() == 1);
        auto mappedTy = typeMapper.Cj2CType(expr->GetTy());
||||||| constructed merge base
    if (typeMapper.IsObjCFunc(*expr->GetTy())) {
        CJC_ASSERT(expr->GetTy()->typeArgs.size() == 1);
        auto mappedTy = typeMapper.Cj2CType(expr->GetTy());
=======
    if (typeMapper.IsObjCFunc(*expr->GetTy())) {
        CJC_ASSERT(expr->DataTy()->TyArgs().size() == 1);
        auto mappedTy = typeMapper.Cj2CType(expr->DataTy());
>>>>>>> feat: local mode
        auto getter = bridge.GetObjCFuncFPointerAccessor();
        CJC_ASSERT(mappedTy->IsPointer());
<<<<<<< HEAD
        return CreateUnsafePointerCast(CreateMemberCall(std::move(expr), getter), mappedTy->typeArgs[0]);
||||||| constructed merge base
        return CreateUnsafePointerCast(
            CreateMemberCall(std::move(expr), getter),
            mappedTy->typeArgs[0]
        );
=======
        return CreateUnsafePointerCast(CreateMemberCall(std::move(expr), getter), mappedTy->TyArg(0));
>>>>>>> feat: local mode
    }
<<<<<<< HEAD
    if (IsObjCBlock(*expr->GetTy())) {
        CJC_ASSERT(expr->GetTy()->typeArgs.size() == 1);
        auto mappedTy = typeMapper.Cj2CType(expr->GetTy());
||||||| constructed merge base
    if (typeMapper.IsObjCBlock(*expr->GetTy())) {
        CJC_ASSERT(expr->GetTy()->typeArgs.size() == 1);
        auto mappedTy = typeMapper.Cj2CType(expr->GetTy());
=======
    if (typeMapper.IsObjCBlock(*expr->GetTy())) {
        CJC_ASSERT(expr->DataTy()->TyArgs().size() == 1);
        auto mappedTy = typeMapper.Cj2CType(expr->DataTy());
>>>>>>> feat: local mode
        auto abiPtr = bridge.GetObjCBlockAbiPointerAccessor();
        CJC_ASSERT(mappedTy->IsPointer());
<<<<<<< HEAD
        return CreateUnsafePointerCast(CreateMemberCall(std::move(expr), abiPtr), mappedTy->typeArgs[0]);
||||||| constructed merge base
        return CreateUnsafePointerCast(
            CreateMemberCall(std::move(expr), abiPtr),
            mappedTy->typeArgs[0]
        );
=======
        return CreateUnsafePointerCast(CreateMemberCall(std::move(expr), abiPtr), mappedTy->TyArg(0));
>>>>>>> feat: local mode
    }

    CJC_ASSERT(expr->GetTy()->IsPrimitive() || expr->GetTy()->IsPointer() || expr->GetTy()->IsCStructType() ||
        expr->GetTy()->IsCFunc() || expr->GetTy()->IsCString());
    return expr;
}

OwnedPtr<Expr> ASTFactory::WrapEntity(OwnedPtr<Expr> expr, Ty& wrapTy, Retain withRetain)
{
    if (IsObjCForeign(wrapTy)) {
        CJC_ASSERT(expr->GetTy()->IsPointer());
        auto curFile = expr->curFile;
        auto classLikeTy = StaticCast<ClassLikeTy>(&wrapTy);
        auto targetClass = As<ASTKind::CLASS_DECL>(classLikeTy->commonDecl);
        if (!targetClass) {
            const auto interfaceDecl = As<ASTKind::INTERFACE_DECL>(classLikeTy->commonDecl);
            CJC_NULLPTR_CHECK(interfaceDecl);
            targetClass = GetObjCMirrorInterfaceHandleWrapper(importManager, *interfaceDecl);
        }

<<<<<<< HEAD
        auto ctor = GetBaseCtor(*targetClass);
        expr = ApplyRetain(std::move(expr), withRetain);
        auto ctorCall = CreateCallExpr(CreateRefExpr(*ctor), Nodes<FuncArg>(CreateFuncArg(std::move(expr))), ctor,
            classLikeTy, CallKind::CALL_OBJECT_CREATION);
        AppendNativeObjCIdMarkerIfNeeded(*ctorCall, curFile);
        return ctorCall;
||||||| constructed merge base
        auto ctor = GetGeneratedBaseCtor(*mirror);
        switch (withRetain) {
            case Retain::RETAINED:
                expr = CreateObjCRetainCall(std::move(expr));
                break;
            case Retain::RETAIN_AUTORELEASED_RETURN_VALUE:
                expr = CreateObjCRetainAutoreleasedReturnValueCall(std::move(expr));
                break;
            case Retain::UNRETAINED:
            default:
                break;
        }
        return CreateCallExpr(CreateRefExpr(*ctor), Nodes<FuncArg>(CreateFuncArg(std::move(expr))), ctor, classLikeTy,
            CallKind::CALL_OBJECT_CREATION);
=======
        auto ctor = GetGeneratedBaseCtor(*mirror);
        switch (withRetain) {
            case Retain::RETAINED:
                expr = CreateObjCRetainCall(std::move(expr));
                break;
            case Retain::RETAIN_AUTORELEASED_RETURN_VALUE:
                expr = CreateObjCRetainAutoreleasedReturnValueCall(std::move(expr));
                break;
            case Retain::UNRETAINED:
            default:
                break;
        }
        return CreateCallExpr(CreateRefExpr(*ctor), Nodes<FuncArg>(CreateFuncArg(std::move(expr))), ctor, {classLikeTy},
            CallKind::CALL_OBJECT_CREATION);
>>>>>>> feat: local mode
    }

<<<<<<< HEAD
    if (IsObjCExport(wrapTy)) {
        CJC_ASSERT(expr->GetTy()->IsPointer());
        // The handle is bound to a temporary first, so that the registry lookup and the ctor call share a
        // single retain - hence Retain::UNRETAINED below.
        expr = ApplyRetain(std::move(expr), withRetain);
        auto curFile = expr->curFile;

        std::vector<OwnedPtr<Node>> wrapNodes;
        auto exprVd = CreateTmpVarDecl(nullptr, std::move(expr));
        auto exprRef = WithinFile(CreateRefExpr(*exprVd), curFile);
        wrapNodes.push_back(std::move(exprVd));
        auto registryId = CreateGetRegistryIdCall(ASTCloner::Clone(exprRef.get()));
        auto& impl = *StaticCast<ClassTy>(&wrapTy)->decl;

        wrapNodes.push_back(
            CreateObjCImplBaseCtorCall(impl, std::move(exprRef), std::move(registryId), curFile, Retain::UNRETAINED));
        return WrapReturningLambdaCall(typeManager, std::move(wrapNodes));
||||||| constructed merge base
    if (typeMapper.IsObjCImpl(wrapTy)) {
        CJC_ASSERT(expr->GetTy()->IsPointer());
        auto& classTy = *StaticCast<ClassTy>(&wrapTy);
        auto& impl = *classTy.commonDecl;
        return CreateGetFromRegistryByNativeHandleCall(std::move(expr), CreateRefType(impl));
=======
    if (typeMapper.IsObjCImpl(wrapTy)) {
        CJC_ASSERT(expr->DataTy()->IsPointer());
        auto& classTy = *StaticCast<ClassTy>(&wrapTy);
        auto& impl = *classTy.commonDecl;
        return CreateGetFromRegistryByNativeHandleCall(std::move(expr), CreateRefType(impl));
>>>>>>> feat: local mode
    }

<<<<<<< HEAD
    if (IsObjCPointer(wrapTy)) {
        CJC_ASSERT(expr->GetTy()->IsPointer());
        CJC_ASSERT(wrapTy.typeArgs.size() == 1);
||||||| constructed merge base
    if (typeMapper.IsObjCPointer(wrapTy)) {
        CJC_ASSERT(expr->GetTy()->IsPointer());
        CJC_ASSERT(wrapTy.typeArgs.size() == 1);
=======
    if (typeMapper.IsObjCPointer(wrapTy)) {
        CJC_ASSERT(expr->DataTy()->IsPointer());
        CJC_ASSERT(wrapTy.TyArgs().size() == 1);
>>>>>>> feat: local mode
        auto ctor = bridge.GetObjCPointerConstructor();
        CJC_ASSERT(ctor);
        auto ctorRef = CreateRefExpr(*ctor, *expr);
<<<<<<< HEAD
        ctorRef->instTys.push_back(wrapTy.typeArgs[0]);
        ctorRef->typeArguments.push_back(CreateType(wrapTy.typeArgs[0]));
        auto unitPtrExpr = CreateUnsafePointerCast(std::move(expr), typeManager.GetPrimitiveTy(TypeKind::TYPE_UNIT));
        return CreateCallExpr(std::move(ctorRef), Nodes<FuncArg>(CreateFuncArg(std::move(unitPtrExpr))), ctor, &wrapTy,
            CallKind::CALL_STRUCT_CREATION);
||||||| constructed merge base
        ctorRef->instTys.push_back(wrapTy.typeArgs[0]);
        ctorRef->typeArguments.push_back(CreateType(wrapTy.typeArgs[0]));
        auto unitPtrExpr = CreateUnsafePointerCast(
            std::move(expr),
            typeManager.GetPrimitiveTy(TypeKind::TYPE_UNIT));
        return CreateCallExpr(std::move(ctorRef), Nodes<FuncArg>(CreateFuncArg(std::move(unitPtrExpr))), ctor, &wrapTy,
            CallKind::CALL_STRUCT_CREATION);
=======
        ctorRef->instTys.push_back(wrapTy.TyArg(0));
        ctorRef->typeArguments.push_back(CreateType(wrapTy.TyArg(0)));
        auto unitPtrExpr = CreateUnsafePointerCast(
            std::move(expr),
            typeManager.GetPrimitiveTy(TypeKind::TYPE_UNIT));
        return CreateCallExpr(std::move(ctorRef), Nodes<FuncArg>(CreateFuncArg(std::move(unitPtrExpr))), ctor,
            {&wrapTy}, CallKind::CALL_STRUCT_CREATION);
>>>>>>> feat: local mode
    }

<<<<<<< HEAD
    if (IsObjCFunc(wrapTy)) {
        CJC_ASSERT(expr->GetTy()->IsPointer());
        CJC_ASSERT(wrapTy.typeArgs.size() == 1);
||||||| constructed merge base
    if (typeMapper.IsObjCFunc(wrapTy)) {
        CJC_ASSERT(expr->GetTy()->IsPointer());
        CJC_ASSERT(wrapTy.typeArgs.size() == 1);
=======
    if (typeMapper.IsObjCFunc(wrapTy)) {
        CJC_ASSERT(expr->DataTy()->IsPointer());
        CJC_ASSERT(wrapTy.TyArgs().size() == 1);
>>>>>>> feat: local mode
        auto ctor = bridge.GetObjCFuncConstructor();
        CJC_ASSERT(ctor);
        auto ctorRef = CreateRefExpr(*ctor, *expr);
<<<<<<< HEAD
        ctorRef->instTys.push_back(wrapTy.typeArgs[0]);
        ctorRef->typeArguments.push_back(CreateType(wrapTy.typeArgs[0]));
        auto unitPtrExpr = CreateUnsafePointerCast(std::move(expr),
            typeManager.GetFunctionTy({}, typeManager.GetPrimitiveTy(TypeKind::TYPE_UNIT), {.isC = true}));
        return CreateCallExpr(std::move(ctorRef), Nodes<FuncArg>(CreateFuncArg(std::move(unitPtrExpr))), ctor, &wrapTy,
            CallKind::CALL_STRUCT_CREATION);
||||||| constructed merge base
        ctorRef->instTys.push_back(wrapTy.typeArgs[0]);
        ctorRef->typeArguments.push_back(CreateType(wrapTy.typeArgs[0]));
        auto unitPtrExpr = CreateUnsafePointerCast(
            std::move(expr),
            typeManager.GetFunctionTy(
                {},
                typeManager.GetPrimitiveTy(TypeKind::TYPE_UNIT),
                {.isC = true}
            ));
        return CreateCallExpr(std::move(ctorRef), Nodes<FuncArg>(CreateFuncArg(std::move(unitPtrExpr))), ctor, &wrapTy,
            CallKind::CALL_STRUCT_CREATION);
=======
        ctorRef->instTys.push_back(wrapTy.TyArg(0));
        ctorRef->typeArguments.push_back(CreateType(wrapTy.TyArg(0)));
        auto unitPtrExpr = CreateUnsafePointerCast(std::move(expr),
            typeManager.GetFunctionTy({}, {typeManager.GetPrimitiveTy(TypeKind::TYPE_UNIT)}, {.isC = true}));
        return CreateCallExpr(std::move(ctorRef), Nodes<FuncArg>(CreateFuncArg(std::move(unitPtrExpr))), ctor,
            {&wrapTy}, CallKind::CALL_STRUCT_CREATION);
>>>>>>> feat: local mode
    }

<<<<<<< HEAD
    if (IsObjCBlock(wrapTy)) {
        CJC_ASSERT(expr->GetTy()->IsPointer());
        CJC_ASSERT(wrapTy.typeArgs.size() == 1);
||||||| constructed merge base
    if (typeMapper.IsObjCBlock(wrapTy)) {
        CJC_ASSERT(expr->GetTy()->IsPointer());
        CJC_ASSERT(wrapTy.typeArgs.size() == 1);
=======
    if (typeMapper.IsObjCBlock(wrapTy)) {
        CJC_ASSERT(expr->DataTy()->IsPointer());
        CJC_ASSERT(wrapTy.TyArgs().size() == 1);
>>>>>>> feat: local mode
        auto ctor = bridge.GetObjCBlockConstructorFromObjC();
        CJC_ASSERT(ctor);
        auto ctorRef = CreateRefExpr(*ctor, *expr);
        ctorRef->instTys.push_back(wrapTy.TyArg(0));
        ctorRef->typeArguments.push_back(CreateType(wrapTy.TyArg(0)));
        return CreateCallExpr(std::move(ctorRef), Nodes<FuncArg>(CreateFuncArg(std::move(expr))), ctor, {&wrapTy},
            CallKind::CALL_OBJECT_CREATION);
    }

    if (wrapTy.IsCoreOptionType()) {
        if (auto classALTy = DynamicCast<ClassLikeTy>(wrapTy.TyArg(0))) {
            if (auto decl = classALTy->commonDecl;
                decl && decl->TestAnyAttr(Attribute::OBJ_C_FOREIGN, Attribute::OBJ_C_EXPORT)) {
                return WrapObjCMirrorOption(expr, decl, expr->curFile, withRetain);
            }
        }
    }

<<<<<<< HEAD
    CJC_ASSERT(expr->GetTy()->IsPrimitive() || Ty::IsCStructType(*expr->GetTy()) || expr->GetTy()->IsCFunc() ||
        expr->GetTy()->IsCString() || expr->GetTy()->IsPointer());
    CJC_ASSERT(wrapTy.IsPrimitive() || Ty::IsCStructType(wrapTy) || wrapTy.IsCFunc() || wrapTy.IsCString() ||
        wrapTy.IsPointer());
||||||| constructed merge base
    CJC_ASSERT(expr->GetTy()->IsPrimitive() || Ty::IsCStructType(*expr->GetTy()) || expr->GetTy()->IsCFunc() ||
        expr->GetTy()->IsCString());
    CJC_ASSERT(wrapTy.IsPrimitive() || Ty::IsCStructType(wrapTy) || wrapTy.IsCFunc() || wrapTy.IsCString());
=======
    CJC_ASSERT(expr->GetTy()->IsPrimitive() || expr->GetTy()->IsCStructType() || expr->GetTy()->IsCFunc() ||
        expr->GetTy()->IsCString());
    CJC_ASSERT(wrapTy.IsPrimitive() || wrapTy.IsCStructType() || wrapTy.IsCFunc() || wrapTy.IsCString());
>>>>>>> feat: local mode
    return expr;
}

<<<<<<< HEAD
OwnedPtr<Expr> ASTFactory::CreateOptionMatch(OwnedPtr<Expr> selector,
    std::function<OwnedPtr<Expr>(VarDecl&)> someBranch, std::function<OwnedPtr<Expr>()> noneBranch, Ptr<Ty> ty)
||||||| constructed merge base
// OptionType
Ptr<Ty> ASTFactory::GetOptionTy(Ptr<Ty> ty)
{
    return typeManager.GetEnumTy(*GetOptionDecl(), {ty});
}

Ptr<EnumDecl> ASTFactory::GetOptionDecl()
{
    static auto decl = importManager.GetCoreDecl<EnumDecl>(STD_LIB_OPTION);
    return decl;
}

Ptr<Decl> ASTFactory::GetOptionSomeDecl()
{
    static auto someDecl = Sema::Desugar::AfterTypeCheck::LookupEnumMember(GetOptionDecl(), OPTION_VALUE_CTOR);
    return someDecl;
}

Ptr<Decl> ASTFactory::GetOptionNoneDecl()
{
    static auto noneDecl = Sema::Desugar::AfterTypeCheck::LookupEnumMember(GetOptionDecl(), OPTION_NONE_CTOR);
    return noneDecl;
}

OwnedPtr<Expr> ASTFactory::CreateOptionSomeRef(Ptr<Ty> ty)
{
    auto someDeclRef = CreateRefExpr(*GetOptionSomeDecl());
    auto optionActualTy = GetOptionTy(ty);
    someDeclRef->SetTy(typeManager.GetFunctionTy({ty}, optionActualTy));
    return someDeclRef;
}

OwnedPtr<Expr> ASTFactory::CreateOptionNoneRef(Ptr<Ty> ty)
{
    auto noneDeclRef = CreateRefExpr(*GetOptionNoneDecl());
    auto optionActualTy = GetOptionTy(ty);
    noneDeclRef->SetTy(optionActualTy);
    return noneDeclRef;
}

OwnedPtr<Expr> ASTFactory::CreateOptionSomeCall(OwnedPtr<Expr> expr, Ptr<Ty> ty)
{
    std::vector<OwnedPtr<FuncArg>> someDeclCallArgs {};
    someDeclCallArgs.emplace_back(CreateFuncArg(std::move(expr)));
    auto someDeclCall = CreateCallExpr(CreateOptionSomeRef(ty), std::move(someDeclCallArgs));
    someDeclCall->SetTy(GetOptionTy(ty));
    someDeclCall->resolvedFunction = As<ASTKind::FUNC_DECL>(GetOptionSomeDecl());
    someDeclCall->callKind = CallKind::CALL_DECLARED_FUNCTION;
    return someDeclCall;
}

OwnedPtr<Expr> ASTFactory::CreateOptionMatch(
    OwnedPtr<Expr> selector,
    std::function<OwnedPtr<Expr>(VarDecl&)> someBranch,
    std::function<OwnedPtr<Expr>()> noneBranch,
    Ptr<Ty> ty)
=======
// OptionType
Ptr<Ty> ASTFactory::GetOptionTy(Ptr<Ty> ty)
{
    return typeManager.GetEnumTy(*GetOptionDecl(), {ty});
}

Ptr<EnumDecl> ASTFactory::GetOptionDecl()
{
    static auto decl = importManager.GetCoreDecl<EnumDecl>(STD_LIB_OPTION);
    return decl;
}

Ptr<Decl> ASTFactory::GetOptionSomeDecl()
{
    static auto someDecl = Sema::Desugar::AfterTypeCheck::LookupEnumMember(GetOptionDecl(), OPTION_VALUE_CTOR);
    return someDecl;
}

Ptr<Decl> ASTFactory::GetOptionNoneDecl()
{
    static auto noneDecl = Sema::Desugar::AfterTypeCheck::LookupEnumMember(GetOptionDecl(), OPTION_NONE_CTOR);
    return noneDecl;
}

OwnedPtr<Expr> ASTFactory::CreateOptionSomeRef(Ptr<Ty> ty)
{
    auto someDeclRef = CreateRefExpr(*GetOptionSomeDecl());
    auto optionActualTy = GetOptionTy(ty);
    auto someFuncTy = typeManager.GetFunctionTy({{ty}}, {optionActualTy});
    someDeclRef->SetTy({someFuncTy});
    return someDeclRef;
}

OwnedPtr<Expr> ASTFactory::CreateOptionNoneRef(Ptr<Ty> ty)
{
    auto noneDeclRef = CreateRefExpr(*GetOptionNoneDecl());
    auto optionActualTy = GetOptionTy(ty);
    noneDeclRef->SetTy({optionActualTy});
    return noneDeclRef;
}

OwnedPtr<Expr> ASTFactory::CreateOptionSomeCall(OwnedPtr<Expr> expr, Ptr<Ty> ty)
{
    std::vector<OwnedPtr<FuncArg>> someDeclCallArgs {};
    someDeclCallArgs.emplace_back(CreateFuncArg(std::move(expr)));
    auto someDeclCall = CreateCallExpr(CreateOptionSomeRef(ty), std::move(someDeclCallArgs));
    someDeclCall->SetTy(GetOptionTy(ty));
    someDeclCall->resolvedFunction = As<ASTKind::FUNC_DECL>(GetOptionSomeDecl());
    someDeclCall->callKind = CallKind::CALL_DECLARED_FUNCTION;
    return someDeclCall;
}

OwnedPtr<Expr> ASTFactory::CreateOptionMatch(
    OwnedPtr<Expr> selector,
    std::function<OwnedPtr<Expr>(VarDecl&)> someBranch,
    std::function<OwnedPtr<Expr>()> noneBranch,
    Ptr<Ty> ty)
>>>>>>> feat: local mode
{
    auto curFile = selector->curFile;
    CJC_NULLPTR_CHECK(curFile);

    auto& optTy = *selector->GetTy();
    CJC_ASSERT(optTy.IsCoreOptionType());
    auto optArgTy{optTy.TyArg(0)};

    auto vp = CreateVarPattern(Cangjie::V_COMPILER, optArgTy);
    vp->curFile = curFile;
    vp->varDecl->curFile = curFile;
    auto& someArgVar = *vp->varDecl;

    auto somePattern = MakeOwnedNode<EnumPattern>();
    somePattern->SetTy(selector->GetTy());
    somePattern->constructor = CreateOptionSomeRef(importManager, typeManager, optArgTy);
    somePattern->patterns.emplace_back(std::move(vp));
    somePattern->curFile = curFile;
    auto caseSome = CreateMatchCase(std::move(somePattern), someBranch(someArgVar));

    auto nonePattern = MakeOwnedNode<EnumPattern>();
    nonePattern->constructor = CreateOptionNoneRef(importManager, typeManager, optArgTy);
    nonePattern->SetTy(nonePattern->constructor->GetTy());
    nonePattern->curFile = curFile;
    auto caseNone = CreateMatchCase(std::move(nonePattern), noneBranch());

<<<<<<< HEAD
    return WithinFile(
        CreateMatchExpr(std::move(selector), Nodes<MatchCase>(std::move(caseSome), std::move(caseNone)), ty), curFile);
||||||| constructed merge base
    return WithinFile(CreateMatchExpr(
        std::move(selector),
        Nodes<MatchCase>(std::move(caseSome), std::move(caseNone)), ty), curFile);
=======
    return WithinFile(
        CreateMatchExpr(std::move(selector), Nodes<MatchCase>(std::move(caseSome), std::move(caseNone)), {ty}),
        curFile);
>>>>>>> feat: local mode
}

Ptr<Ty> ASTFactory::GetObjCTy()
{
    return typeManager.GetPointerTy(typeManager.GetPrimitiveTy(TypeKind::TYPE_UNIT));
}

OwnedPtr<Expr> ASTFactory::CreateObjCobjectNull()
{
    auto pointerExpr = MakeOwnedNode<PointerExpr>();
    pointerExpr->type = MakeOwnedNode<Type>();
    pointerExpr->type->SetTy(GetObjCTy());
    pointerExpr->SetTy(pointerExpr->type->GetTy());
    return pointerExpr;
}

/*
    UNWRAP [OPTION -> HANDLE]
    match(entity) {
        None -> CPointer()
        Some(t) -> Unwrap(t)
    }
*/
OwnedPtr<Expr> ASTFactory::UnwrapObjCMirrorOption(OwnedPtr<Expr> entity, Ptr<Ty> ty)
{
    auto curFile = entity->curFile;
    CJC_NULLPTR_CHECK(curFile);
    auto actualTy = ty;
    return CreateOptionMatch(
        std::move(entity),
        [this, curFile](VarDecl& e) {
            auto unwrapped = UnwrapEntity(WithinFile(CreateRefExpr(e), curFile));
            return unwrapped;
        },
        [this]() {
            // CPointer<Unit>()
            return CreateObjCobjectNull();
        },
        typeMapper.Cj2CType(actualTy));
}
/*
    WRAP [HADNLE -> OPTION]
    {
    match (handle.isNull()) {
        case false => Some(T(handle)) | Some(getFromRegistry(handle))
        case true => None
    }
    }()
 */
OwnedPtr<Expr> ASTFactory::WrapObjCMirrorOption(
    const Ptr<Expr> entity, Ptr<ClassLikeDecl> mirror, const Ptr<File> curFile, Retain withRetain)
{
    std::vector<OwnedPtr<Node>> nodes;
    auto baseTy = typeManager.GetPointerTy(typeManager.GetPrimitiveTy(TypeKind::TYPE_UNIT));
    auto tmpVar = CreateTmpVarDecl(CreateType({baseTy}), entity);
    CopyBasicInfo(tmpVar->initializer.get(), tmpVar.get());
    tmpVar->begin = entity->begin;
    tmpVar->curFile = curFile;
    auto objcrefExpr = WithinFile(CreateRefExpr(*tmpVar), curFile);

    auto castTy = mirror->GetTy();
    // case true => None
<<<<<<< HEAD
    OwnedPtr<Expr> trueBranch = CreateOptionNoneRef(importManager, typeManager, castTy);
||||||| constructed merge base
    OwnedPtr<Expr> trueBranch = CreateOptionNoneRef(castTy);
=======
    OwnedPtr<Expr> trueBranch = CreateOptionNoneRef(castTy.Ty());
>>>>>>> feat: local mode
    // case false => wrap($tmp, T)
    OwnedPtr<Expr> falseBranch = WrapEntity(std::move(objcrefExpr), *castTy, withRetain);

    auto isInstanceCall = WithinFile(CreateGetObjcEntityOrNullCall(*tmpVar, curFile), curFile);
<<<<<<< HEAD
    auto boolMatch = CreateBoolMatch(std::move(isInstanceCall), std::move(trueBranch), std::move(falseBranch),
        GetOptionTy(importManager, typeManager, castTy));
||||||| constructed merge base
    auto boolMatch = CreateBoolMatch(
        std::move(isInstanceCall), std::move(trueBranch), std::move(falseBranch), GetOptionTy(castTy));
=======
    auto boolMatch = CreateBoolMatch(
        std::move(isInstanceCall), std::move(trueBranch), std::move(falseBranch), GetOptionTy(castTy.Ty()));
>>>>>>> feat: local mode
    boolMatch->begin = entity->begin;
    boolMatch->end = entity->end;
    nodes.push_back(std::move(tmpVar));
    nodes.push_back(std::move(boolMatch));
    return WrapReturningLambdaCall(typeManager, std::move(nodes));
}

OwnedPtr<Expr> ASTFactory::CreateOptionalMethodGuard(
    OwnedPtr<Expr> msgSend, OwnedPtr<Expr> cls, const std::string& selector, const Ptr<File> curFile)
{
    std::vector<OwnedPtr<Node>> nodes;
    auto baseTy = msgSend->GetTy();
    auto selectorCall = CreateGetCachedSelectorAccess(selector, curFile);
    auto isRespondToSelectorCall = CreateObjCRespondsToSelectorCall(std::move(cls), std::move(selectorCall), curFile);

    // case true => return msgSend(...)
    OwnedPtr<Expr> trueBranch = std::move(msgSend);

    // case false => throw Exception(...)
    OwnedPtr<Expr> falseBranch = WithinFile(CreateThrowOptionalMethodUnimplemented(*curFile), curFile);

    auto boolMatch =
        CreateBoolMatch(std::move(isRespondToSelectorCall), std::move(trueBranch), std::move(falseBranch), baseTy.Ty());

    nodes.push_back(std::move(boolMatch));
    return WrapReturningLambdaCall(typeManager, std::move(nodes));
}

<<<<<<< HEAD
||||||| constructed merge base
// Mirror(handle)
OwnedPtr<Expr> ASTFactory::CreateMirrorConstructorCall(OwnedPtr<Expr> entity, Ptr<Ty> mirrorTy)
{
    auto curFile = entity->curFile;
    auto classLikeTy = DynamicCast<ClassLikeTy*>(mirrorTy.get());
    if (!classLikeTy) {
        return nullptr;
    }
    if (auto decl = classLikeTy->commonDecl; decl && decl->TestAttr(Attribute::OBJ_C_MIRROR)) {
        Ptr<ClassDecl> cld;
        if (decl->astKind == ASTKind::CLASS_DECL) {
            cld = StaticAs<ASTKind::CLASS_DECL>(decl);
        } else {
            cld = GetSyntheticWrapper(importManager, *decl);
        }
        CJC_NULLPTR_CHECK(cld);
        auto mirrorCtor = GetGeneratedBaseCtor(*cld);
        CJC_NULLPTR_CHECK(mirrorCtor);

        auto ctorCall = CreateCall(mirrorCtor, curFile, std::move(entity));
        if (ctorCall) {
            ctorCall->callKind = CallKind::CALL_OBJECT_CREATION;
            ctorCall->SetTy(mirrorTy);
        }
        return ctorCall;
    }
    CJC_ABORT();
    return nullptr;
}

=======
// Mirror(handle)
OwnedPtr<Expr> ASTFactory::CreateMirrorConstructorCall(OwnedPtr<Expr> entity, Ptr<Ty> mirrorTy)
{
    auto curFile = entity->curFile;
    auto classLikeTy = DynamicCast<ClassLikeTy*>(mirrorTy.get());
    if (!classLikeTy) {
        return nullptr;
    }
    if (auto decl = classLikeTy->commonDecl; decl && decl->TestAttr(Attribute::OBJ_C_MIRROR)) {
        Ptr<ClassDecl> cld;
        if (decl->astKind == ASTKind::CLASS_DECL) {
            cld = StaticAs<ASTKind::CLASS_DECL>(decl);
        } else {
            cld = GetSyntheticWrapper(importManager, *decl);
        }
        CJC_NULLPTR_CHECK(cld);
        auto mirrorCtor = GetGeneratedBaseCtor(*cld);
        CJC_NULLPTR_CHECK(mirrorCtor);

        auto ctorCall = CreateCall(mirrorCtor, curFile, std::move(entity));
        if (ctorCall) {
            ctorCall->callKind = CallKind::CALL_OBJECT_CREATION;
            ctorCall->SetTy({mirrorTy});
        }
        return ctorCall;
    }
    CJC_ABORT();
    return nullptr;
}

>>>>>>> feat: local mode
// handle.$obj.isNull()
// tmp = handle.$obj
// return tmp.isNull()
<<<<<<< HEAD
OwnedPtr<Expr> ASTFactory::CreateGetObjcEntityOrNullCall(VarDecl& entity, Ptr<File> file)
{
    return CreateIsPtrNullCheckCall(importManager, typeManager, WithinFile(CreateRefExpr(entity), file));
||||||| constructed merge base
OwnedPtr<Expr> ASTFactory::CreateGetObjcEntityOrNullCall(VarDecl &entity, Ptr<File> file)
{
    static auto getPointerAddressFunc = importManager.GetCoreDecl<FuncDecl>(
        GET_POINTER_ADDRESS_INTRINSIC);
    CJC_NULLPTR_CHECK(getPointerAddressFunc);
    auto readPointerRef = WithinFile(CreateRefExpr(*getPointerAddressFunc), file);
    auto uintNativeTy = typeManager.GetPrimitiveTy(TypeKind::TYPE_UINT_NATIVE);
    CJC_ASSERT(entity.GetTy()->IsPointer());
    readPointerRef->instTys.push_back(entity.GetTy()->typeArgs.front());
    readPointerRef->SetTy(
        typeManager.GetFunctionTy(
            std::vector{entity.GetTy()},
            uintNativeTy));
    std::vector<OwnedPtr<FuncArg>> callArgs;
    callArgs.push_back(CreateFuncArg(WithinFile(CreateRefExpr(entity), file)));
    auto getPointerAddressCall = CreateCallExpr(
        std::move(readPointerRef),
        std::move(callArgs),
        getPointerAddressFunc,
        uintNativeTy,
        CallKind::CALL_INTRINSIC_FUNCTION);

    auto result = CreateBinaryExpr(
        std::move(getPointerAddressCall),
        CreateLitConstExpr(LitConstKind::INTEGER, "0", uintNativeTy),
        TokenKind::EQUAL);
    // CreateBinaryExpr sets type to type of left or right operand, which is incorrect for ==
    result->SetTy(typeManager.GetBoolTy());
    return result;
=======
OwnedPtr<Expr> ASTFactory::CreateGetObjcEntityOrNullCall(VarDecl &entity, Ptr<File> file)
{
    static auto getPointerAddressFunc = importManager.GetCoreDecl<FuncDecl>(
        GET_POINTER_ADDRESS_INTRINSIC);
    CJC_NULLPTR_CHECK(getPointerAddressFunc);
    auto readPointerRef = WithinFile(CreateRefExpr(*getPointerAddressFunc), file);
    auto uintNativeTy = typeManager.GetPrimitiveTy(TypeKind::TYPE_UINT_NATIVE);
    CJC_ASSERT(entity.GetTy()->IsPointer());
    readPointerRef->instTys.push_back(entity.GetTy()->typeArgs.front().Ty());
    readPointerRef->SetTy({typeManager.GetFunctionTy(std::vector{entity.GetTy()}, {uintNativeTy})});
    std::vector<OwnedPtr<FuncArg>> callArgs;
    callArgs.push_back(CreateFuncArg(WithinFile(CreateRefExpr(entity), file)));
    auto getPointerAddressCall = CreateCallExpr(std::move(readPointerRef), std::move(callArgs), getPointerAddressFunc,
        {uintNativeTy}, CallKind::CALL_INTRINSIC_FUNCTION);

    auto result = CreateBinaryExpr(std::move(getPointerAddressCall),
        CreateLitConstExpr(LitConstKind::INTEGER, "0", {uintNativeTy}), TokenKind::EQUAL);
    // CreateBinaryExpr sets type to type of left or right operand, which is incorrect for ==
    result->SetTy({typeManager.GetBoolTy()});
    return result;
>>>>>>> feat: local mode
}

OwnedPtr<VarDecl> ASTFactory::CreateNativeHandleField(ClassDecl& target)
{
    auto nativeHandleTy = bridge.GetNativeObjCIdTy();

    auto nativeHandleField = CreateVarDecl(NATIVE_HANDLE_IDENT, nullptr, CreateType(nativeHandleTy));
    nativeHandleField->SetTy({nativeHandleTy});
    // mark it initialized because sema initialization analysis is run before
    // objc desugaring
    nativeHandleField->EnableAttr(Attribute::PUBLIC, Attribute::INITIALIZED);

    PutDeclToClassLikeBody(*nativeHandleField, target);

    return nativeHandleField;
}

OwnedPtr<FuncDecl> ASTFactory::CreateGetObjCClassDecl(ClassLikeDecl& target)
{
    auto nativeObjCClassTy = bridge.GetNativeObjCClassTy();

    auto curFile = target.curFile;
    auto paramList = MakeOwned<FuncParamList>();
    std::vector<OwnedPtr<FuncParamList>> wrapperParamLists;
    wrapperParamLists.push_back(std::move(paramList));
    auto getClassCall = CreateGetCachedClassAccess(*StaticCast<ClassLikeTy>(target.DataTy()), curFile);
    auto wrapperFuncTy = typeManager.GetFunctionTy({}, {nativeObjCClassTy});
    auto wrapperBody = CreateFuncBody(std::move(wrapperParamLists), CreateType(nativeObjCClassTy),
        CreateBlock({}, {nativeObjCClassTy}), {wrapperFuncTy});

<<<<<<< HEAD
    auto classDecl =
        CreateFuncDecl(GET_OBJ_C_CLASS_IDENT, std::move(wrapperBody), typeManager.GetFunctionTy({}, nativeObjCClassTy));
||||||| constructed merge base
    auto classDecl = CreateFuncDecl(GET_OBJ_C_CLASS_IDENT, std::move(wrapperBody),
        typeManager.GetFunctionTy({}, nativeObjCClassTy));
=======
    auto classDecl = CreateFuncDecl(GET_OBJ_C_CLASS_IDENT, std::move(wrapperBody), {wrapperFuncTy});
>>>>>>> feat: local mode
    classDecl->moduleName = target.moduleName;
    classDecl->fullPackageName = target.fullPackageName;
    classDecl->EnableAttr(Attribute::PUBLIC, Attribute::STATIC, Attribute::IS_CHECK_VISITED);
    classDecl->funcBody->funcDecl = classDecl.get();
    classDecl->linkage = Linkage::EXTERNAL;
    classDecl->funcBody->parentClassLike = &target;
    PutDeclToClassLikeBody(*classDecl, target);

    return classDecl;
}

OwnedPtr<FuncDecl> ASTFactory::CreateGetObjCClass(ClassLikeDecl& target)
{
    auto curFile = target.curFile;
    auto getClassCall = CreateGetCachedClassAccess(*StaticCast<ClassTy>(target.DataTy()), curFile);
    auto classDecl = CreateGetObjCClassDecl(target);
    classDecl->funcBody->body = CreateBlock(Nodes<Node>(std::move(getClassCall)));
    return classDecl;
}

OwnedPtr<FuncDecl> ASTFactory::CreateInitCjObjectReturningObjCSelf(
    const ClassDecl& impl, const ClassDecl& regComp, FuncDecl& ctor)
{
    auto curFile = ctor.curFile;
<<<<<<< HEAD
||||||| constructed merge base
    auto registryIdTy = bridge.GetRegistryIdTy();

    std::unordered_map<std::string, Ptr<Ty>> actualTyArgMap;
    std::vector<Ptr<Ty>> funcTyParams;
    std::vector<OwnedPtr<Type>> actualPrimitiveType;

    auto wrapperParamList = MakeOwned<FuncParamList>();
    auto& wrapperParams = wrapperParamList->params;
    auto& ctorParams = ctor.funcBody->paramLists[0]->params;
    std::transform(
        ctorParams.begin(),
        ctorParams.end(),
        std::back_inserter(wrapperParams),
        [this, &actualTyArgMap](const auto& p) {
            std::string paramName = p->identifier.Val();
            // default ty
            Ptr<Ty> finalTy = p->GetTy();
            // genericTy->actualTy
            auto it = actualTyArgMap.find(p->GetTy()->name);
            if (it != actualTyArgMap.end()) {
                finalTy = it->second;
            }
            CJC_NULLPTR_CHECK(finalTy);
            Ptr<Ty> cType = this->typeMapper.Cj2CType(finalTy);
            CJC_NULLPTR_CHECK(cType);
            return CreateFuncParam(paramName, nullptr, nullptr, cType);
        }
    );

    std::vector<Ptr<Ty>> wrapperParamTys;
    std::transform(wrapperParams.begin(), wrapperParams.end(), std::back_inserter(wrapperParamTys),
        [](auto& p) { return p->GetTy(); });

    auto wrapperTy = typeManager.GetFunctionTy(wrapperParamTys, registryIdTy, {.isC = true});

    std::vector<OwnedPtr<FuncParamList>> wrapperParamLists;
    wrapperParamLists.emplace_back(std::move(wrapperParamList));

    std::vector<OwnedPtr<FuncArg>> ctorCallArgs;
    size_t argIdx = 0;
    if (!generateForOneWayMapping) {
        CJC_ASSERT_WITH_MSG(!wrapperParams.empty(), "wrapperParams should not be empty");
        auto objParamRef = CreateRefExpr(*wrapperParams[0]);
        ctorCallArgs.emplace_back(CreateFuncArg(std::move(objParamRef)));
        // skip first param, as it is needed only for restore @ObjCImpl instance.
        argIdx = 1;
    }
    while (argIdx < ctorParams.size()) {
        Ptr<Ty> finalTy = ctorParams[argIdx]->GetTy();
        // genericTy->actualTy
        auto it = actualTyArgMap.find(finalTy->name);
        if (it != actualTyArgMap.end()) {
            finalTy = it->second;
        }
        auto& wrapperParam = wrapperParams[argIdx];
        auto paramRef = WithinFile(CreateRefExpr(*wrapperParam), curFile);
        OwnedPtr<FuncArg> ctorCallArg = CreateFuncArg(WrapEntity(std::move(paramRef), *finalTy));
        ctorCallArgs.emplace_back(std::move(ctorCallArg));
        ++argIdx;
    }

    OwnedPtr<CallExpr> ctorCall;
    if (target.GetTy()->HasGeneric()) {
        auto instantiationRefExpr = CreateRefExpr(ctor);
        auto instantTy = GetInstantyForGenericTy(target, actualTyArgMap, typeManager);
        auto retTy = StaticCast<FuncTy*>(ctor.GetTy())->retTy;
        Ptr<FuncTy> funcTy;
        if (retTy->HasGeneric()) {
            funcTy = typeManager.GetFunctionTy(funcTyParams, instantTy, {.isC = true});
        } else {
            funcTy = typeManager.GetFunctionTy(funcTyParams, retTy, {.isC = true});
        }
        instantiationRefExpr->typeArguments = std::move(actualPrimitiveType);
        instantiationRefExpr->SetTy(funcTy);
        ctorCall = CreateCallExpr(
            std::move(instantiationRefExpr), std::move(ctorCallArgs), Ptr(&ctor), instantTy, CallKind::CALL_OBJECT_CREATION);
    } else {
        ctorCall = CreateCallExpr(
            CreateRefExpr(ctor), std::move(ctorCallArgs), Ptr(&ctor), target.GetTy(), CallKind::CALL_OBJECT_CREATION);
    }
    auto putToRegistryCall = CreatePutToRegistryCall(std::move(ctorCall));

    auto wrapperBody = CreateFuncBody(std::move(wrapperParamLists), CreateType(registryIdTy),
        CreateBlock(Nodes<Node>(std::move(putToRegistryCall)), registryIdTy), wrapperTy);

    auto wrapperName = nameGenerator.GenerateInitCjObjectName(ctor);

    auto wrapper = CreateFuncDecl(wrapperName, std::move(wrapperBody), wrapperTy);
    wrapper->moduleName = ctor.moduleName;
    wrapper->fullPackageName = ctor.fullPackageName;
    wrapper->EnableAttr(Attribute::C, Attribute::GLOBAL, Attribute::PUBLIC, Attribute::NO_MANGLE);
    wrapper->funcBody->funcDecl = wrapper.get();
    PutDeclToFile(*wrapper, *ctor.curFile);

    return wrapper;
}


OwnedPtr<FuncDecl> ASTFactory::CreateInitCjObjectReturningObjCSelf(const Decl& target, FuncDecl& ctor) {
    auto curFile = ctor.curFile;
=======
    auto registryIdTy = bridge.GetRegistryIdTy();

    std::unordered_map<std::string, Ptr<Ty>> actualTyArgMap;
    std::vector<ModalTy> funcTyParams;
    std::vector<OwnedPtr<Type>> actualPrimitiveType;

    auto wrapperParamList = MakeOwned<FuncParamList>();
    auto& wrapperParams = wrapperParamList->params;
    auto& ctorParams = ctor.funcBody->paramLists[0]->params;
    std::transform(
        ctorParams.begin(),
        ctorParams.end(),
        std::back_inserter(wrapperParams),
        [this, &actualTyArgMap](const auto& p) {
            std::string paramName = p->identifier.Val();
            // default ty
            Ptr<Ty> finalTy = p->DataTy();
            // genericTy->actualTy
            auto it = actualTyArgMap.find(p->DataTy()->name);
            if (it != actualTyArgMap.end()) {
                finalTy = it->second;
            }
            CJC_NULLPTR_CHECK(finalTy);
            Ptr<Ty> cType = this->typeMapper.Cj2CType(finalTy);
            CJC_NULLPTR_CHECK(cType);
            return CreateFuncParam(paramName, nullptr, nullptr, {cType});
        }
    );

    std::vector<ModalTy> wrapperParamTys;
    std::transform(wrapperParams.begin(), wrapperParams.end(), std::back_inserter(wrapperParamTys),
        [](auto& p) { return p->GetTy(); });

    auto wrapperTy = typeManager.GetFunctionTy(wrapperParamTys, {registryIdTy}, {.isC = true});

    std::vector<OwnedPtr<FuncParamList>> wrapperParamLists;
    wrapperParamLists.emplace_back(std::move(wrapperParamList));

    std::vector<OwnedPtr<FuncArg>> ctorCallArgs;
    size_t argIdx = 0;
    if (!generateForOneWayMapping) {
        CJC_ASSERT_WITH_MSG(!wrapperParams.empty(), "wrapperParams should not be empty");
        auto objParamRef = CreateRefExpr(*wrapperParams[0]);
        ctorCallArgs.emplace_back(CreateFuncArg(std::move(objParamRef)));
        // skip first param, as it is needed only for restore @ObjCImpl instance.
        argIdx = 1;
    }
    while (argIdx < ctorParams.size()) {
        Ptr<Ty> finalTy = ctorParams[argIdx]->DataTy();
        // genericTy->actualTy
        auto it = actualTyArgMap.find(finalTy->name);
        if (it != actualTyArgMap.end()) {
            finalTy = it->second;
        }
        auto& wrapperParam = wrapperParams[argIdx];
        auto paramRef = WithinFile(CreateRefExpr(*wrapperParam), curFile);
        OwnedPtr<FuncArg> ctorCallArg = CreateFuncArg(WrapEntity(std::move(paramRef), *finalTy));
        ctorCallArgs.emplace_back(std::move(ctorCallArg));
        ++argIdx;
    }

    OwnedPtr<CallExpr> ctorCall;
    if (target.DataTy()->HasGeneric()) {
        auto instantiationRefExpr = CreateRefExpr(ctor);
        auto instantTy = GetInstantyForGenericTy(target, actualTyArgMap, typeManager);
        auto retTy = StaticCast<FuncTy*>(ctor.DataTy())->retTy;
        Ptr<FuncTy> funcTy;
        if (retTy.Ty()->HasGeneric()) {
            funcTy = typeManager.GetFunctionTy(funcTyParams, {instantTy}, {.isC = true});
        } else {
            funcTy = typeManager.GetFunctionTy(funcTyParams, retTy, {.isC = true});
        }
        instantiationRefExpr->typeArguments = std::move(actualPrimitiveType);
        instantiationRefExpr->SetTy({funcTy});
        ctorCall = CreateCallExpr(std::move(instantiationRefExpr), std::move(ctorCallArgs), Ptr(&ctor), {instantTy},
            CallKind::CALL_OBJECT_CREATION);
    } else {
        ctorCall = CreateCallExpr(
            CreateRefExpr(ctor), std::move(ctorCallArgs), Ptr(&ctor), target.GetTy(), CallKind::CALL_OBJECT_CREATION);
    }
    auto putToRegistryCall = CreatePutToRegistryCall(std::move(ctorCall));

    auto wrapperBody = CreateFuncBody(std::move(wrapperParamLists), CreateType(registryIdTy),
        CreateBlock(Nodes<Node>(std::move(putToRegistryCall)), {registryIdTy}), {wrapperTy});

    auto wrapperName = nameGenerator.GenerateInitCjObjectName(ctor);

    auto wrapper = CreateFuncDecl(wrapperName, std::move(wrapperBody), {wrapperTy});
    wrapper->moduleName = ctor.moduleName;
    wrapper->fullPackageName = ctor.fullPackageName;
    wrapper->EnableAttr(Attribute::C, Attribute::GLOBAL, Attribute::PUBLIC, Attribute::NO_MANGLE);
    wrapper->funcBody->funcDecl = wrapper.get();
    PutDeclToFile(*wrapper, *ctor.curFile);

    return wrapper;
}


OwnedPtr<FuncDecl> ASTFactory::CreateInitCjObjectReturningObjCSelf(const Decl& target, FuncDecl& ctor) {
    auto curFile = ctor.curFile;
>>>>>>> feat: local mode
    auto nativeObjCIdTy = bridge.GetNativeObjCIdTy();

    auto wrapperParamList = MakeOwned<FuncParamList>();
    auto& wrapperParams = wrapperParamList->params;
    auto objCSelfParam = CreateFuncParam(NATIVE_HANDLE_IDENT, nullptr, nullptr, nativeObjCIdTy);
    auto objCSelfParamRef = CreateRefExpr(*objCSelfParam);
    auto retainedSelfRef = CreateRefExpr(*objCSelfParam);
    wrapperParams.push_back(std::move(objCSelfParam));

    auto& ctorParams = ctor.funcBody->paramLists[0]->params;
<<<<<<< HEAD
    // We can't push the first ctor param, because it has regData ty, which is tuple of ($obj, RegComp) and
    // not @C compatible
    std::transform(std::next(ctorParams.begin()), ctorParams.end(), std::back_inserter(wrapperParams), [this](auto& p) {
        return CreateFuncParam(p->identifier.Val(), nullptr, nullptr, typeMapper.Cj2CType(p->GetTy()));
||||||| constructed merge base
    std::transform(ctorParams.begin(), ctorParams.end(), std::back_inserter(wrapperParams), [this](auto& p) {
        return CreateFuncParam(p->identifier.Val(), nullptr, nullptr, typeMapper.Cj2CType(p->GetTy()));
=======
    std::transform(ctorParams.begin(), ctorParams.end(), std::back_inserter(wrapperParams), [this](auto& p) {
        return CreateFuncParam(p->identifier.Val(), nullptr, nullptr, {typeMapper.Cj2CType(p->DataTy())});
>>>>>>> feat: local mode
    });

    std::vector<ModalTy> wrapperParamTys;
    std::transform(wrapperParams.begin(), wrapperParams.end(), std::back_inserter(wrapperParamTys),
        [](auto& p) { return p->GetTy(); });

    auto wrapperTy = typeManager.GetFunctionTy(wrapperParamTys, {nativeObjCIdTy}, {.isC = true});

    std::vector<OwnedPtr<FuncParamList>> wrapperParamLists;
    wrapperParamLists.emplace_back(std::move(wrapperParamList));

    std::vector<OwnedPtr<FuncArg>> ctorCallArgs;

    auto regCompBaseCtorDecl = GetObjCImplRegCompanionBaseCtor(regComp);
    CJC_ASSERT_WITH_MSG(regCompBaseCtorDecl, "expected a base ctor in the registry companion class");
    auto regCompInstance = CreateCall(regCompBaseCtorDecl, curFile, ASTCloner::Clone(objCSelfParamRef.get()));
    auto regDataArgTy = typeManager.GetTupleTy({nativeObjCIdTy, regComp.GetTy()});
    auto regDataArg =
        CreateTupleLit(Nodes<Expr>(ASTCloner::Clone(objCSelfParamRef.get()), std::move(regCompInstance)), regDataArgTy);

    ctorCallArgs.push_back(CreateFuncArg(std::move(regDataArg)));

    size_t argIdx = 1;
    while (argIdx < ctorParams.size()) {
        auto& wrapperParam = wrapperParams[argIdx];
        auto paramRef = WithinFile(CreateRefExpr(*wrapperParam), curFile);
        auto ctorCallArg = CreateFuncArg(WrapEntity(std::move(paramRef), *ctorParams[argIdx]->DataTy()));
        ctorCallArgs.emplace_back(std::move(ctorCallArg));
        ++argIdx;
    }

    auto ctorCall = CreateCallExpr(
        CreateRefExpr(ctor), std::move(ctorCallArgs), Ptr(&ctor), impl.GetTy(), CallKind::CALL_OBJECT_CREATION);
    ctorCall->curFile = curFile;

<<<<<<< HEAD
    std::vector<OwnedPtr<Node>> wrapperNodes;
    wrapperNodes.push_back(
        CreateObjCRetainAutoreleasedReturnValueCall(WithinFile(std::move(objCSelfParamRef), curFile)));
    wrapperNodes.push_back(UnwrapEntity(std::move(ctorCall)));
||||||| constructed merge base
    auto wrapperBody = CreateFuncBody(std::move(wrapperParamLists), nullptr,
        CreateBlock(Nodes<Node>(UnwrapEntity(std::move(ctorCall))), nativeObjCIdTy), wrapperTy);

    auto wrapperName = nameGenerator.GenerateInitCjObjectName(ctor);

    auto wrapper = CreateFuncDecl(wrapperName, std::move(wrapperBody), wrapperTy);
    wrapper->moduleName = ctor.moduleName;
    wrapper->fullPackageName = ctor.fullPackageName;
    wrapper->EnableAttr(Attribute::C, Attribute::GLOBAL, Attribute::PUBLIC, Attribute::NO_MANGLE);
    wrapper->funcBody->funcDecl = wrapper.get();
    PutDeclToFile(*wrapper, *ctor.curFile);

    return wrapper;
}

OwnedPtr<FuncDecl> ASTFactory::CreateInitCjObjectForEnumNoParams(AST::EnumDecl& target, AST::VarDecl& ctor)
{
    auto curFile = ctor.curFile;
    CJC_NULLPTR_CHECK(curFile);
    auto registryIdTy = bridge.GetRegistryIdTy();

    auto wrapperParamList = MakeOwned<FuncParamList>();
    std::vector<Ptr<Ty>> wrapperParamTys;
    std::vector<OwnedPtr<FuncParamList>> wrapperParamLists;
    wrapperParamLists.emplace_back(std::move(wrapperParamList));
=======
    auto wrapperBody = CreateFuncBody(std::move(wrapperParamLists), nullptr,
        CreateBlock(Nodes<Node>(UnwrapEntity(std::move(ctorCall))), {nativeObjCIdTy}), {wrapperTy});

    auto wrapperName = nameGenerator.GenerateInitCjObjectName(ctor);

    auto wrapper = CreateFuncDecl(wrapperName, std::move(wrapperBody), {wrapperTy});
    wrapper->moduleName = ctor.moduleName;
    wrapper->fullPackageName = ctor.fullPackageName;
    wrapper->EnableAttr(Attribute::C, Attribute::GLOBAL, Attribute::PUBLIC, Attribute::NO_MANGLE);
    wrapper->funcBody->funcDecl = wrapper.get();
    PutDeclToFile(*wrapper, *ctor.curFile);

    return wrapper;
}

OwnedPtr<FuncDecl> ASTFactory::CreateInitCjObjectForEnumNoParams(AST::EnumDecl& target, AST::VarDecl& ctor)
{
    auto curFile = ctor.curFile;
    CJC_NULLPTR_CHECK(curFile);
    auto registryIdTy = bridge.GetRegistryIdTy();

    auto wrapperParamList = MakeOwned<FuncParamList>();
    std::vector<ModalTy> wrapperParamTys;
    std::vector<OwnedPtr<FuncParamList>> wrapperParamLists;
    wrapperParamLists.emplace_back(std::move(wrapperParamList));
>>>>>>> feat: local mode

    auto wrapperBody = CreateFuncBody(
        std::move(wrapperParamLists), nullptr, CreateBlock(std::move(wrapperNodes), nativeObjCIdTy), wrapperTy);

    auto wrapperName = nameGenerator.GenerateInitCjObjectName(ctor);
<<<<<<< HEAD
||||||| constructed merge base
    auto wrapperTy = typeManager.GetFunctionTy(wrapperParamTys, registryIdTy, {.isC = true});
    auto wrapperBody = CreateFuncBody(std::move(wrapperParamLists), CreateType(registryIdTy),
        CreateBlock(Nodes<Node>(std::move(putToRegistryCall)), registryIdTy), wrapperTy);
=======
    auto wrapperTy = typeManager.GetFunctionTy(wrapperParamTys, {registryIdTy}, {.isC = true});
    auto wrapperBody = CreateFuncBody(std::move(wrapperParamLists), CreateType(registryIdTy),
        CreateBlock(Nodes<Node>(std::move(putToRegistryCall)), {registryIdTy}), {wrapperTy});
>>>>>>> feat: local mode

    auto wrapper = CreateFuncDecl(wrapperName, std::move(wrapperBody), {wrapperTy});
    wrapper->moduleName = ctor.moduleName;
    wrapper->fullPackageName = ctor.fullPackageName;
    wrapper->EnableAttr(Attribute::C, Attribute::GLOBAL, Attribute::PUBLIC, Attribute::NO_MANGLE);
    wrapper->funcBody->funcDecl = wrapper.get();
    PutDeclToFile(*wrapper, *ctor.curFile);

    return wrapper;
}

OwnedPtr<FuncDecl> ASTFactory::CreateDeleteCjObject(Decl& target)
{
    auto registryIdTy = bridge.GetRegistryIdTy();
    auto param = CreateFuncParam(REGISTRY_ID_IDENT, CreateType(registryIdTy), nullptr, {registryIdTy});
    auto unitTy = TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT);
    auto paramRef = CreateRefExpr(*param);

    std::vector<ModalTy> funcParamTys;
    funcParamTys.emplace_back(param->GetTy());
    auto funcTy = typeManager.GetFunctionTy(std::move(funcParamTys), {unitTy}, {.isC = true});

    std::vector<OwnedPtr<Node>> funcNodes;
    auto removeFromRegistryCall = CreateRemoveFromRegistryCall(std::move(paramRef));
    funcNodes.emplace_back(std::move(removeFromRegistryCall));

    std::vector<OwnedPtr<FuncParam>> funcParams;
    funcParams.emplace_back(std::move(param));
    auto paramList = CreateFuncParamList(std::move(funcParams));
    std::vector<OwnedPtr<FuncParamList>> paramLists;
    paramLists.emplace_back(std::move(paramList));

    auto funcBody = CreateFuncBody(std::move(paramLists), Native::FFI::CreateUnitType(target.curFile),
        CreateBlock(std::move(funcNodes), {unitTy}), {funcTy});

    auto funcName = nameGenerator.GenerateDeleteCjObjectName(target);

    auto ret = CreateFuncDecl(funcName, std::move(funcBody), {funcTy});
    ret->moduleName = target.moduleName;
    ret->fullPackageName = target.fullPackageName;
    ret->EnableAttr(Attribute::C, Attribute::GLOBAL, Attribute::PUBLIC, Attribute::NO_MANGLE);
    ret->funcBody->funcDecl = ret.get();
    PutDeclToFile(*ret, *target.curFile);

    return ret;
}

OwnedPtr<FuncDecl> ASTFactory::CreateMethodWrapper(FuncDecl& method, ClassDecl& impl)
{
    auto curFile = method.curFile;

    auto wrapperParamList = MakeOwned<FuncParamList>();
    auto& wrapperParams = wrapperParamList->params;

    // Alias the return Type node so each use reads the current Ptr<Ty> (same as former retType->ty ref).
    auto& methodRetType = *method.funcBody->retType;
    OwnedPtr<VarDecl> objTmpVarDecl;
    OwnedPtr<MemberAccess> methodExpr;
<<<<<<< HEAD
||||||| constructed merge base
    std::unordered_map<std::string, Ptr<Ty>> actualTyArgMap;
    std::vector<Ptr<Ty>> funcTyParams;
    std::vector<OwnedPtr<Type>> actualPrimitiveType;
    auto instantTy = GetInstantyForGenericTy(*method.outerDecl, actualTyArgMap, typeManager);
    auto retActualTy =
        methodRetType.GetTy()->IsGeneric() ? actualTyArgMap[methodRetType.GetTy()->name] : methodRetType.GetTy();
=======
    std::unordered_map<std::string, Ptr<Ty>> actualTyArgMap;
    std::vector<ModalTy> funcTyParams;
    std::vector<OwnedPtr<Type>> actualPrimitiveType;
    auto instantTy = GetInstantyForGenericTy(*method.outerDecl, actualTyArgMap, typeManager);
    Ptr<Ty> retActualTy;
    if (methodRetType.DataTy()->IsGeneric()) {
        retActualTy = actualTyArgMap[methodRetType.DataTy()->name];
    } else {
        retActualTy = methodRetType.DataTy();
    }
>>>>>>> feat: local mode
    if (method.TestAttr(Attribute::STATIC)) {
        auto staticRefExpr = CreateRefExpr(*method.outerDecl);
<<<<<<< HEAD
||||||| constructed merge base
        if (method.outerDecl->GetTy()->HasGeneric()) {
            staticRefExpr->typeArguments = std::move(actualPrimitiveType);
            staticRefExpr->SetTy(instantTy);
        }
=======
        if (method.outerDecl->DataTy()->HasGeneric()) {
            staticRefExpr->typeArguments = std::move(actualPrimitiveType);
            staticRefExpr->SetTy({instantTy});
        }
>>>>>>> feat: local mode
        methodExpr = CreateMemberAccess(WithinFile(std::move(staticRefExpr), method.curFile), method);
    } else {
        auto registryIdTy = bridge.GetRegistryIdTy();
        auto registryIdParam = CreateFuncParam(REGISTRY_ID_IDENT, CreateType(registryIdTy), nullptr, {registryIdTy});
        auto registryIdParamRef = CreateRefExpr(*registryIdParam);
<<<<<<< HEAD

        auto nativeObjCIdTy = bridge.GetNativeObjCIdTy();
        auto objCSelfParam = CreateFuncParam(NATIVE_HANDLE_IDENT, nullptr, nullptr, nativeObjCIdTy);
        auto objCSelfParamRef = CreateRefExpr(*objCSelfParam);

        wrapperParams.push_back(std::move(registryIdParam));
        wrapperParams.push_back(std::move(objCSelfParam));

        auto implBaseCtorCall = CreateObjCImplBaseCtorCall(impl, std::move(objCSelfParamRef),
            std::move(registryIdParamRef), curFile, Retain::RETAIN_AUTORELEASED_RETURN_VALUE);
        objTmpVarDecl = CreateTmpVarDecl(CreateRefType(impl), std::move(implBaseCtorCall));
||||||| constructed merge base
        wrapperParams.emplace_back(std::move(registryIdParam));
        if (outerDecl->GetTy()->HasGeneric()) {
            auto registryTy = CreateRefType(*outerDecl, instantTy);
            registryTy->typeArguments = std::move(actualPrimitiveType);

            getFromRegistryCall =
                CreateGetFromRegistryByIdCall(std::move(registryIdParamRef), std::move(registryTy));
            objTmpVarDecl = CreateTmpVarDecl(CreateType(instantTy), std::move(getFromRegistryCall));
        } else {
            getFromRegistryCall =
                CreateGetFromRegistryByIdCall(std::move(registryIdParamRef), CreateRefType(*outerDecl));
            objTmpVarDecl = CreateTmpVarDecl(CreateRefType(*outerDecl), std::move(getFromRegistryCall));
        }
=======
        wrapperParams.emplace_back(std::move(registryIdParam));
        if (outerDecl->DataTy()->HasGeneric()) {
            auto registryTy = CreateRefType(*outerDecl, instantTy);
            registryTy->typeArguments = std::move(actualPrimitiveType);

            getFromRegistryCall =
                CreateGetFromRegistryByIdCall(std::move(registryIdParamRef), std::move(registryTy));
            objTmpVarDecl = CreateTmpVarDecl(CreateType(instantTy), std::move(getFromRegistryCall));
        } else {
            getFromRegistryCall =
                CreateGetFromRegistryByIdCall(std::move(registryIdParamRef), CreateRefType(*outerDecl));
            objTmpVarDecl = CreateTmpVarDecl(CreateRefType(*outerDecl), std::move(getFromRegistryCall));
        }
>>>>>>> feat: local mode

        // The mut function object does not allow assignment using the 'let' method and needs to use 'var' instead.
        if (method.TestAttr(Attribute::MUT)) {
            objTmpVarDecl->isVar = true;
        }

        methodExpr = CreateMemberAccess(CreateRefExpr(*objTmpVarDecl), method);
<<<<<<< HEAD
        methodExpr->SetTy(typeManager.GetFunctionTy({}, methodRetType.GetTy()));
||||||| constructed merge base
        if (!methodRetType.GetTy()->IsGeneric()) {
            methodExpr->SetTy(typeManager.GetFunctionTy(funcTyParams, methodRetType.GetTy()));
        } else {
            methodExpr->SetTy(typeManager.GetFunctionTy(funcTyParams, retActualTy));
        }
=======
        if (!methodRetType.DataTy()->IsGeneric()) {
            methodExpr->SetTy({typeManager.GetFunctionTy(funcTyParams, methodRetType.GetTy())});
        } else {
            methodExpr->SetTy({typeManager.GetFunctionTy(funcTyParams, {retActualTy})});
        }
>>>>>>> feat: local mode
    }

    auto& originParams = method.funcBody->paramLists[0]->params;
<<<<<<< HEAD
    std::transform(originParams.begin(), originParams.end(), std::back_inserter(wrapperParams), [this](const auto& p) {
        Ptr<Ty> finalTy = p->GetTy();
        CJC_NULLPTR_CHECK(finalTy);
        auto convertedParamTy = typeMapper.Cj2CType(finalTy);
        CJC_NULLPTR_CHECK(convertedParamTy);
        return CreateFuncParam(p->identifier.GetRawText(), CreateType(convertedParamTy), nullptr, convertedParamTy);
    });
||||||| constructed merge base
    std::transform(
        originParams.begin(),
        originParams.end(),
        std::back_inserter(wrapperParams),
        [this, &actualTyArgMap](const auto& p) {
            std::string paramName = p->identifier.Val();
            // default ty
            Ptr<Ty> finalTy = p->GetTy();
            // genericTy->actualTy
            auto it = actualTyArgMap.find(p->GetTy()->name);
            if (it != actualTyArgMap.end()) {
                finalTy = it->second;
            }
            CJC_NULLPTR_CHECK(finalTy);
            auto convertedParamTy = typeMapper.Cj2CType(finalTy);
            CJC_NULLPTR_CHECK(convertedParamTy);
            return CreateFuncParam(p->identifier, CreateType(convertedParamTy), nullptr, convertedParamTy);
        }
    );
=======
    std::transform(
        originParams.begin(),
        originParams.end(),
        std::back_inserter(wrapperParams),
        [this, &actualTyArgMap](const auto& p) {
            std::string paramName = p->identifier.Val();
            // default ty
            Ptr<Ty> finalTy = p->DataTy();
            // genericTy->actualTy
            auto it = actualTyArgMap.find(p->DataTy()->name);
            if (it != actualTyArgMap.end()) {
                finalTy = it->second;
            }
            CJC_NULLPTR_CHECK(finalTy);
            auto convertedParamTy = typeMapper.Cj2CType(finalTy);
            CJC_NULLPTR_CHECK(convertedParamTy);
            return CreateFuncParam(p->identifier, CreateType(convertedParamTy), nullptr, convertedParamTy);
        }
    );
>>>>>>> feat: local mode

    std::vector<ModalTy> wrapperParamTys;
    std::transform(wrapperParams.begin(), wrapperParams.end(), std::back_inserter(wrapperParamTys),
        [](auto& p) { return p->GetTy(); });

<<<<<<< HEAD
    auto retWrapperTy =
        typeManager.GetFunctionTy(wrapperParamTys, typeMapper.Cj2CType(methodRetType.GetTy()), {.isC = true});
||||||| constructed merge base
    Ptr<FuncTy> retWrapperTy;
    if (methodRetType.GetTy()->HasGeneric()) {
        retWrapperTy = typeManager.GetFunctionTy(wrapperParamTys, typeMapper.Cj2CType(retActualTy), {.isC = true});
    } else {
        retWrapperTy =
            typeManager.GetFunctionTy(wrapperParamTys, typeMapper.Cj2CType(methodRetType.GetTy()), {.isC = true});
    }
=======
    Ptr<FuncTy> retWrapperTy;
    if (methodRetType.DataTy()->HasGeneric()) {
        retWrapperTy = typeManager.GetFunctionTy(wrapperParamTys, {typeMapper.Cj2CType(retActualTy)}, {.isC = true});
    } else {
        retWrapperTy =
            typeManager.GetFunctionTy(wrapperParamTys, {typeMapper.Cj2CType(methodRetType.DataTy())}, {.isC = true});
    }
>>>>>>> feat: local mode

    std::vector<OwnedPtr<FuncParamList>> wrapperParamLists;
    wrapperParamLists.emplace_back(std::move(wrapperParamList));

    methodExpr->curFile = method.curFile;
    methodExpr->begin = method.GetBegin();
    methodExpr->end = method.GetEnd();

    std::vector<OwnedPtr<FuncArg>> methodArgs;
    // Skip the leading $registryId/$obj params
    size_t index = method.TestAttr(Attribute::STATIC) ? 0 : SELF_INFO_PARAMS_COUNT;
    for (size_t i = index; i < wrapperParams.size(); ++i) {
        auto wrapperParam = wrapperParams[i].get();
        auto originParam = originParams[i - index].get();

<<<<<<< HEAD
        Ptr<Ty> finalTy = originParam->GetTy();
||||||| constructed merge base
        Ptr<Ty> finalTy = originParam->GetTy();
        // genericTy->actualTy
        auto it = actualTyArgMap.find(finalTy->name);
        if (it != actualTyArgMap.end()) {
            finalTy = it->second;
        }
=======
        Ptr<Ty> finalTy = originParam->DataTy();
        // genericTy->actualTy
        auto it = actualTyArgMap.find(finalTy->name);
        if (it != actualTyArgMap.end()) {
            finalTy = it->second;
        }
>>>>>>> feat: local mode
        auto paramRef = CreateRefExpr(*wrapperParam);
        auto wrappedParamRef = WrapEntity(WithinFile(std::move(paramRef), method.curFile), *finalTy);
        auto arg = CreateFuncArg(std::move(wrappedParamRef), wrapperParam->identifier, {finalTy});
        methodArgs.emplace_back(std::move(arg));
    }

<<<<<<< HEAD
    auto methodCall = CreateCallExpr(std::move(methodExpr), std::move(methodArgs), Ptr(&method), methodRetType.GetTy(),
        CallKind::CALL_DECLARED_FUNCTION);
||||||| constructed merge base
    OwnedPtr<CallExpr> methodCall;
    if (outerDecl->GetTy()->HasGeneric()) {
        methodCall = CreateCallExpr(std::move(methodExpr), std::move(methodArgs), Ptr(&method),
            retActualTy, CallKind::CALL_DECLARED_FUNCTION);
    } else {
        methodCall = CreateCallExpr(std::move(methodExpr), std::move(methodArgs), Ptr(&method), methodRetType.GetTy(),
            CallKind::CALL_DECLARED_FUNCTION);
    }
=======
    OwnedPtr<CallExpr> methodCall;
    if (outerDecl->DataTy()->HasGeneric()) {
        methodCall = CreateCallExpr(std::move(methodExpr), std::move(methodArgs), Ptr(&method), {retActualTy},
            CallKind::CALL_DECLARED_FUNCTION);
    } else {
        methodCall = CreateCallExpr(std::move(methodExpr), std::move(methodArgs), Ptr(&method), methodRetType.GetTy(),
            CallKind::CALL_DECLARED_FUNCTION);
    }
>>>>>>> feat: local mode

    std::vector<OwnedPtr<Node>> wrapperNodes;
    if (!method.TestAttr(Attribute::STATIC)) {
        wrapperNodes.emplace_back(std::move(objTmpVarDecl));
    }
    auto resTy = methodCall->GetTy();
    auto unwrappedMethodCall = UnwrapEntity(std::move(methodCall));
    if (IsObjCObjectType(*resTy)) {
        // We always return object values with retain count +1
        // ARC balances it with __bridge_transfer
        // It's done to prevent over release objects, e.g. if Cangjie GC triggers before control acquired by ARC and
        // @Mirror object is collected
        unwrappedMethodCall = CreateObjCRetainCall(std::move(unwrappedMethodCall));
    }
    wrapperNodes.emplace_back(std::move(unwrappedMethodCall));

    auto wrapperBody = CreateFuncBody(std::move(wrapperParamLists), CreateType(retWrapperTy->retTy.Ty()),
        CreateBlock(std::move(wrapperNodes), retWrapperTy->retTy), {retWrapperTy});

    auto wrapperName = nameGenerator.GenerateMethodWrapperName(method);

    auto wrapper = CreateFuncDecl(wrapperName, std::move(wrapperBody), {retWrapperTy});
    wrapper->moduleName = method.moduleName;
    wrapper->fullPackageName = method.fullPackageName;
    wrapper->EnableAttr(Attribute::C, Attribute::GLOBAL, Attribute::PUBLIC, Attribute::NO_MANGLE);
    wrapper->funcBody->funcDecl = wrapper.get();

    PutDeclToFile(*wrapper, *method.curFile);

    return wrapper;
}

OwnedPtr<FuncDecl> ASTFactory::CreateGetterWrapper(PropDecl& prop, ClassDecl& impl)
{
    auto curFile = prop.curFile;

    auto wrapperParamList = MakeOwned<FuncParamList>();
    auto& wrapperParams = wrapperParamList->params;

    OwnedPtr<VarDecl> objTmpVarDecl;
    OwnedPtr<MemberAccess> propGetterExpr;
    if (prop.TestAttr(Attribute::STATIC)) {
        propGetterExpr = CreateMemberAccess(WithinFile(CreateRefExpr(impl), prop.curFile), *prop.getters[0].get());
    } else {
        auto registryIdTy = bridge.GetRegistryIdTy();
        auto registryIdParam = CreateFuncParam(REGISTRY_ID_IDENT, CreateType(registryIdTy), nullptr, {registryIdTy});
        auto registryIdParamRef = CreateRefExpr(*registryIdParam);

        auto nativeObjCIdTy = bridge.GetNativeObjCIdTy();
        auto objCSelfParam = CreateFuncParam(NATIVE_HANDLE_IDENT, nullptr, nullptr, nativeObjCIdTy);
        auto objCSelfParamRef = CreateRefExpr(*objCSelfParam);

        wrapperParams.push_back(std::move(registryIdParam));
        wrapperParams.push_back(std::move(objCSelfParam));

        auto implBaseCtorCall = CreateObjCImplBaseCtorCall(impl, std::move(objCSelfParamRef),
            std::move(registryIdParamRef), curFile, Retain::RETAIN_AUTORELEASED_RETURN_VALUE);
        objTmpVarDecl = CreateTmpVarDecl(CreateRefType(impl), std::move(implBaseCtorCall));
        // Not sure if accessing the first getter is good
        propGetterExpr = CreateMemberAccess(CreateRefExpr(*objTmpVarDecl), *prop.getters[0].get());
    }
    propGetterExpr->curFile = prop.curFile;
    propGetterExpr->begin = prop.GetBegin();
    propGetterExpr->end = prop.GetEnd();
    auto propGetterCall = CreateCallExpr(
        std::move(propGetterExpr), {}, Ptr(prop.getters[0].get()), prop.GetTy(), CallKind::CALL_DECLARED_FUNCTION);

    std::vector<ModalTy> wrapperParamTys;
    std::transform(wrapperParams.begin(), wrapperParams.end(), std::back_inserter(wrapperParamTys),
        [](auto& p) { return p->GetTy(); });

    auto wrapperTy = typeManager.GetFunctionTy(wrapperParamTys, {typeMapper.Cj2CType(prop.DataTy())}, {.isC = true});

    std::vector<OwnedPtr<FuncParamList>> wrapperParamLists;
    wrapperParamLists.emplace_back(std::move(wrapperParamList));

    std::vector<OwnedPtr<Node>> wrapperNodes;
    if (!prop.TestAttr(Attribute::STATIC)) {
        wrapperNodes.emplace_back(std::move(objTmpVarDecl));
    }
    auto unwrappedPropGetterCall = UnwrapEntity(std::move(propGetterCall));
    if (IsObjCObjectType(*prop.GetTy())) {
        // We always return object values with retain count +1
        // ARC balances it with __bridge_transfer
        // It's done to prevent over release objects, e.g. if Cangjie GC triggers before control acquired by ARC and
        // @Mirror object is collected
        unwrappedPropGetterCall = CreateObjCRetainCall(std::move(unwrappedPropGetterCall));
    }
    wrapperNodes.emplace_back(std::move(unwrappedPropGetterCall));

    auto wrapperBody = CreateFuncBody(std::move(wrapperParamLists), CreateType(wrapperTy->retTy.Ty()),
        CreateBlock(std::move(wrapperNodes), wrapperTy->retTy), {wrapperTy});

    auto wrapperName = nameGenerator.GeneratePropGetterWrapperName(prop);

    auto wrapper = CreateFuncDecl(wrapperName, std::move(wrapperBody), {wrapperTy});
    wrapper->moduleName = prop.moduleName;
    wrapper->fullPackageName = prop.fullPackageName;
    wrapper->EnableAttr(Attribute::C, Attribute::GLOBAL, Attribute::PUBLIC, Attribute::NO_MANGLE);
    wrapper->funcBody->funcDecl = wrapper.get();

    PutDeclToFile(*wrapper, *prop.curFile);

    return wrapper;
}

OwnedPtr<FuncDecl> ASTFactory::CreateSetterWrapper(PropDecl& prop, ClassDecl& impl)
{
    auto curFile = prop.curFile;
    auto registryIdTy = bridge.GetRegistryIdTy();
    auto unitTy = TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT);

<<<<<<< HEAD
    auto convertedPropTy = typeMapper.Cj2CType(prop.GetTy());
    auto setterParam = CreateFuncParam(VALUE_IDENT, CreateType(convertedPropTy), nullptr, convertedPropTy);
    auto setterParamRef = WithinFile(CreateRefExpr(*setterParam), curFile);
||||||| constructed merge base
    auto convertedPropTy = typeMapper.Cj2CType(prop.GetTy());
    auto setterParam = CreateFuncParam(VALUE_IDENT, CreateType(convertedPropTy), nullptr, convertedPropTy);
    auto setterParamRef = CreateRefExpr(*setterParam);
=======
    auto convertedPropTy = typeMapper.Cj2CType(prop.DataTy());
    auto setterParam = CreateFuncParam(VALUE_IDENT, CreateType(convertedPropTy), nullptr, {convertedPropTy});
    auto setterParamRef = CreateRefExpr(*setterParam);
>>>>>>> feat: local mode

    auto outerDecl = static_cast<InheritableDecl*>(prop.outerDecl.get());
    CJC_NULLPTR_CHECK(outerDecl);

    auto wrapperParamList = MakeOwned<FuncParamList>();
    auto& wrapperParams = wrapperParamList->params;

    std::vector<OwnedPtr<Node>> wrapperNodes;

    // Not sure if accessing the first setter is good
    auto& setter = *prop.setters[0].get();
    auto& originParams = setter.funcBody->paramLists[0]->params;
    OwnedPtr<Expr> propSetterExpr;

    OwnedPtr<Decl> objTmpVarDecl;

    if (prop.TestAttr(Attribute::STATIC)) {
        wrapperParams.emplace_back(std::move(setterParam));
        propSetterExpr = CreateMemberAccess(WithinFile(CreateRefExpr(*outerDecl), prop.curFile), setter);
        propSetterExpr->curFile = curFile;
        propSetterExpr->begin = prop.GetBegin();
        propSetterExpr->end = prop.GetEnd();
    } else {
<<<<<<< HEAD
        auto registryIdParam = CreateFuncParam(REGISTRY_ID_IDENT, CreateType(registryIdTy), nullptr, registryIdTy);
        auto registryIdParamRef = WithinFile(CreateRefExpr(*registryIdParam), curFile);
||||||| constructed merge base
        auto registryIdParam = CreateFuncParam(REGISTRY_ID_IDENT, CreateType(registryIdTy), nullptr, registryIdTy);
        auto registryIdParamRef = CreateRefExpr(*registryIdParam);
        auto getFromRegistryCall =
            CreateGetFromRegistryByIdCall(std::move(registryIdParamRef), CreateRefType(*outerDecl));
=======
        auto registryIdParam = CreateFuncParam(REGISTRY_ID_IDENT, CreateType(registryIdTy), nullptr, {registryIdTy});
        auto registryIdParamRef = CreateRefExpr(*registryIdParam);
        auto getFromRegistryCall =
            CreateGetFromRegistryByIdCall(std::move(registryIdParamRef), CreateRefType(*outerDecl));
>>>>>>> feat: local mode

        auto nativeObjCIdTy = bridge.GetNativeObjCIdTy();
        auto objCSelfParam = CreateFuncParam(NATIVE_HANDLE_IDENT, nullptr, nullptr, nativeObjCIdTy);
        auto objCSelfParamRef = WithinFile(CreateRefExpr(*objCSelfParam), curFile);

        auto implBaseCtorCall = CreateObjCImplBaseCtorCall(impl, std::move(objCSelfParamRef),
            std::move(registryIdParamRef), curFile, Retain::RETAIN_AUTORELEASED_RETURN_VALUE);
        objTmpVarDecl = CreateTmpVarDecl(CreateRefType(impl), std::move(implBaseCtorCall));

        wrapperParams.push_back(std::move(registryIdParam));
        wrapperParams.push_back(std::move(objCSelfParam));
        wrapperParams.push_back(std::move(setterParam));
        propSetterExpr = CreateMemberAccess(CreateRefExpr(*objTmpVarDecl), setter);
        propSetterExpr->curFile = curFile;
        propSetterExpr->begin = prop.GetBegin();
        propSetterExpr->end = prop.GetEnd();
    }
    std::vector<OwnedPtr<FuncArg>> propSetterArgs;

    CJC_ASSERT(!wrapperParams.empty());
    {
        auto wrapperParam = wrapperParams.back().get();
        auto originParam = originParams.back().get();

<<<<<<< HEAD
        auto paramRef = WithinFile(CreateRefExpr(*wrapperParam), curFile);
        auto wrappedParamRef = WrapEntity(std::move(paramRef), *originParam->GetTy());
||||||| constructed merge base
        auto paramRef = CreateRefExpr(*wrapperParam);
        auto wrappedParamRef = WrapEntity(std::move(paramRef), *originParam->GetTy());
=======
        auto paramRef = CreateRefExpr(*wrapperParam);
        auto wrappedParamRef = WrapEntity(std::move(paramRef), *originParam->DataTy());
>>>>>>> feat: local mode
        auto arg = CreateFuncArg(std::move(wrappedParamRef), wrapperParam->identifier, originParam->GetTy());
        propSetterArgs.emplace_back(std::move(arg));
    }

    std::vector<ModalTy> wrapperParamTys;
    std::transform(wrapperParams.begin(), wrapperParams.end(), std::back_inserter(wrapperParamTys),
        [](auto& p) { return p->GetTy(); });

    auto wrapperTy = typeManager.GetFunctionTy(wrapperParamTys, {unitTy}, {.isC = true});

    std::vector<OwnedPtr<FuncParamList>> wrapperParamLists;
    wrapperParamLists.emplace_back(std::move(wrapperParamList));

    auto propSetterCall = CreateCallExpr(
        std::move(propSetterExpr), std::move(propSetterArgs), Ptr(&setter), {unitTy}, CallKind::CALL_DECLARED_FUNCTION);

    if (objTmpVarDecl != nullptr) {
        wrapperNodes.emplace_back(std::move(objTmpVarDecl));
    }
    wrapperNodes.emplace_back(std::move(propSetterCall));

<<<<<<< HEAD
    auto wrapperBody = CreateFuncBody(std::move(wrapperParamLists), CreateUnitType(curFile),
        CreateBlock(std::move(wrapperNodes), wrapperTy->retTy), wrapperTy);
||||||| constructed merge base
    auto wrapperBody = CreateFuncBody(std::move(wrapperParamLists), CreateUnitType(prop.curFile),
        CreateBlock(std::move(wrapperNodes), wrapperTy->retTy), wrapperTy);
=======
    auto wrapperBody = CreateFuncBody(std::move(wrapperParamLists), CreateUnitType(prop.curFile),
        CreateBlock(std::move(wrapperNodes), wrapperTy->retTy), {wrapperTy});
>>>>>>> feat: local mode

    auto wrapperName = nameGenerator.GetPropSetterWrapperName(prop);

    auto wrapper = CreateFuncDecl(wrapperName, std::move(wrapperBody), {wrapperTy});
    wrapper->moduleName = prop.moduleName;
    wrapper->fullPackageName = prop.fullPackageName;
    wrapper->EnableAttr(Attribute::C, Attribute::GLOBAL, Attribute::PUBLIC, Attribute::NO_MANGLE);
    wrapper->funcBody->funcDecl = wrapper.get();

    PutDeclToFile(*wrapper, *curFile);

    return wrapper;
}

OwnedPtr<FuncDecl> ASTFactory::CreateGetterWrapper(VarDecl& field, ClassDecl& impl)
{
    auto curFile = field.curFile;

    auto wrapperParamList = MakeOwned<FuncParamList>();
    auto& wrapperParams = wrapperParamList->params;

    OwnedPtr<VarDecl> objTmpVarDecl;
    OwnedPtr<MemberAccess> fieldExpr;
    if (field.TestAttr(Attribute::STATIC)) {
        fieldExpr = CreateMemberAccess(WithinFile(CreateRefExpr(impl), field.curFile), field);
    } else {
        auto registryIdTy = bridge.GetRegistryIdTy();
        auto registryIdParam = CreateFuncParam(REGISTRY_ID_IDENT, CreateType(registryIdTy), nullptr, {registryIdTy});
        auto registryIdParamRef = CreateRefExpr(*registryIdParam);
<<<<<<< HEAD

        auto nativeObjCIdTy = bridge.GetNativeObjCIdTy();
        auto objCSelfParam = CreateFuncParam(NATIVE_HANDLE_IDENT, nullptr, nullptr, nativeObjCIdTy);
        auto objCSelfParamRef = CreateRefExpr(*objCSelfParam);

        wrapperParams.push_back(std::move(registryIdParam));
        wrapperParams.push_back(std::move(objCSelfParam));

        auto implBaseCtorCall = CreateObjCImplBaseCtorCall(impl, std::move(objCSelfParamRef),
            std::move(registryIdParamRef), curFile, Retain::RETAIN_AUTORELEASED_RETURN_VALUE);
        objTmpVarDecl = CreateTmpVarDecl(CreateRefType(impl), std::move(implBaseCtorCall));
        fieldExpr = CreateMemberAccess(CreateRefExpr(*objTmpVarDecl), field);
||||||| constructed merge base
        wrapperParams.emplace_back(std::move(registryIdParam));
        if (outerDecl->GetTy()->HasGeneric()) {
            auto instantTy = GetInstantyForGenericTy(*outerDecl, actualTyArgMap, typeManager);
            auto registryTy = CreateRefType(*outerDecl, instantTy);
            registryTy->typeArguments = std::move(actualPrimitiveType);
            auto refTy = ASTCloner::Clone<RefType>(registryTy);
            getFromRegistryCall =
                CreateGetFromRegistryByIdCall(std::move(registryIdParamRef), std::move(registryTy));
            objTmpVarDecl = CreateTmpVarDecl(std::move(refTy), std::move(getFromRegistryCall));
            fieldExpr = CreateMemberAccess(CreateRefExpr(*objTmpVarDecl), field);
            if (field.GetTy()->IsGeneric()) {
                fieldExpr->SetTy(genericActualTy);
            }
        } else {
            getFromRegistryCall =
                CreateGetFromRegistryByIdCall(std::move(registryIdParamRef), CreateRefType(*outerDecl));
            objTmpVarDecl = CreateTmpVarDecl(CreateRefType(*outerDecl), std::move(getFromRegistryCall));
            fieldExpr = CreateMemberAccess(CreateRefExpr(*objTmpVarDecl), field);
        }
=======
        wrapperParams.emplace_back(std::move(registryIdParam));
        if (outerDecl->DataTy()->HasGeneric()) {
            auto instantTy = GetInstantyForGenericTy(*outerDecl, actualTyArgMap, typeManager);
            auto registryTy = CreateRefType(*outerDecl, instantTy);
            registryTy->typeArguments = std::move(actualPrimitiveType);
            auto refTy = ASTCloner::Clone<RefType>(registryTy);
            getFromRegistryCall =
                CreateGetFromRegistryByIdCall(std::move(registryIdParamRef), std::move(registryTy));
            objTmpVarDecl = CreateTmpVarDecl(std::move(refTy), std::move(getFromRegistryCall));
            fieldExpr = CreateMemberAccess(CreateRefExpr(*objTmpVarDecl), field);
            if (field.DataTy()->IsGeneric()) {
                fieldExpr->SetTy({genericActualTy});
            }
        } else {
            getFromRegistryCall =
                CreateGetFromRegistryByIdCall(std::move(registryIdParamRef), CreateRefType(*outerDecl));
            objTmpVarDecl = CreateTmpVarDecl(CreateRefType(*outerDecl), std::move(getFromRegistryCall));
            fieldExpr = CreateMemberAccess(CreateRefExpr(*objTmpVarDecl), field);
        }
>>>>>>> feat: local mode
    }
    fieldExpr->curFile = curFile;
    fieldExpr->begin = field.GetBegin();
    fieldExpr->end = field.GetEnd();

    std::vector<ModalTy> wrapperParamTys;
    std::transform(wrapperParams.begin(), wrapperParams.end(), std::back_inserter(wrapperParamTys),
        [](auto& p) { return p->GetTy(); });

<<<<<<< HEAD
    Ptr<Ty> fieldObjCTy = typeMapper.Cj2CType(field.GetTy());
    auto wrapperTy = typeManager.GetFunctionTy(wrapperParamTys, fieldObjCTy, {.isC = true});
||||||| constructed merge base
    Ptr<Ty> fieldObjCTy = field.GetTy()->IsGeneric() ? genericActualTy : typeMapper.Cj2CType(field.GetTy());
    auto wrapperTy = typeManager.GetFunctionTy(wrapperParamTys, fieldObjCTy, {.isC = true});
=======
    Ptr<Ty> fieldObjCTy = field.DataTy()->IsGeneric() ? genericActualTy : typeMapper.Cj2CType(field.DataTy());
    auto wrapperTy = typeManager.GetFunctionTy(wrapperParamTys, {fieldObjCTy}, {.isC = true});
>>>>>>> feat: local mode

    std::vector<OwnedPtr<FuncParamList>> wrapperParamLists;
    wrapperParamLists.emplace_back(std::move(wrapperParamList));

    std::vector<OwnedPtr<Node>> wrapperNodes;
    if (!field.TestAttr(Attribute::STATIC)) {
        wrapperNodes.emplace_back(std::move(objTmpVarDecl));
    }
    auto unwrappedFieldExpr = UnwrapEntity(std::move(fieldExpr));
    if (IsObjCObjectType(*field.GetTy())) {
        // We always return object values with retain count +1
        // ARC balances it with __bridge_transfer
        // It's done to prevent over release objects, e.g. if Cangjie GC triggers before control acquired by ARC and
        // @Mirror object is collected
        unwrappedFieldExpr = CreateObjCRetainCall(std::move(unwrappedFieldExpr));
    }
    wrapperNodes.emplace_back(std::move(unwrappedFieldExpr));

    auto wrapperBody = CreateFuncBody(std::move(wrapperParamLists), CreateType(wrapperTy->retTy.Ty()),
        CreateBlock(std::move(wrapperNodes), wrapperTy->retTy), {wrapperTy});

    // Generate wrapper name from ORIGIN field, not a mirror one.
    auto wrapperName = nameGenerator.GetFieldGetterWrapperName(field);

    auto wrapper = CreateFuncDecl(wrapperName, std::move(wrapperBody), {wrapperTy});
    wrapper->moduleName = field.moduleName;
    wrapper->fullPackageName = field.fullPackageName;
    wrapper->EnableAttr(Attribute::C, Attribute::GLOBAL, Attribute::PUBLIC, Attribute::NO_MANGLE);
    wrapper->funcBody->funcDecl = wrapper.get();

    PutDeclToFile(*wrapper, *curFile);

    return wrapper;
}

OwnedPtr<FuncDecl> ASTFactory::CreateSetterWrapper(VarDecl& field, ClassDecl& impl)
{
    auto curFile = field.curFile;
    auto registryIdTy = bridge.GetRegistryIdTy();
    auto unitTy = TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT);

<<<<<<< HEAD
    auto convertedFieldTy = typeMapper.Cj2CType(field.GetTy());
    auto setterParam = CreateFuncParam(VALUE_IDENT, CreateType(convertedFieldTy), nullptr, convertedFieldTy);
    auto setterParamRef = WithinFile(CreateRefExpr(*setterParam), curFile);
||||||| constructed merge base
    auto convertedFieldTy = typeMapper.Cj2CType(field.GetTy());
    auto setterParam = CreateFuncParam(VALUE_IDENT, CreateType(convertedFieldTy), nullptr, convertedFieldTy);
    auto setterParamRef = CreateRefExpr(*setterParam);
=======
    auto convertedFieldTy = typeMapper.Cj2CType(field.DataTy());
    auto setterParam = CreateFuncParam(VALUE_IDENT, CreateType(convertedFieldTy), nullptr, {convertedFieldTy});
    auto setterParamRef = CreateRefExpr(*setterParam);
>>>>>>> feat: local mode

    auto outerDecl = static_cast<InheritableDecl*>(field.outerDecl.get());
    CJC_NULLPTR_CHECK(outerDecl);

    auto wrapperParamList = MakeOwned<FuncParamList>();
    auto& wrapperParams = wrapperParamList->params;
    std::vector<OwnedPtr<Node>> wrapperNodes;
    OwnedPtr<MemberAccess> lhs;
    if (field.TestAttr(Attribute::STATIC)) {
        lhs = CreateMemberAccess(WithinFile(CreateRefExpr(*outerDecl), curFile), field);
    } else {
        auto registryIdParam = CreateFuncParam(REGISTRY_ID_IDENT, CreateType(registryIdTy), nullptr, {registryIdTy});
        auto registryIdParamRef = CreateRefExpr(*registryIdParam);

        auto nativeObjCIdTy = bridge.GetNativeObjCIdTy();
        auto objCSelfParam = CreateFuncParam(NATIVE_HANDLE_IDENT, nullptr, nullptr, nativeObjCIdTy);
        auto objCSelfParamRef = CreateRefExpr(*objCSelfParam);

        wrapperParams.push_back(std::move(registryIdParam));
        wrapperParams.push_back(std::move(objCSelfParam));

        auto implBaseCtorCall = CreateObjCImplBaseCtorCall(impl, std::move(objCSelfParamRef),
            std::move(registryIdParamRef), curFile, Retain::RETAIN_AUTORELEASED_RETURN_VALUE);
        auto objTmpVarDecl = CreateTmpVarDecl(CreateRefType(impl), std::move(implBaseCtorCall));
        lhs = CreateMemberAccess(CreateRefExpr(*objTmpVarDecl), field);
        wrapperNodes.emplace_back(std::move(objTmpVarDecl));
    }
    auto assignFieldExpr =
<<<<<<< HEAD
        CreateAssignExpr(std::move(lhs), WrapEntity(std::move(setterParamRef), *field.GetTy()), unitTy);
    assignFieldExpr->curFile = curFile;
||||||| constructed merge base
        CreateAssignExpr(std::move(lhs), WrapEntity(std::move(setterParamRef), *field.GetTy()), unitTy);
    assignFieldExpr->curFile = field.curFile;
=======
        CreateAssignExpr(std::move(lhs), WrapEntity(std::move(setterParamRef), *field.DataTy()), {unitTy});
    assignFieldExpr->curFile = field.curFile;
>>>>>>> feat: local mode
    assignFieldExpr->begin = field.GetBegin();
    assignFieldExpr->end = field.GetEnd();
    wrapperNodes.emplace_back(std::move(assignFieldExpr));

    wrapperParams.emplace_back(std::move(setterParam));
    std::vector<ModalTy> wrapperParamTys;
    std::transform(wrapperParams.begin(), wrapperParams.end(), std::back_inserter(wrapperParamTys),
        [](auto& p) { return p->GetTy(); });

    auto wrapperTy = typeManager.GetFunctionTy(wrapperParamTys, {unitTy}, {.isC = true});

    std::vector<OwnedPtr<FuncParamList>> wrapperParamLists;
    wrapperParamLists.emplace_back(std::move(wrapperParamList));

    auto wrapperBody = CreateFuncBody(std::move(wrapperParamLists), CreateUnitType(field.curFile),
        CreateBlock(std::move(wrapperNodes), wrapperTy->retTy), {wrapperTy});

    // Generate wrapper name from ORIGIN field, not a mirror one.
    auto wrapperName = nameGenerator.GetFieldSetterWrapperName(field);

    auto wrapper = CreateFuncDecl(wrapperName, std::move(wrapperBody), {wrapperTy});
    wrapper->moduleName = field.moduleName;
    wrapper->fullPackageName = field.fullPackageName;
    wrapper->EnableAttr(Attribute::C, Attribute::GLOBAL, Attribute::PUBLIC, Attribute::NO_MANGLE);
    wrapper->funcBody->funcDecl = wrapper.get();

    PutDeclToFile(*wrapper, *curFile);

    return wrapper;
}

OwnedPtr<ThrowExpr> ASTFactory::CreateThrowOptionalMethodUnimplemented(File& file)
{
    auto exceptionDecl = bridge.GetObjCOptionalMethodUnimplementedExceptionDecl();
    CJC_NULLPTR_CHECK(exceptionDecl);
    return CreateThrowException(*exceptionDecl, {}, file, typeManager);
}

OwnedPtr<ThrowExpr> ASTFactory::CreateThrowUnreachableCodeExpr(File& file)
{
    auto exceptionDecl = bridge.GetObjCUnreachableCodeExceptionDecl();
    CJC_NULLPTR_CHECK(exceptionDecl);
    return CreateThrowException(*exceptionDecl, {}, file, typeManager);
}

OwnedPtr<ThrowExpr> ASTFactory::CreateObjCInitException(File& file, ClassLikeDecl& cls)
{
    auto exceptionDecl = bridge.GetObjCInitException();
    CJC_NULLPTR_CHECK(exceptionDecl);

    constexpr auto OBJC_INIT_EXCEPTION_MESSAGE_PART_1 = "Initialization error: expected ";
    constexpr auto OBJC_INIT_EXCEPTION_MESSAGE_PART_2 = ", got nil.";

    auto argTy = GetStringDecl(importManager).GetTy();
    auto className = nameGenerator.GetObjCDeclName(cls);
    auto arg = CreateLitConstExpr(LitConstKind::STRING,
        OBJC_INIT_EXCEPTION_MESSAGE_PART_1 + className + OBJC_INIT_EXCEPTION_MESSAGE_PART_2, argTy);
    std::vector<OwnedPtr<Expr>> args;
    args.emplace_back(std::move(arg));
    return CreateThrowException(*exceptionDecl, std::move(args), file, typeManager);
}

OwnedPtr<ThrowExpr> ASTFactory::CreateThrowStaticMethodCallOnInterfaceExpr(File& file)
{
    auto exceptionDecl = bridge.GetObjCStaticMethodCallOnIntefaceExceptionDecl();
    CJC_NULLPTR_CHECK(exceptionDecl);
    return CreateThrowException(*exceptionDecl, {}, file, typeManager);
}

std::set<Ptr<FuncDecl>> ASTFactory::GetAllParentCtors(ClassDecl& target) const
{
    std::set<Ptr<FuncDecl>> result = {};
    for (auto& it : target.GetAllSuperDecls()) {
        for (OwnedPtr<Decl>& declPtr : it->GetMemberDecls()) {
            if (IsGeneratedMember(*declPtr.get())) {
                continue;
            }

            if (!declPtr->TestAttr(Attribute::CONSTRUCTOR)) {
                continue;
            }

            if (declPtr->astKind != ASTKind::FUNC_DECL) {
                // skip primary ctor, as it is desugared to init already
                continue;
            }

            auto funcDecl = StaticAs<ASTKind::FUNC_DECL>(declPtr.get());
            if (!funcDecl->funcBody) {
                continue;
            }

            result.insert(funcDecl);
        }
    }
    return result;
}

OwnedPtr<FuncDecl> ASTFactory::CreateBaseCtorDecl(ClassDecl& target)
{
<<<<<<< HEAD
    return CreateBaseCtorDecl(target, false);
||||||| constructed merge base
    auto nativeObjCIdTy = bridge.GetNativeObjCIdTy();
    auto param = CreateFuncParam(NATIVE_HANDLE_IDENT, CreateType(nativeObjCIdTy), nullptr, nativeObjCIdTy);

    std::vector<Ptr<Ty>> ctorFuncParamTys;
    ctorFuncParamTys.emplace_back(param->GetTy());
    auto ctorFuncTy = typeManager.GetFunctionTy(std::move(ctorFuncParamTys), target.GetTy());

    std::vector<OwnedPtr<FuncParam>> ctorParams;
    ctorParams.emplace_back(std::move(param));
    auto paramList = CreateFuncParamList(std::move(ctorParams));

    std::vector<OwnedPtr<Node>> ctorNodes;

    std::vector<OwnedPtr<FuncParamList>> paramLists;
    paramLists.emplace_back(std::move(paramList));

    auto ctorFuncBody = CreateFuncBody(
        std::move(paramLists), CreateRefType(target), CreateBlock(std::move(ctorNodes), target.GetTy()), ctorFuncTy);

    auto ctor = CreateFuncDecl(std::string(INIT_IDENT), std::move(ctorFuncBody), ctorFuncTy);
    ctor->funcBody->funcDecl = ctor.get();
    ctor->constructorCall = ConstructorCall::NONE;
    ctor->funcBody->parentClassLike = &target;
    ctor->EnableAttr(Attribute::PUBLIC, Attribute::CONSTRUCTOR);

    PutDeclToClassLikeBody(*ctor, target);

    return ctor;
}

OwnedPtr<FuncDecl> ASTFactory::CreateImplCtor(FuncDecl& from)
{
    auto nativeHandleTy = bridge.GetNativeObjCIdTy();
    auto ctor = ASTCloner::Clone(Ptr(&from));
    auto curFile = ctor->curFile;

    auto& implCtorParams = ctor->funcBody->paramLists[0]->params;

    implCtorParams.insert(implCtorParams.begin(),
        WithinFile(CreateFuncParam(NATIVE_HANDLE_IDENT, CreateType(nativeHandleTy), nullptr, nativeHandleTy), curFile));

    std::vector<Ptr<Ty>> implCtorParamTys;
    std::transform(implCtorParams.begin(), implCtorParams.end(), std::back_inserter(implCtorParamTys),
        [](auto& p) { return p->type->GetTy(); });

    ctor->SetTy(typeManager.GetFunctionTy(implCtorParamTys, ctor->funcBody->retType->GetTy()));
    ctor->funcBody->SetTy(ctor->GetTy());
    ctor->funcBody->funcDecl = ctor.get();
    ctor->constructorCall = ConstructorCall::SUPER;

    return ctor;
}

bool ASTFactory::IsGeneratedMember(const Decl& decl) const
{
    return IsGeneratedNativeHandleField(decl) || IsGeneratedGetObjCClassFunction(decl) ||
        IsGeneratedHasInitedField(decl) || IsGeneratedCtor(decl) || IsGeneratedNativeHandleGetter(decl) ||
        IsObjCGeneratedMember(decl);
=======
    auto nativeObjCIdTy = bridge.GetNativeObjCIdTy();
    auto param = CreateFuncParam(NATIVE_HANDLE_IDENT, CreateType(nativeObjCIdTy), nullptr, {nativeObjCIdTy});

    std::vector<ModalTy> ctorFuncParamTys;
    ctorFuncParamTys.emplace_back(param->GetTy());
    auto ctorFuncTy = typeManager.GetFunctionTy(std::move(ctorFuncParamTys), target.GetTy());

    std::vector<OwnedPtr<FuncParam>> ctorParams;
    ctorParams.emplace_back(std::move(param));
    auto paramList = CreateFuncParamList(std::move(ctorParams));

    std::vector<OwnedPtr<Node>> ctorNodes;

    std::vector<OwnedPtr<FuncParamList>> paramLists;
    paramLists.emplace_back(std::move(paramList));

    auto ctorFuncBody = CreateFuncBody(
        std::move(paramLists), CreateRefType(target), CreateBlock(std::move(ctorNodes), target.GetTy()), {ctorFuncTy});

    auto ctor = CreateFuncDecl(std::string(INIT_IDENT), std::move(ctorFuncBody), {ctorFuncTy});
    ctor->funcBody->funcDecl = ctor.get();
    ctor->constructorCall = ConstructorCall::NONE;
    ctor->funcBody->parentClassLike = &target;
    ctor->EnableAttr(Attribute::PUBLIC, Attribute::CONSTRUCTOR);

    PutDeclToClassLikeBody(*ctor, target);

    return ctor;
}

OwnedPtr<FuncDecl> ASTFactory::CreateImplCtor(FuncDecl& from)
{
    auto nativeHandleTy = bridge.GetNativeObjCIdTy();
    auto ctor = ASTCloner::Clone(Ptr(&from));
    auto curFile = ctor->curFile;

    auto& implCtorParams = ctor->funcBody->paramLists[0]->params;

    implCtorParams.insert(implCtorParams.begin(),
        WithinFile(
            CreateFuncParam(NATIVE_HANDLE_IDENT, CreateType(nativeHandleTy), nullptr, {nativeHandleTy}), curFile));

    std::vector<ModalTy> implCtorParamTys;
    std::transform(implCtorParams.begin(), implCtorParams.end(), std::back_inserter(implCtorParamTys),
        [](auto& p) { return p->type->GetTy(); });

    ctor->SetTy({typeManager.GetFunctionTy(implCtorParamTys, ctor->funcBody->retType->GetTy())});
    ctor->funcBody->SetTy(ctor->GetTy());
    ctor->funcBody->funcDecl = ctor.get();
    ctor->constructorCall = ConstructorCall::SUPER;

    return ctor;
}

bool ASTFactory::IsGeneratedMember(const Decl& decl) const
{
    return IsGeneratedNativeHandleField(decl) || IsGeneratedGetObjCClassFunction(decl) ||
        IsGeneratedHasInitedField(decl) || IsGeneratedCtor(decl) || IsGeneratedNativeHandleGetter(decl) ||
        IsObjCGeneratedMember(decl);
>>>>>>> feat: local mode
}

OwnedPtr<FuncDecl> ASTFactory::CreateObjCMirrorBaseCtorDecl(ClassDecl& target)
{
    return CreateBaseCtorDecl(target, true);
}

OwnedPtr<RefExpr> ASTFactory::CreateNativeObjCIdMarkerRef(Ptr<File> curFile)
{
    return WithinFile(CreateRefExpr(*bridge.GetNativeObjCIdMarkerInstance()), curFile);
}

void ASTFactory::AppendNativeObjCIdMarkerIfNeeded(CallExpr& call, Ptr<File> curFile)
{
    auto ctor = call.resolvedFunction;
    CJC_NULLPTR_CHECK(ctor);
    CJC_ASSERT_WITH_MSG(ctor->TestAttr(Attribute::CONSTRUCTOR), "expected a ctor call");
    if (!HasNativeObjCIdMarkerParam(*ctor)) {
        return;
    }

    call.args.push_back(CreateFuncArg(CreateNativeObjCIdMarkerRef(curFile)));
}

OwnedPtr<FuncDecl> ASTFactory::CreateBaseCtorDecl(ClassDecl& target, bool withMarker)
{
    auto nativeObjCIdTy = bridge.GetNativeObjCIdTy();

    std::vector<OwnedPtr<FuncParam>> ctorParams;
    ctorParams.emplace_back(CreateFuncParam(NATIVE_HANDLE_IDENT, CreateType(nativeObjCIdTy), nullptr,
        nativeObjCIdTy));
    if (withMarker) {
        auto markerTy = bridge.GetNativeObjCIdMarkerTy();
        ctorParams.emplace_back(CreateFuncParam(NATIVE_HANDLE_MARKER, CreateType(markerTy), nullptr, markerTy));
    }

    std::vector<Ptr<Ty>> ctorFuncParamTys;
    std::transform(ctorParams.begin(), ctorParams.end(), std::back_inserter(ctorFuncParamTys),
        [](auto& param) { return param->GetTy(); });
    auto ctorFuncTy = typeManager.GetFunctionTy(std::move(ctorFuncParamTys), target.GetTy());

    std::vector<OwnedPtr<FuncParamList>> paramLists;
    paramLists.emplace_back(CreateFuncParamList(std::move(ctorParams)));

    auto ctorFuncBody = CreateFuncBody(std::move(paramLists), CreateRefType(target),
        CreateBlock(std::vector<OwnedPtr<Node>>{}, target.GetTy()), ctorFuncTy);

    auto ctor = CreateFuncDecl(std::string(INIT_IDENT), std::move(ctorFuncBody), ctorFuncTy);
    ctor->funcBody->funcDecl = ctor.get();
    ctor->constructorCall = ConstructorCall::NONE;
    ctor->funcBody->parentClassLike = &target;
    ctor->EnableAttr(Attribute::PUBLIC, Attribute::CONSTRUCTOR);

    PutDeclToClassLikeBody(*ctor, target);

    return ctor;
}

void ASTFactory::PutDeclToClassLikeBody(Decl& decl, ClassLikeDecl& target)
{
    // use generics?
    switch (target.astKind) {
        case ASTKind::INTERFACE_DECL:
            PutDeclToInterfaceBody(decl, *StaticAs<ASTKind::INTERFACE_DECL>(Ptr(&target)));
            break;
        case ASTKind::CLASS_DECL:
            PutDeclToClassBody(decl, *StaticAs<ASTKind::CLASS_DECL>(Ptr(&target)));
            break;
        default:
            // Not supported
            CJC_ABORT();
    }
    decl.outerDecl = &target;
    decl.curFile = target.curFile;
    decl.fullPackageName = target.fullPackageName;
    decl.EnableAttr(Attribute::IN_CLASSLIKE);
}

void ASTFactory::PutDeclToClassBody(Decl& decl, ClassDecl& target)
{
    decl.begin = target.body->end;
    decl.end = target.body->end;
}

void ASTFactory::PutDeclToInterfaceBody(Decl& decl, InterfaceDecl& target)
{
    decl.begin = target.body->end;
    decl.end = target.body->end;
}

void ASTFactory::PutDeclToFile(Decl& decl, File& target)
{
    decl.curFile = &target;
    decl.begin = target.end;
    decl.end = target.end;
}

OwnedPtr<Expr> ASTFactory::ApplyRetain(OwnedPtr<Expr> expr, Retain withRetain)
{
    switch (withRetain) {
        case Retain::RETAINED:
            return CreateObjCRetainCall(std::move(expr));
        case Retain::RETAIN_AUTORELEASED_RETURN_VALUE:
            return CreateObjCRetainAutoreleasedReturnValueCall(std::move(expr));
        case Retain::UNRETAINED:
        default:
            return expr;
    }
}

OwnedPtr<CallExpr> ASTFactory::CreateObjCImplBaseCtorCall(
    ClassDecl& impl, OwnedPtr<Expr> nativeHandle, OwnedPtr<Expr> registryId, Ptr<File> curFile, Retain withRetain)
{
    auto implBaseCtorDecl = GetObjCImplBaseCtor(impl);
    CJC_ASSERT_WITH_MSG(implBaseCtorDecl, "expected base ctor in the @ObjCImpl class");
    auto handle = ApplyRetain(WithinFile(std::move(nativeHandle), curFile), withRetain);
    auto ctorCall = CreateCall(implBaseCtorDecl, curFile, std::move(handle), std::move(registryId));
    AppendNativeObjCIdMarkerIfNeeded(*ctorCall, curFile);
    return ctorCall;
}

OwnedPtr<CallExpr> ASTFactory::CreatePutToRegistryCall(OwnedPtr<Expr> expr)
{
    auto putToRegistryDecl = bridge.GetPutToRegistryDecl();
    auto putToRegistryExpr = CreateRefExpr(*putToRegistryDecl);

    std::vector<OwnedPtr<FuncArg>> args;
    args.emplace_back(CreateFuncArg(std::move(expr)));

    return CreateCallExpr(std::move(putToRegistryExpr), std::move(args), putToRegistryDecl,
        putToRegistryDecl->funcBody->retType->GetTy(), CallKind::CALL_DECLARED_FUNCTION);
}

OwnedPtr<Expr> ASTFactory::CreateNativeLambdaForBlockType(Ty& ty, Ptr<File> curFile)
{
    const auto fty = DynamicCast<FuncTy>(&ty);
    CJC_NULLPTR_CHECK(fty);

    std::vector<ModalTy> cArgTys;
    cArgTys.emplace_back(typeManager.GetPointerTy(bridge.GetNativeBlockABIDecl()->DataTy()));
    for (const ModalTy& aty : fty->paramTys) {
        cArgTys.emplace_back(typeMapper.Cj2CType(aty.Ty()));
    }
    Ptr<Ty> cResTy = typeMapper.Cj2CType(fty->retTy.Ty());

<<<<<<< HEAD
    auto cFuncTy = typeManager.GetFunctionTy(std::move(cArgTys), cResTy, {.isC = true});
||||||| constructed merge base
    auto cFuncTy = typeManager.GetFunctionTy(
        std::move(cArgTys),
        cResTy,
        { .isC = true }
    );
=======
    auto cFuncTy = typeManager.GetFunctionTy(std::move(cArgTys), {cResTy}, {.isC = true});
>>>>>>> feat: local mode
    std::vector<OwnedPtr<FuncParam>> lambdaParams;
    auto varIndex = 0;
<<<<<<< HEAD
    for (auto cty : cArgTys) {
        lambdaParams.push_back(CreateFuncParam("arg" + std::to_string(varIndex++), nullptr, nullptr, cty));
||||||| constructed merge base
    for (auto cty : cArgTys) {
        lambdaParams.push_back(
            CreateFuncParam(
                "arg" + std::to_string(varIndex++), nullptr, nullptr, cty));
=======
    for (const ModalTy& m : cArgTys) {
        lambdaParams.push_back(CreateFuncParam("arg" + std::to_string(varIndex++), nullptr, nullptr, m));
>>>>>>> feat: local mode
    }
    auto getLambdaFromBlockDecl = bridge.GetObjCGetLambdaFromBlockDecl();
    auto getLambdaFromBlockRef = WithinFile(CreateRefExpr(*getLambdaFromBlockDecl), curFile);
<<<<<<< HEAD
    getLambdaFromBlockRef->instTys = {&ty};
    getLambdaFromBlockRef->SetTy(
        typeManager.GetFunctionTy(StaticCast<FuncTy>(getLambdaFromBlockRef->GetTy())->paramTys, &ty));
||||||| constructed merge base
    getLambdaFromBlockRef->instTys = { &ty };
    getLambdaFromBlockRef->SetTy(
        typeManager.GetFunctionTy(StaticCast<FuncTy>(getLambdaFromBlockRef->GetTy())->paramTys, &ty));
=======
    getLambdaFromBlockRef->instTys = { &ty };
    auto refFnTy = DynamicCast<FuncTy>(getLambdaFromBlockRef->DataTy());
    CJC_NULLPTR_CHECK(refFnTy);
    getLambdaFromBlockRef->SetTy({typeManager.GetFunctionTy(refFnTy->paramTys, {Ptr(&ty)})});
>>>>>>> feat: local mode
    std::vector<OwnedPtr<FuncArg>> getLambdaFromBlockArgs;
    getLambdaFromBlockArgs.push_back(CreateFuncArg(WithinFile(CreateRefExpr(*lambdaParams[0]), curFile)));
<<<<<<< HEAD
    auto cangjieFuncExpr = CreateCallExpr(
        std::move(getLambdaFromBlockRef), std::move(getLambdaFromBlockArgs), getLambdaFromBlockDecl, &ty);
||||||| constructed merge base
    auto cangjieFuncExpr = CreateCallExpr(
        std::move(getLambdaFromBlockRef),
        std::move(getLambdaFromBlockArgs),
        getLambdaFromBlockDecl, &ty);
=======
    auto cangjieFuncExpr = CreateCallExpr(std::move(getLambdaFromBlockRef), std::move(getLambdaFromBlockArgs),
        getLambdaFromBlockDecl, {Ptr(&ty)}, CallKind::CALL_DECLARED_FUNCTION);
>>>>>>> feat: local mode
    std::vector<OwnedPtr<FuncArg>> cangjieFuncArgs;
    for (size_t i = 1; i < lambdaParams.size(); ++i) {
<<<<<<< HEAD
        cangjieFuncArgs.push_back(CreateFuncArg(
            WrapEntity(WithinFile(CreateRefExpr(*lambdaParams[i]), curFile), *fty->paramTys[i - 1], Retain::RETAINED)));
||||||| constructed merge base
        cangjieFuncArgs.push_back(
            CreateFuncArg(
                WrapEntity(
                    WithinFile(CreateRefExpr(*lambdaParams[i]), curFile),
                    *fty->paramTys[i - 1], Retain::RETAINED)));
=======
        cangjieFuncArgs.push_back(CreateFuncArg(
            WrapEntity(WithinFile(CreateRefExpr(*lambdaParams[i]), curFile), *fty->paramTys[i - 1].Ty())));
>>>>>>> feat: local mode
    }
<<<<<<< HEAD
    auto resultCangjie = CreateCallExpr(std::move(cangjieFuncExpr), std::move(cangjieFuncArgs), nullptr, cResTy);
||||||| constructed merge base
    auto resultCangjie = CreateCallExpr(
        std::move(cangjieFuncExpr),
        std::move(cangjieFuncArgs),
        nullptr, cResTy);
=======
    auto resultCangjie = CreateCallExpr(
        std::move(cangjieFuncExpr), std::move(cangjieFuncArgs), nullptr, {cResTy}, CallKind::CALL_DECLARED_FUNCTION);
>>>>>>> feat: local mode
    std::vector<OwnedPtr<Node>> body;
<<<<<<< HEAD
    auto unwrappedResultCj = UnwrapEntity(std::move(resultCangjie));
    body.push_back(std::move(unwrappedResultCj));
    auto lambda = WrapReturningLambdaExpr(typeManager, std::move(body), std::move(lambdaParams));
    lambda->SetTy(cFuncTy);
||||||| constructed merge base
    auto unwrappedResultCj = UnwrapEntity(std::move(resultCangjie));
    body.push_back(std::move(unwrappedResultCj));
    auto lambda = WrapReturningLambdaExpr(
        typeManager,
        std::move(body),
        std::move(lambdaParams)
    );
    lambda->SetTy(cFuncTy);
=======
    body.push_back(UnwrapEntity(std::move(resultCangjie)));
    auto lambda = WrapReturningLambdaExpr(typeManager, std::move(body), std::move(lambdaParams));
    lambda->SetTy({cFuncTy});
>>>>>>> feat: local mode

    return lambda;
}

OwnedPtr<Expr> ASTFactory::CreateObjCBlockFromLambdaCall(OwnedPtr<Expr> funcExpr)
{
    auto curFile = funcExpr->curFile;
    auto funcTy = DynamicCast<FuncTy>(funcExpr->DataTy());
    CJC_NULLPTR_CHECK(funcTy);
    auto creatorFunc = bridge.GetObjCStoreLambdaAsBlockDecl();
    auto cfuncLambda = CreateNativeLambdaForBlockType(*funcTy, curFile);
    std::vector<OwnedPtr<FuncArg>> creatorFuncArgs;
    creatorFuncArgs.push_back(CreateFuncArg(std::move(funcExpr), "", {typeManager.GetAnyTy()}));
    auto nativeAbiErasedFuncTy = typeManager.GetFunctionTy(
<<<<<<< HEAD
        {bridge.GetNativeBlockABIDecl()->GetTy()}, typeManager.GetPrimitiveTy(TypeKind::TYPE_UNIT), {.isC = true});
    creatorFuncArgs.push_back(CreateFuncArg(CreateUnsafePointerCast(std::move(cfuncLambda), nativeAbiErasedFuncTy)));
    auto pointerToAbiTy = typeManager.GetPointerTy(bridge.GetCangjieBlockABIDecl()->GetTy());
||||||| constructed merge base
        {bridge.GetNativeBlockABIDecl()->GetTy()}, typeManager.GetPrimitiveTy(TypeKind::TYPE_UNIT), {.isC = true});
    creatorFuncArgs.push_back(CreateFuncArg(
        CreateUnsafePointerCast(
            std::move(cfuncLambda), nativeAbiErasedFuncTy)));
    auto pointerToAbiTy = typeManager.GetPointerTy(bridge.GetCangjieBlockABIDecl()->GetTy());
=======
        {bridge.GetNativeBlockABIDecl()->GetTy()}, {typeManager.GetPrimitiveTy(TypeKind::TYPE_UNIT)}, {.isC = true});
    creatorFuncArgs.push_back(CreateFuncArg(
        CreateUnsafePointerCast(
            std::move(cfuncLambda), nativeAbiErasedFuncTy)));
    auto pointerToAbiTy = typeManager.GetPointerTy(bridge.GetCangjieBlockABIDecl()->DataTy());
>>>>>>> feat: local mode
    auto objcBlockDecl = bridge.GetObjCBlockDecl();
    CJC_NULLPTR_CHECK(objcBlockDecl);
    auto objcBlockTy =
        typeManager.GetInstantiatedTy(objcBlockDecl->GetTy(), GenerateTypeMapping(*objcBlockDecl, {funcTy}));

<<<<<<< HEAD
    auto nativeBlockExpr = CreateCallExpr(WithinFile(CreateRefExpr(*creatorFunc), curFile), std::move(creatorFuncArgs),
        creatorFunc, pointerToAbiTy, CallKind::CALL_DECLARED_FUNCTION);
||||||| constructed merge base
    auto nativeBlockExpr = CreateCallExpr(
        WithinFile(CreateRefExpr(*creatorFunc), curFile),
        std::move(creatorFuncArgs),
        creatorFunc,
        pointerToAbiTy,
        CallKind::CALL_DECLARED_FUNCTION);
=======
    auto nativeBlockExpr = CreateCallExpr(WithinFile(CreateRefExpr(*creatorFunc), curFile), std::move(creatorFuncArgs),
        creatorFunc, {pointerToAbiTy}, CallKind::CALL_DECLARED_FUNCTION);
>>>>>>> feat: local mode
    auto blockConstructor = bridge.GetObjCBlockConstructorFromCangjie();
    auto blockConstructorRef = WithinFile(CreateRefExpr(*blockConstructor), curFile);
    blockConstructorRef->instTys = {funcTy};
    blockConstructorRef->SetTy({typeManager.GetFunctionTy({{pointerToAbiTy}}, {objcBlockTy})});
    std::vector<OwnedPtr<FuncArg>> constructorArgs;
    constructorArgs.push_back(CreateFuncArg(std::move(nativeBlockExpr)));
<<<<<<< HEAD
    auto result = CreateCallExpr(std::move(blockConstructorRef), std::move(constructorArgs), blockConstructor,
        objcBlockTy, CallKind::CALL_OBJECT_CREATION);
||||||| constructed merge base
    auto result = CreateCallExpr(
        std::move(blockConstructorRef),
        std::move(constructorArgs),
        blockConstructor,
        objcBlockTy,
        CallKind::CALL_OBJECT_CREATION);
=======
    auto result = CreateCallExpr(std::move(blockConstructorRef), std::move(constructorArgs), blockConstructor,
        {objcBlockTy}, CallKind::CALL_OBJECT_CREATION);
>>>>>>> feat: local mode

    return result;
}

<<<<<<< HEAD
OwnedPtr<CallExpr> ASTFactory::CreateGetFromRegistryByIdCall(
    OwnedPtr<Expr> registryId, OwnedPtr<Type> typeArg)
||||||| constructed merge base
OwnedPtr<CallExpr> ASTFactory::CreateGetFromRegistryByNativeHandleCall(
    OwnedPtr<Expr> nativeHandle, OwnedPtr<Type> typeArg)
{
    CJC_ASSERT(nativeHandle->GetTy()->IsPointer());
    auto getFromRegistryByNativeHandleDecl = bridge.GetGetFromRegistryByNativeHandleDecl();
    auto getFromRegistryNativeHandleExpr = CreateRefExpr(*getFromRegistryByNativeHandleDecl);

    auto ty = typeArg->GetTy();
    CJC_ASSERT(TypeMapper::IsObjCImpl(*ty));
    std::vector<OwnedPtr<FuncArg>> args;
    args.emplace_back(CreateFuncArg(std::move(nativeHandle)));

    getFromRegistryNativeHandleExpr->instTys.emplace_back(ty);
    getFromRegistryNativeHandleExpr->SetTy(typeManager.GetInstantiatedTy(getFromRegistryByNativeHandleDecl->GetTy(),
        GenerateTypeMapping(*getFromRegistryByNativeHandleDecl, getFromRegistryNativeHandleExpr->instTys)));
    getFromRegistryNativeHandleExpr->typeArguments.emplace_back(std::move(typeArg));

    return CreateCallExpr(std::move(getFromRegistryNativeHandleExpr), std::move(args),
        getFromRegistryByNativeHandleDecl, ty, CallKind::CALL_DECLARED_FUNCTION);
}

OwnedPtr<CallExpr> ASTFactory::CreateGetFromRegistryByIdCall(OwnedPtr<Expr> registryId, OwnedPtr<Type> typeArg)
=======
OwnedPtr<CallExpr> ASTFactory::CreateGetFromRegistryByNativeHandleCall(
    OwnedPtr<Expr> nativeHandle, OwnedPtr<Type> typeArg)
{
    CJC_ASSERT(nativeHandle->DataTy()->IsPointer());
    auto getFromRegistryByNativeHandleDecl = bridge.GetGetFromRegistryByNativeHandleDecl();
    auto getFromRegistryNativeHandleExpr = CreateRefExpr(*getFromRegistryByNativeHandleDecl);

    auto ty = typeArg->GetTy();
    CJC_ASSERT(TypeMapper::IsObjCImpl(*ty));
    std::vector<OwnedPtr<FuncArg>> args;
    args.emplace_back(CreateFuncArg(std::move(nativeHandle)));

    getFromRegistryNativeHandleExpr->instTys.emplace_back(ty.Ty());
    getFromRegistryNativeHandleExpr->SetTy(typeManager.GetInstantiatedTy(getFromRegistryByNativeHandleDecl->GetTy(),
        GenerateTypeMapping(*getFromRegistryByNativeHandleDecl, getFromRegistryNativeHandleExpr->instTys)));
    getFromRegistryNativeHandleExpr->typeArguments.emplace_back(std::move(typeArg));

    return CreateCallExpr(std::move(getFromRegistryNativeHandleExpr), std::move(args),
        getFromRegistryByNativeHandleDecl, ty, CallKind::CALL_DECLARED_FUNCTION);
}

OwnedPtr<CallExpr> ASTFactory::CreateGetFromRegistryByIdCall(OwnedPtr<Expr> registryId, OwnedPtr<Type> typeArg)
>>>>>>> feat: local mode
{
    auto getFromRegistryByIdDecl = bridge.GetGetFromRegistryByIdDecl();
    auto getFromRegistryByIdExpr = CreateRefExpr(*getFromRegistryByIdDecl);

    auto ty = typeArg->GetTy();
    CJC_ASSERT(IsObjCExport(*ty));

    std::vector<OwnedPtr<FuncArg>> args;
    args.emplace_back(CreateFuncArg(std::move(registryId)));

    getFromRegistryByIdExpr->instTys.emplace_back(ty.Ty());
    getFromRegistryByIdExpr->SetTy(typeManager.GetInstantiatedTy(getFromRegistryByIdDecl->GetTy(),
        GenerateTypeMapping(*getFromRegistryByIdDecl, getFromRegistryByIdExpr->instTys)));
    getFromRegistryByIdExpr->typeArguments.emplace_back(std::move(typeArg));

    return CreateCallExpr(std::move(getFromRegistryByIdExpr), std::move(args), getFromRegistryByIdDecl, ty,
        CallKind::CALL_DECLARED_FUNCTION);
}

OwnedPtr<CallExpr> ASTFactory::CreateRemoveFromRegistryCall(OwnedPtr<Expr> registryId)
{
    auto removeFromRegistryDecl = bridge.GetRemoveFromRegistryDecl();
    auto removeFromRegistryExpr = CreateRefExpr(*removeFromRegistryDecl);

    std::vector<OwnedPtr<FuncArg>> args;
    args.emplace_back(CreateFuncArg(std::move(registryId)));

    return CreateCallExpr(std::move(removeFromRegistryExpr), std::move(args), removeFromRegistryDecl,
        removeFromRegistryDecl->funcBody->retType->GetTy(), CallKind::CALL_DECLARED_FUNCTION);
}

OwnedPtr<Expr> ASTFactory::CreateObjCReleaseCall(OwnedPtr<Expr> nativeHandle)
{
    auto releaseDecl = bridge.GetObjCReleaseDecl();
    auto releaseExpr = CreateRefExpr(*releaseDecl);

    std::vector<OwnedPtr<FuncArg>> args;
    args.emplace_back(CreateFuncArg(std::move(nativeHandle)));
    return CreateCallExpr(std::move(releaseExpr), std::move(args), nullptr,
        {TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT)}, CallKind::CALL_DECLARED_FUNCTION);
}

OwnedPtr<Expr> ASTFactory::CreateObjCIsKindOfClassCall(OwnedPtr<Expr> id, OwnedPtr<Expr> cls, Ptr<File> file)
{
    auto kindOfClassDecl = bridge.GetObjCIsKindOfClassDecl();
    auto kindOfClassExpr = CreateRefExpr(*kindOfClassDecl);

    std::vector<OwnedPtr<FuncArg>> args;
    args.emplace_back(CreateFuncArg(std::move(id)));
    args.emplace_back(CreateFuncArg(std::move(cls)));
<<<<<<< HEAD
    auto ret = CreateCallExpr(std::move(kindOfClassExpr), std::move(args), kindOfClassDecl, typeManager.GetBoolTy(),
        CallKind::CALL_DECLARED_FUNCTION);
    ret->curFile = file;
    return ret;
||||||| constructed merge base
    return WithinFile(CreateCallExpr(std::move(kindOfClassExpr), std::move(args), kindOfClassDecl,
        typeManager.GetBoolTy(), CallKind::CALL_DECLARED_FUNCTION), file);
=======
    return WithinFile(CreateCallExpr(std::move(kindOfClassExpr), std::move(args), kindOfClassDecl,
        {typeManager.GetBoolTy()}, CallKind::CALL_DECLARED_FUNCTION),
        file);
>>>>>>> feat: local mode
}

OwnedPtr<Expr> ASTFactory::CreateObjCConformsToProtocolCall(
    OwnedPtr<Expr> id, OwnedPtr<Expr> cls, Ptr<File> file)
{
    auto conformsToProtocolDecl = bridge.GetObjCConformsToProtocolDecl();
    auto conformsToProtocolExpr = CreateRefExpr(*conformsToProtocolDecl);

    std::vector<OwnedPtr<FuncArg>> args;
    args.emplace_back(CreateFuncArg(std::move(id)));
    args.emplace_back(CreateFuncArg(std::move(cls)));
<<<<<<< HEAD
    auto ret = CreateCallExpr(std::move(conformsToProtocolExpr), std::move(args), conformsToProtocolDecl,
        typeManager.GetBoolTy(), CallKind::CALL_DECLARED_FUNCTION);
    ret->curFile = file;
    return ret;
||||||| constructed merge base
    return WithinFile(CreateCallExpr(std::move(conformsToProtocolExpr), std::move(args), conformsToProtocolDecl,
        typeManager.GetBoolTy(), CallKind::CALL_DECLARED_FUNCTION), file);
=======
    return WithinFile(CreateCallExpr(std::move(conformsToProtocolExpr), std::move(args), conformsToProtocolDecl,
        {typeManager.GetBoolTy()}, CallKind::CALL_DECLARED_FUNCTION),
        file);
>>>>>>> feat: local mode
}

OwnedPtr<Expr> ASTFactory::CreateObjCRespondsToSelectorCall(
    OwnedPtr<Expr> cls, OwnedPtr<Expr> sel, Ptr<File> file)
{
    auto responseToSelDecl = bridge.GetObjCRespondsToSelectorDecl();
    auto responseToSelExpr = CreateRefExpr(*responseToSelDecl);

    std::vector<OwnedPtr<FuncArg>> args;
    args.emplace_back(CreateFuncArg(std::move(cls)));
    args.emplace_back(CreateFuncArg(std::move(sel)));
<<<<<<< HEAD
    auto ret = CreateCallExpr(std::move(responseToSelExpr), std::move(args), responseToSelDecl, typeManager.GetBoolTy(),
        CallKind::CALL_DECLARED_FUNCTION);
    ret->curFile = file;
    return ret;
||||||| constructed merge base
    return WithinFile(CreateCallExpr(std::move(responseToSelExpr), std::move(args), responseToSelDecl,
        typeManager.GetBoolTy(), CallKind::CALL_DECLARED_FUNCTION), file);
=======
    return WithinFile(CreateCallExpr(std::move(responseToSelExpr), std::move(args), responseToSelDecl,
        {typeManager.GetBoolTy()}, CallKind::CALL_DECLARED_FUNCTION),
        file);
>>>>>>> feat: local mode
}

OwnedPtr<Expr> ASTFactory::CreateGetSuperClassExpr(OwnedPtr<Expr> objCSuper, Ptr<File> file)
{
    auto getSuperClassDecl = bridge.GetGetSuperClassDecl();
    auto getSuperClassExpr = CreateRefExpr(*getSuperClassDecl);

    std::vector<OwnedPtr<FuncArg>> args;
    args.emplace_back(CreateFuncArg(std::move(objCSuper)));
<<<<<<< HEAD
    auto ret = CreateCallExpr(std::move(getSuperClassExpr), std::move(args), getSuperClassDecl,
        bridge.GetNativeObjCClassTy(), CallKind::CALL_DECLARED_FUNCTION);
    ret->curFile = file;
    return ret;
||||||| constructed merge base
    return WithinFile(CreateCallExpr(std::move(getSuperClassExpr), std::move(args), getSuperClassDecl,
        bridge.GetNativeObjCClassTy(), CallKind::CALL_DECLARED_FUNCTION), file);
=======
    return WithinFile(CreateCallExpr(std::move(getSuperClassExpr), std::move(args), getSuperClassDecl,
        {bridge.GetNativeObjCClassTy()}, CallKind::CALL_DECLARED_FUNCTION),
        file);
>>>>>>> feat: local mode
}

OwnedPtr<Expr> ASTFactory::CreateWithObjCSuperScope(OwnedPtr<Expr> nativeHandle, ClassDecl& outerDecl,
    Ptr<Ty> retTy, std::function<std::vector<OwnedPtr<Node>>(OwnedPtr<Expr>, OwnedPtr<Expr>)> bodyFactory)
{
    CJC_ASSERT(IsObjCCompatible(*retTy, TypePosition::OUT));

    auto withObjCSuperDecl = bridge.GetWithObjCSuperDecl();
    auto unwrappedTy = typeMapper.Cj2CType(retTy);
    auto withObjCSuperRef = CreateRefExpr(*withObjCSuperDecl);
    withObjCSuperRef->instTys.emplace_back(unwrappedTy);
    withObjCSuperRef->SetTy(typeManager.GetInstantiatedTy(
        withObjCSuperDecl->GetTy(), GenerateTypeMapping(*withObjCSuperDecl, withObjCSuperRef->instTys)));
    withObjCSuperRef->typeArguments.emplace_back(CreateType(unwrappedTy));
    auto receiverParam =
        WithinFile(CreateFuncParam("receiver", nullptr, nullptr, {bridge.GetNativeObjCIdTy()}), nativeHandle->curFile);
    auto receiverRef = WithinFile(CreateRefExpr(*receiverParam), nativeHandle->curFile);

    auto objCSuperParam = WithinFile(
        CreateFuncParam("objCSuper", nullptr, nullptr, {bridge.GetNativeObjCSuperPtrTy()}), nativeHandle->curFile);
    auto objCSuperRef = WithinFile(CreateRefExpr(*objCSuperParam), nativeHandle->curFile);

    auto actionParams = Nodes<FuncParam>(std::move(receiverParam), std::move(objCSuperParam));
    auto objcname = nameGenerator.GetObjCDeclName(outerDecl);
    auto classNameExpr = CreateLitConstExpr(LitConstKind::STRING, objcname, GetStringDecl(importManager).GetTy());
    auto args = Nodes<FuncArg>(CreateFuncArg(std::move(nativeHandle)), CreateFuncArg(std::move(classNameExpr)),
        CreateFuncArg(WrapReturningLambdaExpr(
            typeManager, bodyFactory(std::move(receiverRef), std::move(objCSuperRef)), std::move(actionParams))));

    auto withObjCSuperFnTy = DynamicCast<FuncTy>(withObjCSuperRef->DataTy());
    CJC_NULLPTR_CHECK(withObjCSuperFnTy);
    ModalTy realRetTy = withObjCSuperFnTy->retTy;
    return CreateCallExpr(
        std::move(withObjCSuperRef), std::move(args), withObjCSuperDecl, realRetTy, CallKind::CALL_DECLARED_FUNCTION);
}

OwnedPtr<CallExpr> ASTFactory::CreateObjCMsgSendCall(
    Ptr<FuncTy> ty, OwnedPtr<FuncType> funcType, std::vector<OwnedPtr<Expr>> funcArgs)
{
    auto msgSendDecl = bridge.GetObjCMsgSendDecl();
    auto msgSendExpr = CreateCallExpr(CreateRefExpr(*msgSendDecl), {}, nullptr, msgSendDecl->funcBody->retType->GetTy(),
        CallKind::CALL_DECLARED_FUNCTION);
    auto retType = funcType->retType.get();

    auto cFuncDecl = importManager.GetCoreDecl<BuiltInDecl>(std::string(CFUNC_NAME));
    CJC_NULLPTR_CHECK(cFuncDecl);
    auto cFuncRefExpr = CreateRefExpr(*cFuncDecl);

    cFuncRefExpr->SetTy({ty});
    cFuncRefExpr->typeArguments.emplace_back(std::move(funcType));

    // CFunc<...>(msgSend)

    auto cFuncCallExpr = CreateCallExpr(std::move(cFuncRefExpr), Nodes<FuncArg>(CreateFuncArg(std::move(msgSendExpr))),
        nullptr, {ty}, CallKind::CALL_FUNCTION_PTR);

    std::vector<OwnedPtr<FuncArg>> msgSendCallArgs;
    std::transform(funcArgs.begin(), funcArgs.end(), std::back_inserter(msgSendCallArgs),
        [](auto&& argExpr) { return CreateFuncArg(std::move(argExpr)); });

    // CFunc<...>(msgSend)(...)
    return CreateCallExpr(
        std::move(cFuncCallExpr), std::move(msgSendCallArgs), nullptr, retType->GetTy(), CallKind::CALL_FUNCTION_PTR);
}

OwnedPtr<CallExpr> ASTFactory::CreateObjCMsgSendCall(
    OwnedPtr<Expr> nativeHandle, const std::string& selector, Ptr<Ty> retTy, std::vector<OwnedPtr<Expr>> args)
{
    auto selectorCall = CreateGetCachedSelectorAccess(selector, nativeHandle->curFile);

    auto ft = MakeOwned<FuncType>();
    ft->retType = CreateType(retTy);

    args.insert(args.begin(), std::move(selectorCall));
    args.insert(args.begin(), std::move(nativeHandle));
    std::vector<ModalTy> paramTys;
    for (auto& param : args) {
        ft->paramTypes.emplace_back(CreateType(param->DataTy()));
        paramTys.emplace_back(param->GetTy());
    }

    auto fty = typeManager.GetFunctionTy(paramTys, {retTy}, {.isC = true});
    ft->SetTy({fty});

    return CreateObjCMsgSendCall(fty, std::move(ft), std::move(args));
}

OwnedPtr<Expr> ASTFactory::CreateGetClassCall(const ClassLikeDecl& cls, Ptr<File> curFile)
{
    auto cnameFunc =
        GetMemberDecl<ASTKind::FUNC_DECL>(cls, [](auto&& decl) { return decl.identifier == GET_OBJ_C_CLASS_IDENT; });
    auto cnameRefExpr = CreateRefExpr(*cnameFunc);
    auto cnameCall = CreateCall(cnameFunc, curFile);
    return cnameCall;
}

OwnedPtr<Expr> ASTFactory::CreateGetClassCall(std::string& className, Ptr<File> curFile)
{
    auto getClassFuncDecl = bridge.GetGetClassDecl();
    auto getClassExpr = CreateRefExpr(*getClassFuncDecl);

    auto cnameAsLit = CreateLitConstExpr(LitConstKind::STRING, className, GetStringDecl(importManager).GetTy());
    return CreateCall(getClassFuncDecl, curFile, std::move(cnameAsLit));
}

OwnedPtr<Expr> ASTFactory::CreateGetProtoCall(std::string& protoName, Ptr<File> curFile)
{
    auto getClassFuncDecl = bridge.GetGetProtoDecl();
    auto getClassExpr = CreateRefExpr(*getClassFuncDecl);

    auto cnameAsLit = CreateLitConstExpr(LitConstKind::STRING, protoName, GetStringDecl(importManager).GetTy());
    return CreateCall(getClassFuncDecl, curFile, std::move(cnameAsLit));
}

OwnedPtr<CallExpr> ASTFactory::CreateRegisterNameCall(OwnedPtr<Expr> selectorExpr)
{
    auto registerNameDecl = bridge.GetRegisterNameDecl();
    auto registerNameExpr = CreateRefExpr(*registerNameDecl);

    auto curFile = selectorExpr->curFile;
    return CreateCall(registerNameDecl, curFile, std::move(selectorExpr));
}

OwnedPtr<CallExpr> ASTFactory::CreateRegisterNameCall(const std::string& selector, Ptr<File> curFile)
{
    auto strTy = GetStringDecl(importManager).GetTy();
    auto selectorAsLit = CreateLitConstExpr(LitConstKind::STRING, selector, strTy);
    selectorAsLit->curFile = curFile;

    return CreateRegisterNameCall(std::move(selectorAsLit));
}

OwnedPtr<CallExpr> ASTFactory::CreateAllocCall(OwnedPtr<Expr> nativeClassExpr)
{
    auto allocDecl = bridge.GetAllocDecl();
    auto allocExpr = CreateRefExpr(*allocDecl);

    auto curFile = nativeClassExpr->curFile;
    return CreateCall(allocDecl, curFile, std::move(nativeClassExpr));
}

OwnedPtr<CallExpr> ASTFactory::CreateAllocCall(Decl& decl, Ptr<File> curFile)
{
    auto objcname = nameGenerator.GetObjCDeclName(decl);
    return CreateAllocCall(WithinFile(CreateGetCachedClassAccess(objcname, curFile), curFile));
}

OwnedPtr<Expr> ASTFactory::CreateMethodCallViaMsgSend(
    FuncDecl& fd, OwnedPtr<Expr> nativeHandle, std::vector<OwnedPtr<Expr>> rawArgs)
{
    auto objcname = nameGenerator.GetObjCDeclName(fd);
    auto fdFnTy = DynamicCast<FuncTy>(fd.DataTy());
    CJC_NULLPTR_CHECK(fdFnTy);
    return CreateObjCMsgSendCall(
        std::move(nativeHandle), objcname, typeMapper.Cj2CType(fdFnTy->retTy.Ty()), std::move(rawArgs));
}

OwnedPtr<Expr> ASTFactory::CreateMethodCallViaMsgSend(FuncDecl& fd, OwnedPtr<Expr> handle)
{
    CJC_NULLPTR_CHECK(fd.outerDecl);
    CJC_ASSERT(IsObjCCompatible(*fd.outerDecl->GetTy()));
    CJC_NULLPTR_CHECK(fd.funcBody);
    CJC_ASSERT(!fd.funcBody->paramLists.empty());

    auto curFile = handle->curFile;
    auto& params = fd.funcBody->paramLists[0]->params;

    std::vector<OwnedPtr<Expr>> args;
    std::transform(params.begin(), params.end(), std::back_inserter(args), [this, curFile](auto& param) {
        auto unwrapped = UnwrapEntity(WithinFile(CreateRefExpr(*param), curFile));
        return unwrapped;
    });

    return CreateMethodCallViaMsgSend(fd, std::move(handle), std::move(args));
}

OwnedPtr<Expr> ASTFactory::CreateAllocInitCall(FuncDecl& fd)
{
    // object instantiation is allowed only for:
    // - init constructor
    // - @ObjCInit method
    CJC_ASSERT(fd.TestAttr(Attribute::CONSTRUCTOR) || IsObjCInitMethod(fd));
    CJC_NULLPTR_CHECK(fd.outerDecl);
    CJC_ASSERT(fd.outerDecl->astKind == ASTKind::CLASS_DECL);
    auto& mirror = *StaticAs<ASTKind::CLASS_DECL>(fd.outerDecl);
    auto curFile = mirror.curFile;

    auto allocCall = CreateAllocCall(mirror, curFile);
    return WithinFile(CreateMethodCallViaMsgSend(fd, std::move(allocCall)), curFile);
}

OwnedPtr<Expr> ASTFactory::CreatePropGetterCallViaMsgSend(PropDecl& pd, OwnedPtr<Expr> nativeHandle)
{
    auto objcname = nameGenerator.GetObjCGetterName(pd);
    return CreateObjCMsgSendCall(std::move(nativeHandle), objcname, typeMapper.Cj2CType(pd.DataTy()), {});
}

OwnedPtr<Expr> ASTFactory::CreatePropSetterCallViaMsgSend(
    PropDecl& pd, OwnedPtr<Expr> nativeHandle, OwnedPtr<Expr> arg)
{
    auto objcname = nameGenerator.GetObjCSetterName(pd);
    return CreateObjCMsgSendCall(
        std::move(nativeHandle), objcname, typeMapper.Cj2CType(pd.DataTy()), Nodes<Expr>(std::move(arg)));
}

OwnedPtr<Expr> ASTFactory::CreateFuncCallViaOpaquePointer(
    OwnedPtr<Expr> ptr, Ptr<Ty> retTy, std::vector<OwnedPtr<Expr>> args)
{
    std::vector<ModalTy> argTys;
    for (auto& rawArg : args) {
        argTys.push_back(rawArg->GetTy());
    }
<<<<<<< HEAD
    auto ty = typeManager.GetFunctionTy(argTys, retTy, {.isC = true});
||||||| constructed merge base
    auto ty = typeManager.GetFunctionTy(
        argTys,
        retTy,
        { .isC = true }
    );
=======
    auto ty = typeManager.GetFunctionTy(std::move(argTys), {retTy}, {.isC = true});
>>>>>>> feat: local mode
    auto cFuncDecl = importManager.GetCoreDecl<BuiltInDecl>(std::string(CFUNC_NAME));
    CJC_NULLPTR_CHECK(cFuncDecl);
    auto cFuncRefExpr = CreateRefExpr(*cFuncDecl);

    cFuncRefExpr->SetTy({ty});
    cFuncRefExpr->instTys.emplace_back(ty);

    // CFunc<...>(ptr)

<<<<<<< HEAD
    auto cFuncCallExpr = CreateCallExpr(std::move(cFuncRefExpr), Nodes<FuncArg>(CreateFuncArg(std::move(ptr))), nullptr,
        ty, CallKind::CALL_FUNCTION_PTR);
||||||| constructed merge base
    auto cFuncCallExpr = CreateCallExpr(std::move(cFuncRefExpr), Nodes<FuncArg>(CreateFuncArg(std::move(ptr))),
        nullptr, ty, CallKind::CALL_FUNCTION_PTR);
=======
    auto cFuncCallExpr = CreateCallExpr(std::move(cFuncRefExpr), Nodes<FuncArg>(CreateFuncArg(std::move(ptr))), nullptr,
        {ty}, CallKind::CALL_FUNCTION_PTR);
>>>>>>> feat: local mode

    std::vector<OwnedPtr<FuncArg>> actualArgs;
    std::transform(args.begin(), args.end(), std::back_inserter(actualArgs),
        [](auto&& argExpr) { return CreateFuncArg(std::move(argExpr)); });

    // CFunc<...>(ptr)(...)
<<<<<<< HEAD
    return CreateCallExpr(std::move(cFuncCallExpr), std::move(actualArgs), nullptr, retTy, CallKind::CALL_FUNCTION_PTR);
||||||| constructed merge base
    return CreateCallExpr(
        std::move(cFuncCallExpr), std::move(actualArgs), nullptr, retTy, CallKind::CALL_FUNCTION_PTR);
=======
    return CreateCallExpr(
        std::move(cFuncCallExpr), std::move(actualArgs), nullptr, {retTy}, CallKind::CALL_FUNCTION_PTR);
>>>>>>> feat: local mode
}

OwnedPtr<Expr> ASTFactory::CreateAutoreleasePoolScope(Ptr<Ty> ty, std::vector<OwnedPtr<Node>> actions)
{
    CJC_ASSERT(IsObjCCompatible(*ty));
    CJC_ASSERT(!actions.empty());

    Ptr<FuncDecl> arpdecl;
    OwnedPtr<RefExpr> arpref;
    if (IsObjCObjectType(*ty) || IsObjCBlock(*ty)) {
        arpdecl = bridge.GetWithAutoreleasePoolObjDecl();
        arpref = CreateRefExpr(*arpdecl);
    } else {
        arpdecl = bridge.GetWithAutoreleasePoolDecl();
        auto unwrappedTy = typeMapper.Cj2CType(ty);
        arpref = CreateRefExpr(*arpdecl);
        arpref->instTys.emplace_back(unwrappedTy);
        arpref->SetTy(typeManager.GetInstantiatedTy(arpdecl->GetTy(), GenerateTypeMapping(*arpdecl, arpref->instTys)));
        arpref->typeArguments.emplace_back(CreateType(unwrappedTy));
    }

    auto args = Nodes<FuncArg>(CreateFuncArg(WrapReturningLambdaExpr(typeManager, std::move(actions))));

    auto arpFnTy = DynamicCast<FuncTy>(arpref->DataTy());
    CJC_NULLPTR_CHECK(arpFnTy);
    ModalTy retTy = arpFnTy->retTy;
    return CreateCallExpr(std::move(arpref), std::move(args), arpdecl, retTy, CallKind::CALL_DECLARED_FUNCTION);
}

OwnedPtr<CallExpr> ASTFactory::CreateGetInstanceVariableCall(
    const PropDecl& field, OwnedPtr<Expr> nativeHandle)
{
    Ptr<FuncDecl> getInstVarDecl;
    OwnedPtr<RefExpr> getInstVarRef;
    auto curFile = nativeHandle->curFile;
<<<<<<< HEAD
    if (IsObjCObjectType(*field.GetTy())) {
||||||| constructed merge base
    if (typeMapper.IsObjCObjectType(*field.GetTy())) {
=======
    if (typeMapper.IsObjCObjectType(*field.DataTy())) {
>>>>>>> feat: local mode
        getInstVarDecl = bridge.GetGetInstanceVariableObjDecl();
        getInstVarRef = CreateRefExpr(*getInstVarDecl);
    } else {
        getInstVarDecl = bridge.GetGetInstanceVariableDecl();
        getInstVarRef = CreateRefExpr(*getInstVarDecl);

        getInstVarRef->instTys.emplace_back(typeMapper.Cj2CType(field.DataTy()));
        getInstVarRef->SetTy(typeManager.GetInstantiatedTy(
            getInstVarDecl->GetTy(), GenerateTypeMapping(*getInstVarDecl, getInstVarRef->instTys)));
    }

    auto objcname = nameGenerator.GetObjCDeclName(field);
    auto nameExpr =
        WithinFile(CreateLitConstExpr(LitConstKind::STRING, objcname, GetStringDecl(importManager).GetTy()), curFile);

    auto args = Nodes<FuncArg>(CreateFuncArg(std::move(nativeHandle)), CreateFuncArg(std::move(nameExpr)));

<<<<<<< HEAD
    auto retTy = StaticCast<FuncTy>(getInstVarRef->GetTy())->retTy;
    auto ret = CreateCallExpr(
        std::move(getInstVarRef), std::move(args), getInstVarDecl, retTy, CallKind::CALL_DECLARED_FUNCTION);
    ret->curFile = curFile;
    return ret;
||||||| constructed merge base
    auto retTy = StaticCast<FuncTy>(getInstVarRef->GetTy())->retTy;
    return WithinFile(CreateCallExpr(std::move(getInstVarRef), std::move(args), getInstVarDecl, retTy,
                          CallKind::CALL_DECLARED_FUNCTION),
        curFile);
=======
    auto getInstVarFnTy = DynamicCast<FuncTy>(getInstVarRef->DataTy());
    CJC_NULLPTR_CHECK(getInstVarFnTy);
    ModalTy retTy = getInstVarFnTy->retTy;
    return WithinFile(CreateCallExpr(std::move(getInstVarRef), std::move(args), getInstVarDecl, retTy,
                          CallKind::CALL_DECLARED_FUNCTION),
        curFile);
>>>>>>> feat: local mode
}

OwnedPtr<CallExpr> ASTFactory::CreateSetInstanceVariableCall(
    const PropDecl& field, OwnedPtr<Expr> nativeHandle, OwnedPtr<Expr> value)
{
    Ptr<FuncDecl> setInstVarDecl;
    OwnedPtr<RefExpr> setInstVarRef;
    auto curFile = nativeHandle->curFile;
<<<<<<< HEAD
    if (IsObjCObjectType(*field.GetTy())) {
||||||| constructed merge base
    if (typeMapper.IsObjCObjectType(*field.GetTy())) {
=======
    if (typeMapper.IsObjCObjectType(*field.DataTy())) {
>>>>>>> feat: local mode
        setInstVarDecl = bridge.GetSetInstanceVariableObjDecl();
        setInstVarRef = CreateRefExpr(*setInstVarDecl);
    } else {
        setInstVarDecl = bridge.GetSetInstanceVariableDecl();
        setInstVarRef = CreateRefExpr(*setInstVarDecl);

        setInstVarRef->instTys.emplace_back(typeMapper.Cj2CType(field.DataTy()));
        setInstVarRef->SetTy(typeManager.GetInstantiatedTy(
            setInstVarDecl->GetTy(), GenerateTypeMapping(*setInstVarDecl, setInstVarRef->instTys)));
    }

    auto objcname = nameGenerator.GetObjCDeclName(field);
    auto nameExpr =
        WithinFile(CreateLitConstExpr(LitConstKind::STRING, objcname, GetStringDecl(importManager).GetTy()), curFile);

    auto args = Nodes<FuncArg>(
        CreateFuncArg(std::move(nativeHandle)), CreateFuncArg(std::move(nameExpr)), CreateFuncArg(std::move(value)));

<<<<<<< HEAD
    auto ret = CreateCallExpr(std::move(setInstVarRef), std::move(args), setInstVarDecl,
        TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT), CallKind::CALL_DECLARED_FUNCTION);
    ret->curFile = curFile;

    return ret;
||||||| constructed merge base
    return WithinFile(CreateCallExpr(std::move(setInstVarRef), std::move(args), setInstVarDecl,
        TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT), CallKind::CALL_DECLARED_FUNCTION), curFile);
=======
    return WithinFile(CreateCallExpr(std::move(setInstVarRef), std::move(args), setInstVarDecl,
        {TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT)}, CallKind::CALL_DECLARED_FUNCTION),
        curFile);
>>>>>>> feat: local mode
}

OwnedPtr<Expr> ASTFactory::CreateUnsafePointerCast(OwnedPtr<Expr> expr, Ptr<Ty> elementType)
{
    CJC_ASSERT(expr->DataTy()->IsPointer() || expr->DataTy()->IsCFunc());
    CJC_ASSERT(elementType->IsMetCType());
    auto ptrExpr = MakeOwned<PointerExpr>();
    auto pointerType = typeManager.GetPointerTy(elementType);
    CopyBasicInfo(expr, ptrExpr);
    ptrExpr->arg = CreateFuncArg(std::move(expr));
    ptrExpr->SetTy({pointerType});
    ptrExpr->type = CreateType(ptrExpr->DataTy());
    ptrExpr->EnableAttr(Attribute::COMPILER_ADD);
    return ptrExpr;
}

void ASTFactory::SetDesugarExpr(Ptr<Expr> original, OwnedPtr<Expr> desugared)
{
    original->desugarExpr = std::move(desugared);
    original->desugarExpr->sourceExpr = original;
    CopyBasicInfo(original, original->desugarExpr);
    AddCurFile(*original->desugarExpr, original->curFile);
}

OwnedPtr<FuncDecl> ASTFactory::CreateFinalizer(ClassDecl& target)
{
    static auto unitTy = TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT);
    auto finalizerFnTy = typeManager.GetFunctionTy({}, {unitTy});
    auto fbody = CreateFuncBody({}, nullptr, CreateBlock({}, {unitTy}), {finalizerFnTy});
    fbody->paramLists.emplace_back(MakeOwned<FuncParamList>());
    auto nativeHandleExpr = CreateNativeHandleFieldExpr(target);
    auto releaseCall = CreateObjCReleaseCall(std::move(nativeHandleExpr));
    fbody->body->body.emplace_back(std::move(releaseCall));
    auto fd = CreateFuncDecl(FINALIZER_IDENT, std::move(fbody), {finalizerFnTy});
    PutDeclToClassLikeBody(*fd, target);
    fd->EnableAttr(Attribute::PRIVATE, Attribute::FINALIZER, Attribute::DOES_NOT_THROW);
    fd->linkage = Linkage::EXTERNAL;
    fd->funcBody->funcDecl = fd.get();

    return fd;
}

<<<<<<< HEAD
||||||| constructed merge base
OwnedPtr<VarDecl> ASTFactory::CreateHasInitedField(ClassDecl& target)
{
    static auto boolTy = typeManager.GetPrimitiveTy(TypeKind::TYPE_BOOLEAN);
    auto initializer = CreateLitConstExpr(LitConstKind::BOOL, "false", boolTy);
    auto ret = CreateVarDecl(HAS_INITED_IDENT, std::move(initializer));
    ret->isVar = true;
    PutDeclToClassLikeBody(*ret, target);
    ret->EnableAttr(Attribute::PRIVATE, Attribute::NO_REFLECT_INFO, Attribute::HAS_INITED_FIELD);

    return ret;
}

=======
OwnedPtr<VarDecl> ASTFactory::CreateHasInitedField(ClassDecl& target)
{
    static auto boolTy = typeManager.GetPrimitiveTy(TypeKind::TYPE_BOOLEAN);
    auto initializer = CreateLitConstExpr(LitConstKind::BOOL, "false", {boolTy});
    auto ret = CreateVarDecl(HAS_INITED_IDENT, std::move(initializer));
    ret->isVar = true;
    PutDeclToClassLikeBody(*ret, target);
    ret->EnableAttr(Attribute::PRIVATE, Attribute::NO_REFLECT_INFO, Attribute::HAS_INITED_FIELD);

    return ret;
}

>>>>>>> feat: local mode
OwnedPtr<FuncDecl> ASTFactory::CreateNativeHandleGetterDecl(ClassLikeDecl& target)
{
    static auto nativeObjCIdTy = bridge.GetNativeObjCIdTy();
    std::vector<OwnedPtr<FuncParam>> getterParams;
    auto getterParamList = CreateFuncParamList(std::move(getterParams));
    auto getterFnTy = typeManager.GetFunctionTy({}, {nativeObjCIdTy});
    auto getterFuncBody = CreateFuncBody(Nodes<FuncParamList>(std::move(getterParamList)), CreateType(nativeObjCIdTy),
        CreateBlock({}, {nativeObjCIdTy}), {getterFnTy});
    auto getterDecl = CreateFuncDecl(NATIVE_HANDLE_GETTER_IDENT, std::move(getterFuncBody), {getterFnTy});
    getterDecl->funcBody->funcDecl = getterDecl.get();
    getterDecl->funcBody->parentClassLike = &target;
    getterDecl->EnableAttr(Attribute::PUBLIC, Attribute::INITIALIZED, Attribute::IS_CHECK_VISITED);
    PutDeclToClassLikeBody(*getterDecl, target);

    return getterDecl;
}

OwnedPtr<Expr> ASTFactory::CreateNativeHandleFieldExpr(ClassDecl& target)
{
<<<<<<< HEAD
    auto nativeHandleDecl = GetNativeHandleField(target);
    return CreateMemberAccess(CreateThisRef(&target, target.GetTy(), target.curFile), *nativeHandleDecl);
||||||| constructed merge base
    auto nativeHandleDecl = GetNativeVarHandle(target);
    CJC_NULLPTR_CHECK(nativeHandleDecl);
    return CreateMemberAccess(CreateThisRef(&target, target.GetTy(), target.curFile), *nativeHandleDecl);
=======
    auto nativeHandleDecl = GetNativeVarHandle(target);
    CJC_NULLPTR_CHECK(nativeHandleDecl);
    return CreateMemberAccess(CreateThisRef(&target, target.DataTy(), target.curFile), *nativeHandleDecl);
>>>>>>> feat: local mode
}

OwnedPtr<Expr> ASTFactory::CreateMethodCallViaMsgSendSuper(
    FuncDecl& fd, OwnedPtr<Expr> receiver, OwnedPtr<Expr> objCSuper, std::vector<OwnedPtr<Expr>> rawArgs)
{
    auto objCName = nameGenerator.GetObjCDeclName(fd);
    auto fdFnTy = DynamicCast<FuncTy>(fd.DataTy());
    CJC_NULLPTR_CHECK(fdFnTy);
    return CreateObjCMsgSendSuperCall(std::move(receiver), std::move(objCSuper), objCName,
        typeMapper.Cj2CType(fdFnTy->retTy.Ty()), std::move(rawArgs));
}

OwnedPtr<Expr> ASTFactory::CreatePropGetterCallViaMsgSendSuper(
    PropDecl& pd, OwnedPtr<Expr> receiver, OwnedPtr<Expr> objCSuper)
{
    auto objCName = nameGenerator.GetObjCDeclName(pd);
    return CreateObjCMsgSendSuperCall(
        std::move(receiver), std::move(objCSuper), objCName, typeMapper.Cj2CType(pd.DataTy()), {});
}

OwnedPtr<Expr> ASTFactory::CreatePropSetterCallViaMsgSendSuper(
    PropDecl& pd, OwnedPtr<Expr> receiver, OwnedPtr<Expr> objCSuper, OwnedPtr<Expr> value)
{
    auto objCName = nameGenerator.GetObjCDeclName(pd);
    std::transform(
        objCName.begin(), objCName.begin() + 1, objCName.begin(), [](unsigned char c) { return std::toupper(c); });
    objCName = "set" + objCName + ":";
    return CreateObjCMsgSendSuperCall(std::move(receiver), std::move(objCSuper), objCName,
        typeMapper.Cj2CType(pd.DataTy()), Nodes<Expr>(std::move(value)));
}

OwnedPtr<CallExpr> ASTFactory::CreateObjCMsgSendSuperCall(OwnedPtr<Expr> receiver, OwnedPtr<Expr> objCSuper,
    const std::string& selector, Ptr<Ty> retTy, std::vector<OwnedPtr<Expr>> rawArgs)
{
    auto selCall = CreateGetCachedSelectorAccess(selector, receiver->curFile);

    auto ft = MakeOwned<FuncType>();
    ft->retType = CreateType(retTy);

    rawArgs.insert(rawArgs.begin(), ASTCloner::Clone(selCall.get()));
    rawArgs.insert(rawArgs.begin(), std::move(receiver));
    std::vector<ModalTy> paramTys;
    for (auto& param : rawArgs) {
        ft->paramTypes.emplace_back(CreateType(param->DataTy()));
        paramTys.emplace_back(param->GetTy());
    }

    auto fty = typeManager.GetFunctionTy(paramTys, {retTy}, {.isC = true});
    ft->SetTy({fty});
    return CreateObjCMsgSendSuperCall(
        std::move(objCSuper), std::move(selCall), std::move(fty), std::move(ft), std::move(rawArgs));
}

OwnedPtr<CallExpr> ASTFactory::CreateObjCMsgSendSuperCall(OwnedPtr<Expr> objCSuper, OwnedPtr<Expr> sel,
    Ptr<FuncTy> ty, OwnedPtr<FuncType> funcType, std::vector<OwnedPtr<Expr>> funcArgs)
{
    auto msgSendSuperDecl = bridge.GetObjCMsgSendSuperDecl();
    // objCMsgSendSuper(objc_super*, SEL)
    auto msgSendSuperExpr = CreateCallExpr(CreateRefExpr(*msgSendSuperDecl),
        Nodes<FuncArg>(CreateFuncArg(std::move(objCSuper)), CreateFuncArg(std::move(sel))), nullptr,
        msgSendSuperDecl->funcBody->retType->GetTy(), CallKind::CALL_DECLARED_FUNCTION);
    auto retType = funcType->retType.get();

    auto cFuncDecl = importManager.GetCoreDecl<BuiltInDecl>(std::string(CFUNC_NAME));
    CJC_NULLPTR_CHECK(cFuncDecl);
    auto cFuncRefExpr = CreateRefExpr(*cFuncDecl);

    cFuncRefExpr->SetTy({ty});
    cFuncRefExpr->typeArguments.emplace_back(std::move(funcType));

    // CFunc<...>(msgSendSuper)
    auto cFuncCallExpr = CreateCallExpr(std::move(cFuncRefExpr),
        Nodes<FuncArg>(CreateFuncArg(std::move(msgSendSuperExpr))), nullptr, {ty}, CallKind::CALL_FUNCTION_PTR);

    std::vector<OwnedPtr<FuncArg>> msgSendSuperCallArgs;
    std::transform(funcArgs.begin(), funcArgs.end(), std::back_inserter(msgSendSuperCallArgs),
        [](auto&& argExpr) { return CreateFuncArg(std::move(argExpr)); });

    // CFunc<...>(msgSendSuper)(...)
    return CreateCallExpr(std::move(cFuncCallExpr), std::move(msgSendSuperCallArgs), nullptr, retType->GetTy(),
        CallKind::CALL_FUNCTION_PTR);
}

OwnedPtr<Expr> ASTFactory::CreateObjectGetClassCall(OwnedPtr<Expr> id, Ptr<File> curFile)
{
    auto objectGetClassDecl = bridge.GetObjectGetClassDecl();
    auto objectGetClassExpr = CreateRefExpr(*objectGetClassDecl);

    std::vector<OwnedPtr<FuncArg>> args;
    args.emplace_back(CreateFuncArg(std::move(id)));
    auto objectGetClassCallExpr = CreateCallExpr(std::move(objectGetClassExpr), std::move(args), objectGetClassDecl,
        {typeManager.GetBoolTy()}, CallKind::CALL_DECLARED_FUNCTION);
    return WithinFile(std::move(objectGetClassCallExpr), curFile);
}

OwnedPtr<Expr> ASTFactory::CreateConvertToNSStringCall(
    OwnedPtr<Expr> id, ClassDecl& classDecl, Ptr<File> curFile)
{
    auto convertDecl = bridge.GetConvertToNSStringDecl();
    auto convertExpr = CreateRefExpr(*convertDecl);

    std::vector<OwnedPtr<FuncArg>> args;
    args.emplace_back(CreateFuncArg(std::move(id)));

    auto nativeObjCIdTy = bridge.GetNativeObjCIdTy();
<<<<<<< HEAD
    auto convertCallExpr = CreateCallExpr(
        std::move(convertExpr), std::move(args), convertDecl, nativeObjCIdTy, CallKind::CALL_DECLARED_FUNCTION);
||||||| constructed merge base
    auto convertCallExpr = CreateCallExpr(std::move(convertExpr), std::move(args), convertDecl, nativeObjCIdTy,
        CallKind::CALL_DECLARED_FUNCTION);
=======
    auto convertCallExpr = CreateCallExpr(
        std::move(convertExpr), std::move(args), convertDecl, {nativeObjCIdTy}, CallKind::CALL_DECLARED_FUNCTION);
>>>>>>> feat: local mode

    std::vector<OwnedPtr<FuncArg>> ctorArgs;
    ctorArgs.emplace_back(CreateFuncArg(std::move(convertCallExpr)));

<<<<<<< HEAD
    auto realTarget = GetObjCMirrorBaseCtor(classDecl);
    auto thisCall = CreateThisCall(classDecl, *realTarget, realTarget->GetTy(), curFile, std::move(ctorArgs));
    AppendNativeObjCIdMarkerIfNeeded(*thisCall, curFile);

    return thisCall;
||||||| constructed merge base
    auto realTarget = GetGeneratedBaseCtor(classDecl);
    return CreateThisCall(classDecl, *realTarget, realTarget->GetTy(), curFile, std::move(ctorArgs));
=======
    auto realTarget = GetGeneratedBaseCtor(classDecl);
    return CreateThisCall(classDecl, *realTarget, realTarget->DataTy(), curFile, std::move(ctorArgs));
>>>>>>> feat: local mode
}

OwnedPtr<Expr> ASTFactory::CreateDescriptionAsStringCall(OwnedPtr<Expr> id)
{
    auto convertDecl = bridge.GetDescriptionAsStringDecl();
    auto curFile = convertDecl->curFile;
    return CreateCall(std::move(convertDecl), curFile, std::move(id));
}

OwnedPtr<Expr> ASTFactory::CreateObjCRetainCall(OwnedPtr<Expr> id)
{
    auto curFile = id->curFile;
    auto objCRetainDecl = bridge.GetObjCRetainDecl();
    return CreateCall(std::move(objCRetainDecl), curFile, std::move(id));
}

OwnedPtr<Expr> ASTFactory::CreateObjCRetainAutoreleasedReturnValueCall(OwnedPtr<Expr> id)
{
    auto curFile = id->curFile;
    auto objCRetainAutoreleasedReturnValueDecl = bridge.GetObjCRetainAutoreleasedReturnValueDecl();
    return CreateCall(std::move(objCRetainAutoreleasedReturnValueDecl), curFile, std::move(id));
}

OwnedPtr<Expr> ASTFactory::CreateGetCachedSelectorAccess(std::string selector, Ptr<File> curFile)
{
    auto nativeObjCSelTy = bridge.GetNativeObjCSelDecl()->type->GetTy();
    auto&& cachedSelectors = declarationCache.cachedSelectorDecls;
    if (auto it = cachedSelectors.find(selector); it != std::end(cachedSelectors)) {
        auto& varDecl = *it->second;
        std::vector<OwnedPtr<Node>> ifNull;
<<<<<<< HEAD
        ifNull.push_back(CreateAssignExpr(WithinFile(CreateRefExpr(varDecl), curFile),
            CreateRegisterNameCall(selector, curFile), typeManager.GetPrimitiveTy(TypeKind::TYPE_UNIT)));
||||||| constructed merge base
        ifNull.push_back(
            CreateAssignExpr(
                WithinFile(CreateRefExpr(varDecl), curFile),
                CreateRegisterNameCall(selector, curFile),
                typeManager.GetPrimitiveTy(TypeKind::TYPE_UNIT)));
=======
        ifNull.push_back(CreateAssignExpr(WithinFile(CreateRefExpr(varDecl), curFile),
            CreateRegisterNameCall(selector, curFile), {typeManager.GetPrimitiveTy(TypeKind::TYPE_UNIT)}));
>>>>>>> feat: local mode
        ifNull.push_back(WithinFile(CreateRefExpr(varDecl), curFile));

        std::vector<OwnedPtr<Node>> ifNotNull;
        ifNotNull.push_back(WithinFile(CreateRefExpr(varDecl), curFile));
        return CreateIfExpr(CreateGetObjcEntityOrNullCall(varDecl, curFile),
            CreateBlock(std::move(ifNull), nativeObjCSelTy), CreateBlock(std::move(ifNotNull), nativeObjCSelTy),
            nativeObjCSelTy);
    } else {
        std::string selectorVarName = "objcSelector$";
        // selector names may contain : and $ symbols,
        // so we conservatively fix them
        for (auto ch : selector) {
            if (ch == '$') {
                selectorVarName += "$DOLLAR$";
            } else if (ch == ':') {
                selectorVarName += "$$";
            } else {
                selectorVarName += ch;
            }
        }
        auto null = MakeOwned<PointerExpr>();
        null->SetTy(nativeObjCSelTy);
        auto varDecl = CreateVar(selectorVarName, nativeObjCSelTy, true, std::move(null));
        varDecl->EnableAttr(Attribute::INTERNAL, Attribute::GLOBAL, Attribute::INITIALIZED);
        PutDeclToFile(*varDecl, *curFile);
        declarationCache.cachedSelectorDecls[selector] = std::move(varDecl);
        return CreateGetCachedSelectorAccess(selector, curFile);
    }
}

OwnedPtr<Expr> ASTFactory::CreateGetCachedClassAccess(std::string className, Ptr<File> curFile)
{
    auto nativeObjCClassTy = bridge.GetNativeObjCClassTy();
    auto&& cachedClasses = declarationCache.cachedClassDecls;
    if (auto it = cachedClasses.find(className); it != std::end(cachedClasses)) {
        auto& varDecl = *it->second;
        std::vector<OwnedPtr<Node>> ifNull;
<<<<<<< HEAD
        ifNull.push_back(CreateAssignExpr(WithinFile(CreateRefExpr(varDecl), curFile),
            CreateGetClassCall(className, curFile), typeManager.GetPrimitiveTy(TypeKind::TYPE_UNIT)));
||||||| constructed merge base
        ifNull.push_back(
            CreateAssignExpr(
                WithinFile(CreateRefExpr(varDecl), curFile),
                CreateGetClassCall(className, curFile),
                typeManager.GetPrimitiveTy(TypeKind::TYPE_UNIT)));
=======
        ifNull.push_back(CreateAssignExpr(WithinFile(CreateRefExpr(varDecl), curFile),
            CreateGetClassCall(className, curFile), {typeManager.GetPrimitiveTy(TypeKind::TYPE_UNIT)}));
>>>>>>> feat: local mode
        ifNull.push_back(WithinFile(CreateRefExpr(varDecl), curFile));

        std::vector<OwnedPtr<Node>> ifNotNull;
        ifNotNull.push_back(WithinFile(CreateRefExpr(varDecl), curFile));
        return CreateIfExpr(CreateGetObjcEntityOrNullCall(varDecl, curFile),
            CreateBlock(std::move(ifNull), nativeObjCClassTy), CreateBlock(std::move(ifNotNull), nativeObjCClassTy),
            nativeObjCClassTy);
    } else {
        // unlike selectors, classes are required to be valid identifiers
        std::string classVarName = "objcClass$" + className;
        auto null = MakeOwned<PointerExpr>();
        null->SetTy(nativeObjCClassTy);
        auto varDecl = CreateVar(classVarName, nativeObjCClassTy, true, std::move(null));
        varDecl->EnableAttr(Attribute::INTERNAL, Attribute::GLOBAL, Attribute::INITIALIZED);
        PutDeclToFile(*varDecl, *curFile);
        declarationCache.cachedClassDecls[className] = std::move(varDecl);
        return CreateGetCachedClassAccess(className, curFile);
    }
}

OwnedPtr<Expr> ASTFactory::CreateGetCachedClassAccess(ClassLikeTy& ty, Ptr<File> curFile)
{
    std::string name = nameGenerator.GetObjCDeclName(*Ty::GetDeclOfTy(&ty));
    return CreateGetCachedClassAccess(name, curFile);
}

OwnedPtr<Expr> ASTFactory::CreateGetRegistryIdCall(OwnedPtr<Expr> id)
{
    auto curFile = id->curFile;
    auto getRegistryIdDecl = bridge.GetGetRegistryIdDecl();
    return CreateCall(std::move(getRegistryIdDecl), curFile, std::move(id));
}
