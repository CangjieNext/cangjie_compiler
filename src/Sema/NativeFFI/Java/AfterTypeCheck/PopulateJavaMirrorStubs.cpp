// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "PopulateJavaMirrorStubs.h"
#include "NativeFFI/Java/AfterTypeCheck/ASTFactory.h"
#include "NativeFFI/Java/AfterTypeCheck/AfterTypeCheckContext.h"
#include "NativeFFI/Java/AfterTypeCheck/Utils.h"
#include "NativeFFI/Java/Utils.h"
#include "NativeFFI/Utils.h"
#include "InteropLibBridge.h"

#include "cangjie/AST/Create.h"
#include "cangjie/AST/Node.h"
#include "cangjie/AST/Types.h"
#include "cangjie/AST/Utils.h"
#include "cangjie/Sema/TypeManager.h"
#include "cangjie/Utils/CheckUtils.h"

namespace Cangjie::Native::FFI::Java {

void PopulateJavaMirrorStubs::PopulateUserConstructor(AfterTypeCheckContext& ctx,
    AST::FuncDecl& userCtor, AST::FuncDecl& wrappingCtor) const
{
    if (IsJArray(*userCtor.outerDecl)) {
        return;
    }
    auto curFile = userCtor.curFile;
    CJC_ASSERT(userCtor.TestAttr(Attribute::CONSTRUCTOR));
    userCtor.constructorCall = ConstructorCall::OTHER_INIT;
    auto thisCall = CreateThisCall(*userCtor.outerDecl, wrappingCtor, wrappingCtor.GetTy(), curFile);
    CJC_ASSERT(userCtor.funcBody);
    CJC_ASSERT(userCtor.funcBody->paramLists.size() == 1);
    auto& body = userCtor.funcBody->body->body;
    auto& paramList = *userCtor.funcBody->paramLists[0];
    paramList.curFile = userCtor.curFile;
    auto ctorSignature = JavaMemberSignature::FromConstructor(userCtor, /* withMarker = */ false);
    auto constructorExpr = factory.CreateNewJavaObjectCall(ctx, *curFile, ctorSignature,
        factory.CreateParamsUsage(paramList));
    if (!constructorExpr) {
        userCtor.EnableAttr(Attribute::IS_BROKEN);
        return;
    }
    thisCall->args.push_back(CreateFuncArg(WrapReturningLambdaCall(typeManager, std::move(constructorExpr))));
    body.push_back(std::move(thisCall));
}

void PopulateJavaMirrorStubs::PopulateJArrayMethod(ClassDecl& jarray, FuncDecl& userMethod) const
{
    auto curFile = userMethod.curFile;
    CJC_ASSERT(IsJArray(jarray));
    CJC_ASSERT(!userMethod.TestAttr(Attribute::STATIC)); // There are no static methods in JArray yet.

    CJC_ASSERT_WITH_MSG(!jarray.generic->typeParameters.empty(), "JArray ctor must be generic");
    auto genericParam = jarray.generic->typeParameters[0].get();
    CJC_NULLPTR_CHECK(genericParam);

    auto retTy = userMethod.funcBody->retType->GetTy();
    userMethod.funcBody->body = CreateBlock({}, retTy);
    userMethod.funcBody->SetTy(retTy);
    auto& body = userMethod.funcBody->body->body;
    CJC_ASSERT(!userMethod.funcBody->paramLists.empty());
    auto& paramList = *userMethod.funcBody->paramLists[0];
    auto& jniEnv = *StaticAs<ASTKind::VAR_DECL>(
        body.emplace_back(CreateTmpVarDecl(nullptr, ilib.CreateGetJniEnvCall(curFile))).get());

    auto methodCall = ilib.CreateCFFICallArrayMethodCall(WithinFile(CreateRefExpr(jniEnv), curFile),
        CreateJavaRefCall(jarray, curFile), paramList, genericParam, GetArrayOperationKind(userMethod));
    methodCall = ilib.ConvertJavaResultToCJ(std::move(methodCall), retTy, &jarray);
    body.emplace_back(std::move(methodCall));
}

void PopulateJavaMirrorStubs::PopulateUserMethod(AfterTypeCheckContext& ctx,
    ClassLikeDecl& mirror, FuncDecl& userMethod) const
{
    if (IsJArray(mirror)) {
        return PopulateJArrayMethod(StaticCast<ClassDecl&>(mirror), userMethod);
    }
    if (userMethod.TestAttr(Attribute::ABSTRACT) && !IsSyntheticMirrorWrapper(mirror)) {
        return;
    }
    auto curFile = userMethod.curFile;
    CJC_NULLPTR_CHECK(curFile);

    // For mirror method, syntactic return type has to be specified, so it's safe to use it after parser.
    auto retTy = userMethod.funcBody->retType->GetTy();
    userMethod.funcBody->body = CreateBlock({}, retTy);
    userMethod.funcBody->SetTy(retTy);
    auto& body = userMethod.funcBody->body->body;
    CJC_ASSERT_WITH_MSG(!userMethod.funcBody->paramLists.empty(), "paramLists cannot be empty");
    auto& paramList = *userMethod.funcBody->paramLists[0].get();

    auto methodSignature = JavaMemberSignature::FromMethod(userMethod);

    auto methodCall = methodSignature.IsStatic()
        ? factory.CreateStaticJavaMethodCall(ctx,
            *curFile,
            methodSignature, *retTy, factory.CreateParamsUsage(paramList))
        : factory.CreateVirtualJavaMethodCall(ctx,
            *curFile,
            ilib.CreateAsJniJobjectCall(CreateJavaRefCall(mirror, curFile)),
            methodSignature, *retTy, factory.CreateParamsUsage(paramList));
    if (!methodCall) {
        userMethod.EnableAttr(Attribute::IS_BROKEN);
        return;
    }
    InjectNodes(body, std::move(methodCall));

    if (IsSyntheticMirrorWrapper(mirror)) {
        userMethod.DisableAttr(Attribute::ABSTRACT);
    }
}

void PopulateJavaMirrorStubs::PopulateUserProperty(AfterTypeCheckContext& ctx,
    ClassLikeDecl& mirror, PropDecl& userProp) const
{
    CJC_ASSERT(userProp.outerDecl);
    InsertPropGetter(ctx, mirror, userProp);

    if (userProp.isVar) {
        InsertPropSetter(ctx, mirror, userProp);
    }
}

void PopulateJavaMirrorStubs::InsertPropGetter(AfterTypeCheckContext& ctx, ClassLikeDecl& mirror, PropDecl& prop) const
{
    auto& getter = *prop.getters.begin();
    getter->funcBody->body = CreateBlock({}, prop.GetTy());
    getter->funcBody->SetTy(prop.GetTy());
    auto& body = getter->funcBody->body->body;
    auto curFile = prop.curFile;
    CJC_NULLPTR_CHECK(curFile);

    OwnedPtr<Expr> jniGetterCall;
    auto isArrayGetLength = IsJArray(*prop.outerDecl) && GetArrayOperationKind(prop) == ArrayOperationKind::GET_LENGTH;

    auto fieldSignature = JavaMemberSignature::FromProperty(prop);
    if (isArrayGetLength) {
        jniGetterCall = ilib.CreateCFFIArrayLengthGetCall(CreateJavaRefCall(mirror, curFile), curFile);
    } else if (fieldSignature.IsStatic()) {
        jniGetterCall = WrapReturningLambdaCall(typeManager,
            factory.CreateStaticJavaFieldGetCall(ctx, *curFile, fieldSignature, *prop.GetTy()));
    } else {
        jniGetterCall = WrapReturningLambdaCall(typeManager, factory.CreateInstanceJavaFieldGetCall(ctx, *curFile,
            ilib.CreateAsJniJobjectCall(CreateJavaRefCall(mirror, curFile)),
            fieldSignature, *prop.GetTy()));
    }

    auto& jniGetterCallRes = *StaticAs<ASTKind::VAR_DECL>(
        body.emplace_back(CreateTmpVarDecl(nullptr, std::move(jniGetterCall))).get());
    jniGetterCallRes.SetTy(jniGetterCallRes.initializer->GetTy());
    jniGetterCallRes.curFile = curFile;

    CopyBasicInfo(jniGetterCallRes.initializer.get(), &jniGetterCallRes);

    auto callResRef = CreateRefExpr(jniGetterCallRes);
    CopyBasicInfo(&jniGetterCallRes, callResRef.get());
    callResRef->curFile = curFile;

    body.emplace_back(CreateReturnExpr(std::move(callResRef), getter->funcBody.get()));
}

void PopulateJavaMirrorStubs::InsertPropSetter(AfterTypeCheckContext& ctx,
    ClassLikeDecl& mirror, PropDecl& prop) const
{
    static auto unitTy = TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT);

    auto curFile = prop.curFile;

    auto& setter = *prop.setters.begin();
    setter->funcBody->body = CreateBlock({}, prop.GetTy());
    setter->funcBody->SetTy(unitTy);

    auto& body = setter->funcBody->body->body;

    CJC_ASSERT_WITH_MSG(!prop.setters.empty(), "expected at least one setter");
    CJC_ASSERT_WITH_MSG(!prop.setters[0]->funcBody->paramLists.empty(), "setter paramLists cannot be empty");
    CJC_ASSERT_WITH_MSG(!prop.setters[0]->funcBody->paramLists[0]->params.empty(), "setter params cannot be empty");
    auto paramRef = WithinFile(CreateRefExpr(*prop.setters[0]->funcBody->paramLists[0]->params[0]), curFile);

    auto fieldSignature = JavaMemberSignature::FromProperty(prop);
    OwnedPtr<Expr> jniSetterCall = fieldSignature.IsStatic()
        ? factory.CreateStaticJavaFieldSetCall(ctx, *curFile, fieldSignature, *prop.GetTy(),
            std::move(paramRef))
        : factory.CreateInstanceJavaFieldSetCall(ctx, *curFile,
            ilib.CreateAsJniJobjectCall(CreateJavaRefCall(mirror, curFile)),
            fieldSignature, *prop.GetTy(),
            std::move(paramRef));
    InjectNodes(body, std::move(jniSetterCall));
}

void PopulateJavaMirrorStubs::Process(AfterTypeCheckContext& ctx, ClassLikeDecl& mirror) const
{
    Ptr<FuncDecl> wrappingCtor = Is<ClassDecl>(mirror) ? GetJavaMirrorWrappingConstructor(mirror) : nullptr;
    for (auto& member : mirror.GetMemberDeclPtrs()) {
        if (member->TestAttr(Attribute::IS_BROKEN)) {
            continue;
        }
        if (auto fd = As<ASTKind::FUNC_DECL>(member.get())) {
            if (fd->IsFinalizer() || IsJavaRefGetter(*fd) || IsWrappingConstructorOfJavaMirror(*fd)) {
                continue;
            }

            if (fd->TestAttr(Attribute::CONSTRUCTOR)) {
                PopulateUserConstructor(ctx, *fd, *wrappingCtor);
            } else {
                bool isStatic = member->TestAttr(Attribute::STATIC);
                bool isDefault = member->TestAttr(Attribute::JAVA_HAS_DEFAULT);
                if (mirror.IsInterfaceDecl() && !isStatic && !isDefault) {
                    continue;
                }

                PopulateUserMethod(ctx, mirror, *fd);
            }
        } else if (auto prop = As<ASTKind::PROP_DECL>(member.get())) {
            PopulateUserProperty(ctx, mirror, *prop);
        }
    }
}

PopulateJavaMirrorStubs::PopulateJavaMirrorStubs(TypeManager& typeManager, InteropLibBridge& ilib, ASTFactory& factory)
    : typeManager(typeManager), ilib(ilib), factory(factory)
{
}

void PopulateJavaMirrorStubs::Process(AfterTypeCheckContext& ctx)
{
    for (auto mirror : ctx.GetJavaMirrors()) {
        if (mirror->TestAttr(Attribute::IS_BROKEN)) {
            continue;
        }

        Process(ctx, *mirror);
    }
}

} // namespace Cangjie::Native::FFI::Java
