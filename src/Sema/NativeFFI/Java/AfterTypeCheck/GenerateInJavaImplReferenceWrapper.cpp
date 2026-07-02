// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "JavaDesugarManager.h"
#include "GenerateInJavaImplReferenceWrapper.h"
#include "NativeFFI/Utils.h"
#include "Utils.h"
#include "NativeFFI/Java/Utils.h"
#include "InteropLibBridge.h"

#include "cangjie/AST/Create.h"
#include "cangjie/AST/Match.h"
#include "cangjie/AST/Node.h"
#include "cangjie/AST/Types.h"
#include "cangjie/AST/Utils.h"
#include "cangjie/Utils/CheckUtils.h"
#include "cangjie/Utils/SafePointer.h"

namespace Cangjie::Native::FFI::Java {
using namespace Cangjie::Interop::Java;

namespace {

// Clone user params from commonCtor's param list into params.
void AppendClonedUserParams(FuncDecl& commonCtor, std::vector<OwnedPtr<FuncParam>>& params)
{
    if (!commonCtor.funcBody || commonCtor.funcBody->paramLists.empty() ||
        !commonCtor.funcBody->paramLists[0]) {
        return;
    }
    for (auto& userParam : commonCtor.funcBody->paramLists[0]->params) {
        if (userParam && userParam->type) {
            auto clonedParam = ASTCloner::Clone(Ptr<FuncParam>(userParam.get()));
            params.push_back(std::move(clonedParam));
        }
    }
}

// Collect user param types from commonCtor into paramTys.
void AppendUserParamTypes(FuncDecl& commonCtor, std::vector<Ptr<Ty>>& paramTys)
{
    if (!commonCtor.funcBody || commonCtor.funcBody->paramLists.empty() ||
        !commonCtor.funcBody->paramLists[0]) {
        return;
    }
    for (auto& userParam : commonCtor.funcBody->paramLists[0]->params) {
        if (userParam && Ty::IsTyCorrect(userParam->GetTy())) {
            paramTys.push_back(userParam->GetTy());
        }
    }
}

OwnedPtr<CallExpr> CreateRegistryCompanionConstructorCall(ClassDecl& companion, VarDecl& javaRefField)
{
    auto curFile = companion.curFile;
    auto& companionCtor = *GetJavaImplRegistryCompanionConstructor(companion);
    std::vector<OwnedPtr<FuncArg>> args;
    args.emplace_back(CreateFuncArg(WithinFile(CreateRefExpr(javaRefField), curFile)));

    return CreateCallExpr(
        WithinFile(CreateRefExpr(companionCtor), curFile),
        std::move(args),
        &companionCtor, companion.GetTy(), CallKind::CALL_OBJECT_CREATION);
}

OwnedPtr<AssignExpr> CreateRegistryCompanionRefFieldAssignment(ClassDecl& companion, ClassDecl& refWrapper,
    VarDecl& companionRefField)
{
    auto curFile = refWrapper.curFile;
    // `$reg = <companion>(this.$obj)`
    return CreateAssignExpr(
        WithinFile(CreateRefExpr(companionRefField), curFile),
        CreateRegistryCompanionConstructorCall(companion, *GetJavaRefField(refWrapper)),
        TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT));
}

/**
  * For original PrimaryCtorDecl, insert dummy registry companion reference initializer
  * since it is not actually used after desugaring into FuncDecl, but still participates in CHIR-level checks.
  */
void RewriteRefWrapperPrimaryConstructor(ClassDecl& refWrapper, ClassDecl& companion, VarDecl& companionRefField)
{
    CJC_ASSERT(Interop::Java::IsImplReferenceWrapper(refWrapper));

    for (auto member : refWrapper.GetMemberDeclPtrs()) {
        if (member->TestAttr(Attribute::FROM_COMMON_PART)) {
            continue;
        }
        auto pctor = As<ASTKind::PRIMARY_CTOR_DECL>(member);
        if (!pctor) {
            continue;
        }

        auto init = CreateRegistryCompanionRefFieldAssignment(companion, refWrapper, companionRefField);
        auto& pbody = pctor->funcBody->body->body;
        auto initPos = pbody.begin();

        if (!pbody.empty()) {
            auto firstNode = pbody[0].get();
            if (auto callExpr = As<ASTKind::CALL_EXPR>(firstNode); callExpr && IsSuperConstructorCall(*callExpr)) {
                // Insert field initialization right after parent constructor call.
                initPos++;
            }
        }

        pbody.insert(initPos, std::move(init));
    }
}
} // namespace

OwnedPtr<VarDecl> GenerateJavaImplRegistryCompanionReferenceField::CreateJavaImplRegistryCompanionReferenceField(
    ClassDecl& companion) const
{
    auto field = CreateVarDecl(JAVA_IMPL_REGISTRY_COMPANION_REF_FIELD_NAME, nullptr, CreateRefType(companion));
    field->SetTy(companion.GetTy());
    field->doNotExport = true;
    field->isVar = false;
    field->EnableAttr(Attribute::PRIVATE);
    return field;
}

VarDecl& GenerateJavaImplRegistryCompanionReferenceField::InsertJavaImplRegistryCompanionReferenceField(
    ClassDecl& companion, ClassDecl& refWrapper) const
{
    auto field = WithinFile(CreateJavaImplRegistryCompanionReferenceField(companion), refWrapper.curFile);
    auto& fieldRef = *field;
    field->outerDecl = &refWrapper;
    field->fullPackageName = refWrapper.fullPackageName;
    field->moduleName = refWrapper.moduleName;
    field->begin = refWrapper.body->begin;
    field->end = refWrapper.body->begin;
    field->EnableAttr(Attribute::IN_CLASSLIKE);

    refWrapper.GetMemberDecls().emplace(refWrapper.GetMemberDecls().begin(), std::move(field));

    return fieldRef;
}

void GenerateInJavaImplReferenceWrapper::GenerateWrappingConstructorBody(FuncDecl& wrappingCtor,
    VarDecl& companionRefField, ClassDecl& companion) const
{
    auto curFile = wrappingCtor.curFile;

    CJC_ASSERT(wrappingCtor.outerDecl && wrappingCtor.outerDecl->astKind == ASTKind::CLASS_DECL);
    auto& refWrapper = *StaticAs<ASTKind::CLASS_DECL>(wrappingCtor.outerDecl);

    auto& params = wrappingCtor.funcBody->paramLists[0]->params;

    CJC_ASSERT(!params.empty()); // params[0] is java entity parameter
    auto& entityParam = *params[0];

    CJC_ASSERT(params.size() > 1); // params[1] is registry id parameter
    auto& regIdParam = *params[1];

    wrappingCtor.constructorCall = ConstructorCall::SUPER;

    Ptr<FuncDecl> parentCtor = GetGeneratedJavaMirrorConstructor(*refWrapper.GetSuperClassDecl());
    CJC_ASSERT(parentCtor);
    CJC_ASSERT(parentCtor->funcBody->paramLists[0]->params.size() == 1); // Java_CFFI_JavaEntity

    auto superCall = CreateSuperCall(refWrapper, *parentCtor, parentCtor->GetTy());
    CopyPos(&wrappingCtor, superCall.get());
    superCall->args.push_back(CreateFuncArg(WithinFile(CreateRefExpr(entityParam), curFile)));

    auto& block = wrappingCtor.funcBody->body;
    block->SetTy(refWrapper.GetTy());

    // this.$reg = getFromRegistryById<companion.ty>(regId)
    auto regCompanionAssignment = CreateAssignExpr(
        WithinFile(CreateRefExpr(companionRefField), curFile),
        ilib.CreateGetFromRegistryCall(
            ilib.CreateGetJniEnvCall(curFile),
            WithinFile(CreateRefExpr(regIdParam), curFile),
            companion.GetTy()),
        TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT));
    CopyPos(&wrappingCtor, regCompanionAssignment.get());

    block->body.insert(block->body.begin(), std::move(regCompanionAssignment));
    block->body.insert(block->body.begin(), std::move(superCall));
}

OwnedPtr<FuncDecl> GenerateInJavaImplReferenceWrapper::GenerateJavaSideConstructor(FuncDecl& userCtor,
    VarDecl& companionRefField, ClassDecl& companion) const
{
    auto curFile = userCtor.curFile;
    auto& refWrapper = *StaticAs<ASTKind::CLASS_DECL>(userCtor.outerDecl);
    auto ctor = ASTCloner::Clone(Ptr(&userCtor));

    CJC_ASSERT(!ctor->funcBody->paramLists.empty());
    auto& params = ctor->funcBody->paramLists[0]->params;

    // reg: companion.ty
    auto& regCompanionParam = **params.emplace(params.begin(),
        CreateFuncParam("reg", CreateRefType(companion), nullptr, companion.GetTy()));

    // obj: Java_CFFI_JavaEntity
    auto javaEntityDecl = ilib.GetJavaEntityDecl();
    if (!javaEntityDecl) {
        return nullptr;
    }

    auto& entityParam = **params.emplace(params.begin(),
        CreateFuncParam(JAVA_IMPL_ENTITY_ARG_NAME_IN_GENERATED_CTOR,
            CreateRefType(*javaEntityDecl), nullptr, javaEntityDecl->GetTy()));

    Ptr<FuncDecl> parentCtor = GetGeneratedJavaMirrorConstructor(*refWrapper.GetSuperClassDecl());
    CJC_ASSERT(parentCtor);
    CJC_ASSERT(parentCtor->funcBody->paramLists[0]->params.size() == 1); // Java_CFFI_JavaEntity

    std::vector<Ptr<Ty>> paramTys;
    paramTys.push_back(javaEntityDecl->GetTy());
    paramTys.push_back(companion.GetTy());
    for (auto paramTy : StaticCast<FuncTy*>(userCtor.GetTy().get())->paramTys) {
        paramTys.push_back(paramTy);
    }
    auto ctorTy = typeManager.GetFunctionTy(paramTys, StaticCast<FuncTy*>(userCtor.GetTy().get())->retTy);

    auto superCall = CreateSuperCall(refWrapper, *parentCtor, parentCtor->GetTy());
    superCall->args.push_back(CreateFuncArg(WithinFile(CreateRefExpr(entityParam), curFile)));

    auto& block = ctor->funcBody->body;

    block->body.erase(std::remove_if(block->body.begin(), block->body.end(),
        [](auto& node) {
            if (auto call = As<ASTKind::CALL_EXPR>(node.get())) {
                return IsSuperConstructorCall(*call);
            }
            return false;
        }),
        block->body.end());

    auto regCompanionAssignment = CreateAssignExpr(
        WithinFile(CreateRefExpr(companionRefField), curFile),
        WithinFile(CreateRefExpr(regCompanionParam), curFile),
        TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT));

    block->body.insert(block->body.begin(), std::move(regCompanionAssignment));
    block->body.insert(block->body.begin(), std::move(superCall));

    ctor->funcBody->SetTy(ctorTy);
    ctor->SetTy(ctorTy);
    ctor->funcBody->funcDecl = ctor.get();
    ctor->constructorCall = ConstructorCall::SUPER;
    ctor->DisableAttr(Attribute::PRIMARY_CONSTRUCTOR);
    ctor->EnableAttr(Attribute::UNSAFE);
    return ctor;
}

FuncDecl& GenerateInJavaImplReferenceWrapper::InsertJavaSideConstructor(FuncDecl& userCtor,
    VarDecl& companionRefField, ClassDecl& companion, AfterTypeCheckContext& ctx) const
{
    CJC_ASSERT(userCtor.outerDecl && IsImpl(*userCtor.outerDecl));
    // reference wrapper is a class.
    auto refWrapper = StaticAs<ASTKind::CLASS_DECL>(userCtor.outerDecl);
    auto ctor = GenerateJavaSideConstructor(userCtor, companionRefField, companion);
    auto& res = *ctor;
    ctor->outerDecl = refWrapper;
    ctor->fullPackageName = refWrapper->fullPackageName;
    ctor->moduleName = refWrapper->moduleName;
    ctor->begin = refWrapper->body->begin;
    ctor->end = refWrapper->body->begin;
    ctor->funcBody->parentClassLike = refWrapper;
    ctor->EnableAttr(Attribute::IN_CLASSLIKE, Attribute::PROTECTED);

    refWrapper->GetMemberDecls().emplace_back(std::move(ctor));
    ctx.CacheJavaImplReferenceWrapperConstructorsPair(userCtor, res);
    return res;
}

void GenerateInJavaImplReferenceWrapper::RewriteUserDefinedConstructorInitialization(FuncDecl& ctor,
    VarDecl& companionRefField, ClassDecl& companion) const
{
    auto curFile = ctor.curFile;
    auto jniEnvCall = ilib.CreateGetJniEnvCall(curFile);
    if (!jniEnvCall) {
        ctor.EnableAttr(Attribute::IS_BROKEN);
        return;
    }

    auto jniEnvPtrDecl = ilib.GetJniEnvPtrDecl();
    if (!jniEnvPtrDecl) {
        ctor.EnableAttr(Attribute::IS_BROKEN);
        return;
    }

    auto& refWrapper = *StaticAs<ASTKind::CLASS_DECL>(ctor.outerDecl);
    FuncDecl& parentCtor = *GetGeneratedJavaMirrorConstructor(*refWrapper.GetSuperClassDecl());
    CJC_ASSERT(parentCtor.funcBody->paramLists[0]->params.size() == 1); // Java_CFFI_JavaEntity

    auto jniEnvVar = CreateTmpVarDecl(jniEnvPtrDecl->type, jniEnvCall);

    CJC_ASSERT(ctor.funcBody);
    CJC_ASSERT(ctor.funcBody->paramLists.size() == 1);
    auto& paramList = *ctor.funcBody->paramLists[0];

    // Java_CFFI_newJavaObject_raw call.
    // No global reference is created here:
    // it is wrapped into global reference in `super` constructors chain call: within `JObject`.
    if (auto newObjCall = ilib.CreateCFFINewJavaObjectCall(WithinFile(CreateRefExpr(*jniEnvVar), ctor.curFile),
        utils.GetJavaClassNormalizeSignature(*refWrapper.GetTy()), paramList, false, *ctor.curFile)) {
            auto jniEnvRef = WithinFile(CreateRefExpr(*jniEnvVar), curFile);
            std::vector<OwnedPtr<Node>> nodes;
            nodes.push_back(std::move(jniEnvVar));
            nodes.push_back(std::move(newObjCall));
            auto lambdaCall = WrapReturningLambdaCall(typeManager, std::move(nodes));
            auto superCall = CreateSuperCall(*ctor.outerDecl, parentCtor, parentCtor.GetTy());

            superCall->args.insert(superCall->args.begin(), CreateFuncArg(std::move(lambdaCall)));

            // Inserts assignment of registry companion into an instance.
            ctor.funcBody->body->body.insert(ctor.funcBody->body->body.begin(),
                CreateRegistryCompanionRefFieldAssignment(companion, refWrapper, companionRefField));
            // Inserts generated super call at the beginning of the constructor.
            // Original super call was removed at `DesugarJavaImplSuperConstructorCall` stage.
            ctor.funcBody->body->body.insert(ctor.funcBody->body->body.begin(), std::move(superCall));
    } else {
        ctor.EnableAttr(Attribute::IS_BROKEN);
        companion.EnableAttr(Attribute::HAS_BROKEN, Attribute::IS_BROKEN);
    }
}

void GenerateInJavaImplReferenceWrapper::RewriteUserDefinedConstructorsInitialization(AfterTypeCheckContext& ctx,
    ClassDecl& refWrapper, VarDecl& companionRefField, ClassDecl& companion) const
{
    for (auto ctor : ctx.GetJavaImplUserDefinedConstructors(refWrapper)) {
        RewriteUserDefinedConstructorInitialization(*ctor, companionRefField, companion);
    }

    RewriteRefWrapperPrimaryConstructor(refWrapper, companion, companionRefField);
}

void GenerateInJavaImplReferenceWrapper::InsertFinalizer(ClassDecl& refWrapper) const
{
    refWrapper.body->decls.emplace_back(ilib.CreateDeletingGlobalRefFinalizer(refWrapper));
}

void GenerateInJavaImplReferenceWrapper::Process(ClassDecl& refWrapper, AfterTypeCheckContext& ctx) const
{
    CJC_ASSERT(IsImplReferenceWrapper(refWrapper));

    auto registryCompanionName = GetImplRegistryCompanionClassName(refWrapper);
    auto registryCompanion = importManager.GetImportedDecl(refWrapper.fullPackageName, registryCompanionName);
    CJC_NULLPTR_CHECK(registryCompanion);
    CJC_ASSERT(registryCompanion->astKind == ASTKind::CLASS_DECL);

    auto& registryCompanionClass = *StaticAs<ASTKind::CLASS_DECL>(registryCompanion);

    auto& regCompanionRefField = ctx.GetJavaImplRegistryCompanionReferenceField(refWrapper);

    auto& wrappingCtor = ctx.GetJavaImplWrappingConstructor(refWrapper);
    GenerateWrappingConstructorBody(wrappingCtor, regCompanionRefField, registryCompanionClass);

    for (auto userDefinedCtor : ctx.GetJavaImplUserDefinedConstructors(refWrapper)) {
        InsertJavaSideConstructor(*userDefinedCtor, regCompanionRefField, registryCompanionClass, ctx);
    }

    for (auto commonCtor : ctx.GetJavaImplFromCommonPartConstructors(refWrapper)) {
        if (commonCtor->TestAttr(Attribute::IS_BROKEN)) {
            continue;
        }
        InsertJavaSideConstructorForFromCommonPart(*commonCtor, regCompanionRefField, registryCompanionClass, ctx);
    }

    RewriteUserDefinedConstructorsInitialization(ctx, refWrapper, regCompanionRefField, registryCompanionClass);
    InsertFinalizer(refWrapper);
}

GenerateInJavaImplReferenceWrapper::GenerateInJavaImplReferenceWrapper(TypeManager& typeManager,
    const ImportManager& importManager, InteropLibBridge& ilib, Native::FFI::Java::Utils& utils)
    : typeManager(typeManager), importManager(importManager), ilib(ilib), utils(utils)
{
}

void GenerateJavaImplRegistryCompanionReferenceField::Process(AfterTypeCheckContext& ctx)
{
    for (auto refWrapper : ctx.GetJavaImplReferenceWrappers()) {
        auto& companion = ctx.GetJavaImplRegistryCompanion(*refWrapper);
        auto& field = InsertJavaImplRegistryCompanionReferenceField(companion, *refWrapper);
        ctx.CacheJavaImplRegistryCompanionReferenceField(*refWrapper, field);
    }
}

void GenerateInJavaImplReferenceWrapper::Process(AfterTypeCheckContext& ctx)
{
    GenerateJavaImplRegistryCompanionReferenceField regCompanionFieldStage;
    regCompanionFieldStage(ctx);

    for (auto& refWrapper : ctx.GetJavaImplReferenceWrappers()) {
        if (refWrapper->TestAttr(Attribute::IS_BROKEN)) {
            continue;
        }

        Process(*refWrapper, ctx);
    }
}

OwnedPtr<FuncDecl> GenerateInJavaImplReferenceWrapper::GenerateJavaSideConstructorForFromCommonPart(
    FuncDecl& commonCtor, VarDecl& companionRefField, ClassDecl& companion) const
{
    auto curFile = commonCtor.curFile;
    auto& refWrapper = *StaticAs<ASTKind::CLASS_DECL>(commonCtor.outerDecl);

    auto javaEntityDecl = ilib.GetJavaEntityDecl();
    if (!javaEntityDecl) {
        return nullptr;
    }

    auto ctor = MakeOwned<FuncDecl>();
    ctor->identifier = commonCtor.identifier;
    ctor->outerDecl = &refWrapper;
    ctor->EnableAttr(Attribute::COMPILER_ADD, Attribute::FROM_COMMON_PART);

    auto ctorBody = MakeOwned<FuncBody>();
    ctorBody->EnableAttr(Attribute::COMPILER_ADD);
    ctorBody->paramLists.push_back(MakeOwned<FuncParamList>());
    ctorBody->paramLists[0]->EnableAttr(Attribute::COMPILER_ADD);
    ctorBody->parentClassLike = &refWrapper;

    auto& params = ctorBody->paramLists[0]->params;

    params.insert(params.begin(),
        CreateFuncParam(JAVA_IMPL_ENTITY_ARG_NAME_IN_GENERATED_CTOR,
            CreateRefType(*javaEntityDecl), nullptr, javaEntityDecl->GetTy()));
    auto& entityParam = *params[0];

    params.insert(params.begin() + 1,
        CreateFuncParam("reg", CreateRefType(companion), nullptr, companion.GetTy()));
    auto& regCompanionParam = *params[1];

    AppendClonedUserParams(commonCtor, params);

    std::vector<Ptr<Ty>> paramTys;
    paramTys.push_back(javaEntityDecl->GetTy());
    paramTys.push_back(companion.GetTy());
    AppendUserParamTypes(commonCtor, paramTys);
    auto ctorTy = typeManager.GetFunctionTy(paramTys, TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT));

    Ptr<FuncDecl> parentCtor = GetGeneratedJavaMirrorConstructor(*refWrapper.GetSuperClassDecl());
    if (!parentCtor) {
        return nullptr;
    }

    auto superCall = CreateSuperCall(refWrapper, *parentCtor, parentCtor->GetTy());
    superCall->args.push_back(CreateFuncArg(WithinFile(CreateRefExpr(entityParam), curFile)));

    auto regCompanionAssignment = CreateAssignExpr(
        WithinFile(CreateRefExpr(companionRefField), curFile),
        WithinFile(CreateRefExpr(regCompanionParam), curFile),
        TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT));

    auto block = CreateBlock({});
    block->SetTy(refWrapper.GetTy());

    block->body.insert(block->body.begin(), std::move(regCompanionAssignment));
    block->body.insert(block->body.begin(), std::move(superCall));

    ctorBody->body = std::move(block);
    ctor->funcBody = std::move(ctorBody);
    ctor->funcBody->funcDecl = ctor.get();
    ctor->funcBody->SetTy(ctorTy);
    ctor->SetTy(ctorTy);
    ctor->constructorCall = ConstructorCall::SUPER;
    ctor->DisableAttr(Attribute::PRIMARY_CONSTRUCTOR);
    ctor->EnableAttr(Attribute::UNSAFE);
    return ctor;
}

FuncDecl& GenerateInJavaImplReferenceWrapper::InsertJavaSideConstructorForFromCommonPart(FuncDecl& commonCtor,
    VarDecl& companionRefField, ClassDecl& companion, AfterTypeCheckContext& ctx) const
{
    CJC_ASSERT(commonCtor.outerDecl && IsImpl(*commonCtor.outerDecl));
    auto refWrapper = StaticAs<ASTKind::CLASS_DECL>(commonCtor.outerDecl);
    auto ctor = GenerateJavaSideConstructorForFromCommonPart(commonCtor, companionRefField, companion);
    CJC_ASSERT(ctor);
    auto& res = *ctor;
    ctor->outerDecl = refWrapper;
    ctor->fullPackageName = refWrapper->fullPackageName;
    ctor->moduleName = refWrapper->moduleName;
    ctor->begin = refWrapper->body->begin;
    ctor->end = refWrapper->body->begin;
    ctor->EnableAttr(Attribute::IN_CLASSLIKE, Attribute::PROTECTED);

    refWrapper->GetMemberDecls().emplace_back(std::move(ctor));
    ctx.CacheJavaImplReferenceWrapperConstructorsPair(commonCtor, res);
    return res;
}

} // namespace Cangjie::Native::FFI::Java
