// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "NativeFFI/Java/Utils.h"
#include "TypeCheckUtil.h"
#include "JavaDesugarManager.h"

#include "NativeFFI/Utils.h"
#include "cangjie/AST/AttributePack.h"
#include "cangjie/AST/Create.h"
#include "cangjie/AST/Match.h"
#include "cangjie/AST/Node.h"
#include "cangjie/Utils/CheckUtils.h"
#include "cangjie/Utils/ConstantsUtils.h"
#include "Utils.h"
#include "NativeFFI/Utils.h"
#include "NativeFFI/Java/AfterTypeCheck/Utils.h"
#include "cangjie/AST/Utils.h"

#include "cangjie/Utils/SafePointer.h"
#include "cangjie/AST/Create.h"


namespace Cangjie::Interop::Java {
using namespace Cangjie::Native::FFI;

// For an array of object-based type we add additional `get` method to perform unwrapping further,
// at call site with a concrete type
void JavaDesugarManager::InsertArrayJavaEntityGet(ClassDecl& decl)
{
    if (!decl.body) {
        return;
    }

    Ptr<FuncDecl> getOperationDecl;
    for (auto& member : decl.body->decls) {
        auto funcDecl = As<ASTKind::FUNC_DECL>(member);
        if (!funcDecl || !funcDecl->funcBody || funcDecl->funcBody->paramLists.empty()) {
            continue;
        }
        if (funcDecl && funcDecl->identifier == "[]" && funcDecl->funcBody->paramLists[0]->params.size() == 1) {
            getOperationDecl = As<ASTKind::FUNC_DECL>(member);
            break;
        }
    }

    if (!getOperationDecl) {
        return;
    }

    auto javaEntityGetDecl = ASTCloner::Clone(getOperationDecl);
    javaEntityGetDecl->identifier = JAVA_ARRAY_GET_FOR_REF_TYPES;
    javaEntityGetDecl->SetTy(
        {typeManager.GetFunctionTy({{TypeManager::GetPrimitiveTy(TypeKind::TYPE_INT32)}}, {lib.GetJavaEntityTy()})});

    auto javaEntity = lib.GetJavaEntityTy();
    if (!javaEntity || !javaEntityGetDecl->funcBody->retType) {
        return;
    }
    javaEntityGetDecl->funcBody->retType->SetTy(javaEntity);
    decl.body->decls.push_back(std::move(javaEntityGetDecl));
}

// For an array of object-based type we add additional `set` method to perform unwrapping further,
// at call site with a concrete type
void JavaDesugarManager::InsertArrayJavaEntitySet(ClassDecl& decl)
{
    if (!decl.body) {
        return;
    }

    Ptr<FuncDecl> setOperationDecl;
    for (auto& member : decl.body->decls) {
        auto funcDecl = As<ASTKind::FUNC_DECL>(member);
        if (!funcDecl || !funcDecl->funcBody || funcDecl->funcBody->paramLists.empty()) {
            continue;
        }
        if (funcDecl && funcDecl->identifier == "[]" && funcDecl->funcBody->paramLists[0]->params.size() == 2) {
            // Array "set" call requires 2 parameters: index and value to be set
            setOperationDecl = As<ASTKind::FUNC_DECL>(member);
            break;
        }
    }

    if (!setOperationDecl) {
        return;
    }

    auto javaEntity = lib.GetJavaEntityTy();
    auto javaEntitySetDecl = ASTCloner::Clone(setOperationDecl);
    if (!javaEntity || !javaEntitySetDecl->funcBody->retType) {
        return;
    }

    auto unitTy = TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT);
    javaEntitySetDecl->identifier = JAVA_ARRAY_SET_FOR_REF_TYPES;
    javaEntitySetDecl->funcBody->paramLists[0]->params[1]->SetTy(javaEntity);
    if (javaEntitySetDecl->funcBody->paramLists[0]->params[1]->type) {
        javaEntitySetDecl->funcBody->paramLists[0]->params[1]->type->SetTy({javaEntity});
    }
    javaEntitySetDecl->SetTy(
        {typeManager.GetFunctionTy({{TypeManager::GetPrimitiveTy(TypeKind::TYPE_INT32)}, {javaEntity}}, {unitTy})});

    javaEntitySetDecl->funcBody->retType->SetTy({unitTy});
    decl.body->decls.push_back(std::move(javaEntitySetDecl));
}

void JavaDesugarManager::InsertJavaRefVarDecl(ClassDecl& decl)
{
    auto& javaEntityDecl = *lib.GetJavaEntityDecl();

    auto javaref = CreateVarDecl(JAVA_REF_FIELD_NAME, nullptr, CreateRefType(javaEntityDecl));
    javaref->SetTy(javaEntityDecl.GetTy());
    javaref->EnableAttr(Attribute::INITIALIZATION_CHECKED, Attribute::INITIALIZED);
    javaref->begin = decl.body ? decl.body->begin : decl.begin;
    javaref->curFile = decl.curFile;

    Modifier protectedMod = Modifier(TokenKind::PUBLIC, javaref->begin);
    protectedMod.curFile = decl.curFile;
    javaref->modifiers.emplace(std::move(protectedMod));

    javaref->isVar = false;
    javaref->outerDecl = Ptr(&decl);
    javaref->fullPackageName = decl.fullPackageName;

    decl.body->decls.push_back(std::move(javaref));
}

void JavaDesugarManager::InsertJavaMirrorCtor(ClassDecl& decl, bool doStub)
{
    auto curFile = decl.curFile;
    auto isJObject = IsJObject(decl);
    auto& javaEntityDecl = *lib.GetJavaEntityDecl();

    auto param = CreateFuncParam("$ref", CreateRefType(javaEntityDecl), nullptr, javaEntityDecl.GetTy());

    std::vector<OwnedPtr<Node>> ctorNodes;

    if (isJObject) {
        // for JObject, body can be created and filled in one step
        if (!doStub) {
            return;
        }
        auto lhsRef = WithinFile(CreateRefExpr(*GetJavaRefField(decl)), curFile);
        auto rhs = lib.CreateNewGlobalRefCall(
            lib.CreateGetJniEnvCall(curFile),
            WithinFile(CreateRefExpr(*param), curFile),
            false);

        auto unitTy = TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT);
        auto refAssignment = CreateAssignExpr(std::move(lhsRef), std::move(rhs), {unitTy});
        ctorNodes.push_back(lib.CreateEnsureNotNullCall(WithinFile(CreateRefExpr(*param), curFile)));
        ctorNodes.push_back(std::move(refAssignment));
    } else if (!doStub) {
        auto ctor = GetJavaMirrorWrappingConstructor(decl);
        CJC_NULLPTR_CHECK(ctor);
        auto actualParam = CreateFuncArg(WithinFile(CreateRefExpr(*ctor->funcBody->paramLists[0]->params[0]), curFile));
<<<<<<< HEAD
        auto& superCtor = *GetJavaMirrorWrappingConstructor(*decl.GetSuperClassDecl());
        auto superCall = CreateSuperCall(decl, superCtor, superCtor.GetTy());
=======
        auto& superCtor = *GetGeneratedJavaMirrorConstructor(*decl.GetSuperClassDecl());
        auto superCall = CreateSuperCall(decl, superCtor, superCtor.DataTy());
>>>>>>> 0c41e6d (feat: add local mode implementation)
        superCall->args.emplace_back(std::move(actualParam));
        ctor->funcBody->body->body.emplace_back(std::move(superCall));
        return;
    }

    std::vector<ModalTy> ctorFuncParamTys;
    ctorFuncParamTys.push_back(param->GetTy());
    auto ctorFuncTy = typeManager.GetFunctionTy(std::move(ctorFuncParamTys), decl.GetTy());

    std::vector<OwnedPtr<FuncParam>> ctorParams;
    ctorParams.push_back(std::move(param));
    auto paramList = CreateFuncParamList(std::move(ctorParams));

    std::vector<OwnedPtr<FuncParamList>> paramLists;
    paramLists.push_back(std::move(paramList));

    auto ctorFuncBody = CreateFuncBody(
        std::move(paramLists), CreateRefType(decl), CreateBlock(std::move(ctorNodes), decl.GetTy()), decl.GetTy());

    auto fd = CreateFuncDecl("init", std::move(ctorFuncBody), {ctorFuncTy});
    fd->fullPackageName = decl.fullPackageName;
    fd->outerDecl = &decl;
    fd->funcBody->funcDecl = fd.get();
    fd->constructorCall = ConstructorCall::SUPER;
    fd->EnableAttr(Attribute::PUBLIC, Attribute::IN_CLASSLIKE, Attribute::CONSTRUCTOR);
    fd->funcBody->parentClassLike = &decl;
    fd->begin = decl.begin;
    fd->curFile = decl.curFile;
    fd->fullPackageName = decl.fullPackageName;

    decl.body->decls.emplace_back(std::move(fd));
}

void JavaDesugarManager::InsertJavaMirrorFinalizer(ClassDecl& mirror)
{
<<<<<<< HEAD
    mirror.body->decls.emplace_back(lib.CreateDeletingGlobalRefFinalizer(mirror));
=======
    static auto unitTy = TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT);
    auto curFile = mirror.curFile;
    auto fbody = CreateFuncBody({}, nullptr, CreateBlock({}, {unitTy}), {unitTy});
    fbody->paramLists.emplace_back(MakeOwned<FuncParamList>());
    auto delCall = lib.CreateDeleteGlobalRefCall(lib.CreateGetJniEnvCall(curFile), CreateJavaRefCall(mirror, curFile));
    fbody->body->body.emplace_back(std::move(delCall));
    auto fd = CreateFuncDecl("~init", std::move(fbody), {typeManager.GetFunctionTy({}, {unitTy})});
    fd->EnableAttr(Attribute::PRIVATE, Attribute::FINALIZER, Attribute::IN_CLASSLIKE);
    fd->linkage = Linkage::EXTERNAL;
    fd->funcBody->funcDecl = fd.get();
    fd->fullPackageName = mirror.fullPackageName;
    fd->outerDecl = Ptr(&mirror);
    fd->curFile = curFile;

    mirror.body->decls.emplace_back(std::move(fd));
>>>>>>> 0c41e6d (feat: add local mode implementation)
}

void JavaDesugarManager::InsertJavaMirrorHasInited(ClassDecl& mirror)
{
    static auto boolTy = typeManager.GetPrimitiveTy(TypeKind::TYPE_BOOLEAN);
    auto curFile = mirror.curFile;
    auto initializer = CreateLitConstExpr(LitConstKind::BOOL, "false", {boolTy});
    auto ret = WithinFile(CreateVarDecl(HAS_INITED_IDENT, std::move(initializer)), curFile);
    ret->isVar = true;
    ret->fullPackageName = mirror.fullPackageName;
    ret->outerDecl = Ptr(&mirror);
    ret->EnableAttr(
        Attribute::PRIVATE, Attribute::NO_REFLECT_INFO, Attribute::IN_CLASSLIKE, Attribute::HAS_INITED_FIELD);

    mirror.body->decls.emplace_back(std::move(ret));
}

void JavaDesugarManager::InsertAbstractJavaRefGetter(ClassLikeDecl& decl)
{
    auto& javaEntityDecl = *lib.GetJavaEntityDecl();
    std::vector<OwnedPtr<FuncParam>> callParams;
    std::vector<OwnedPtr<FuncParamList>> paramLists;
    paramLists.emplace_back(CreateFuncParamList(std::move(callParams)));

    auto funcBody =
        CreateFuncBody(std::move(paramLists), CreateRefType(javaEntityDecl), nullptr, {TypeManager::GetNothingTy()});

    funcBody->parentClassLike = &decl;

    std::vector<ModalTy> funcParamTys;
    Ptr<FuncTy> funcTy = typeManager.GetFunctionTy(std::move(funcParamTys), javaEntityDecl.GetTy());

    auto fd = CreateFuncDecl(JAVA_REF_GETTER_FUNC_NAME, std::move(funcBody), {funcTy});
    fd->EnableAttr(Attribute::PUBLIC, Attribute::IN_CLASSLIKE, Attribute::ABSTRACT);
    fd->funcBody->funcDecl = fd.get();
    fd->fullPackageName = decl.fullPackageName;
    fd->outerDecl = Ptr(&decl);
    // Every AST decl must carry a curFile (see CHIR AST2CHIR::CreateFuncSignatureAndSetGlobalCache).
    fd->curFile = decl.curFile;

    if (auto iDecl = As<ASTKind::INTERFACE_DECL>(&decl)) {
        iDecl->body->decls.emplace_back(std::move(fd));
    } else if (auto clDecl = As<ASTKind::CLASS_DECL>(&decl)) {
        clDecl->body->decls.emplace_back(std::move(fd));
    }
}

void JavaDesugarManager::InsertJavaRefGetterWithBody(ClassDecl& decl)
{
    auto javaEntityDecl = lib.GetJavaEntityDecl();
    std::vector<OwnedPtr<FuncParam>> callParams;
    std::vector<OwnedPtr<FuncParamList>> paramLists;
    paramLists.push_back(CreateFuncParamList(std::move(callParams)));

    auto javaRef = GetJavaRefField(decl);
    auto ref = CreateRefExpr(*javaRef);

    auto ret = CreateReturnExpr(std::move(ref));
    ret->SetTy({TypeManager::GetNothingTy()});
    std::vector<OwnedPtr<Node>> nodes;
    nodes.emplace_back(std::move(ret));

    auto block = CreateBlock(std::move(nodes), javaEntityDecl->GetTy());

    auto funcBody = CreateFuncBody(
        std::move(paramLists), CreateRefType(*javaEntityDecl), std::move(block), {TypeManager::GetNothingTy()});

    std::vector<ModalTy> funcParamTys;
    Ptr<FuncTy> funcTy = typeManager.GetFunctionTy(std::move(funcParamTys), javaEntityDecl->GetTy());

    auto fd = CreateFuncDecl(JAVA_REF_GETTER_FUNC_NAME, std::move(funcBody), {funcTy});
    fd->EnableAttr(Attribute::PUBLIC, Attribute::IN_CLASSLIKE, Attribute::INITIALIZED,
        Attribute::IS_CHECK_VISITED);
    fd->fullPackageName = decl.fullPackageName;
    fd->funcBody->funcDecl = fd.get();
    fd->funcBody->parentClassLike = &decl;
    fd->outerDecl = &decl;
    fd->curFile = decl.curFile;

    decl.body->decls.emplace_back(std::move(fd));
}

void JavaDesugarManager::DesugarJavaMirrorConstructor(FuncDecl& ctor, FuncDecl& generatedCtor)
{
    if (IsJArray(*ctor.outerDecl)) {
        return;
    }
    auto curFile = ctor.curFile;
    CJC_ASSERT(ctor.TestAttr(Attribute::CONSTRUCTOR));
    ctor.constructorCall = ConstructorCall::OTHER_INIT;
<<<<<<< HEAD
    auto thisCall = CreateThisCall(*ctor.outerDecl, generatedCtor, generatedCtor.GetTy(), curFile);
=======
    auto thisCall = CreateThisCall(*ctor.outerDecl, generatedCtor, generatedCtor.DataTy(), curFile);

>>>>>>> 0c41e6d (feat: add local mode implementation)
    CJC_ASSERT(ctor.funcBody);
    CJC_ASSERT(ctor.funcBody->paramLists.size() == 1);
    auto& body = ctor.funcBody->body->body;
        auto& paramList = *ctor.funcBody->paramLists[0];
    auto constructorExpr = lib.CreateJavaConstructorBlock(ctor.outerDecl->GetTy(),
        paramList, curFile, true);
    if (!constructorExpr) {
        ctor.EnableAttr(Attribute::IS_BROKEN);
        return;
    }
    thisCall->args.push_back(CreateFuncArg(std::move(constructorExpr)));
    body.push_back(std::move(thisCall));
}

void JavaDesugarManager::AddJavaMirrorMethodBody(ClassLikeDecl& mirror, FuncDecl& fun, OwnedPtr<Expr> javaRefCall)
{
    if (fun.TestAttr(Attribute::ABSTRACT) && !IsSyntheticMirrorWrapper(mirror)) {
        return;
    }
    auto curFile = fun.curFile;
    CJC_NULLPTR_CHECK(curFile);

    // For mirror method, syntactic return type has to be specified, so it's safe to use it after parser.
    auto retTy = fun.funcBody->retType->GetTy();
    fun.funcBody->body = CreateBlock({}, retTy);
    auto& body = fun.funcBody->body->body;
    CJC_ASSERT_WITH_MSG(!fun.funcBody->paramLists.empty(), "paramLists cannot be empty");
    auto& paramList = *fun.funcBody->paramLists[0].get();
    OwnedPtr<Expr> methodCall;

    if (IsJArray(mirror) && !fun.TestAttr(Attribute::STATIC)) {
        auto& jniEnv = *StaticAs<ASTKind::VAR_DECL>(
            body.emplace_back(CreateTmpVarDecl(nullptr, lib.CreateGetJniEnvCall(curFile))).get());
        CJC_ASSERT_WITH_MSG(!mirror.generic->typeParameters.empty(), "JArray ctor must be generic");
        auto genericParam = mirror.generic->typeParameters[0].get();
        CJC_ASSERT(genericParam);
        methodCall = lib.CreateCFFICallArrayMethodCall(WithinFile(CreateRefExpr(jniEnv), curFile),
            std::move(javaRefCall), paramList, genericParam, GetArrayOperationKind(fun));
    } else {
        auto memberJNISignature = MemberJNISignature(utils, fun);
        auto methodBlock = lib.CreateJavaMethodCallBlock(curFile, paramList, retTy,
            fun.TestAttr(Attribute::STATIC), std::move(javaRefCall), memberJNISignature);
        if (!methodBlock) {
            fun.EnableAttr(Attribute::IS_BROKEN);
            return;
        }
        methodCall = WrapReturningLambdaCall(typeManager, std::move(methodBlock->body));
    }
    if (!methodCall) {
        fun.EnableAttr(Attribute::IS_BROKEN);
        return;
    }

<<<<<<< HEAD
    auto& methodCallRes = *StaticAs<ASTKind::VAR_DECL>(
        body.emplace_back(CreateTmpVarDecl(nullptr, std::move(methodCall))).get());
    methodCallRes.SetTy(methodCallRes.initializer->GetTy());
    CopyBasicInfo(methodCallRes.initializer.get(), &methodCallRes);
    methodCallRes.begin = fun.begin;
    methodCallRes.curFile = curFile;
    auto callResRef = WithinFile(CreateRefExpr(methodCallRes), curFile);
    OwnedPtr<Expr> retCallExpr = lib.ConvertJavaResultToCJ(std::move(callResRef), retTy, mirror);
=======
    auto methodCallRes = CreateTmpVarDecl(nullptr, std::move(methodCall));
    methodCallRes->SetTy(methodCallRes->initializer->GetTy());
    CopyBasicInfo(methodCallRes->initializer.get(), methodCallRes.get());
    methodCallRes->begin = fun.begin;
    methodCallRes->curFile = fun.curFile;

    auto callResRef = CreateRefExpr(*methodCallRes);
    CopyBasicInfo(methodCallRes.get(), callResRef.get());

    OwnedPtr<Expr> retCallExpr = nullptr;
    // cjmapping interface return param support lambda.
    if (fun.funcBody->retType->TyKind() == TypeKind::TYPE_FUNC &&
        fun.TestAttr(Attribute::CJ_MIRROR_JAVA_INTERFACE_FWD)) {
        retCallExpr = CreateGetCJLambdaCallExpr(std::move(callResRef), fun.funcBody->retType->DataTy(), mirror);
    } else {
        retCallExpr = lib.UnwrapJavaEntity(std::move(callResRef), fun.funcBody->retType->DataTy(), mirror);
    }

>>>>>>> 0c41e6d (feat: add local mode implementation)
    if (!retCallExpr) {
        fun.EnableAttr(Attribute::IS_BROKEN);
        return;
    }
<<<<<<< HEAD

    auto& retExpr = *StaticAs<ASTKind::RETURN_EXPR>(
        body.emplace_back(CreateReturnExpr(std::move(retCallExpr), fun.funcBody.get())).get());
    retExpr.SetTy(TypeManager::GetNothingTy());
    retExpr.refFuncBody = fun.funcBody.get();

    fun.funcBody->SetTy(TypeManager::GetNothingTy());
    if (IsSyntheticMirrorWrapper(mirror)) {
=======
    std::vector<OwnedPtr<Node>> blockNodes;
    auto retExpr = CreateReturnExpr(std::move(retCallExpr), fun.funcBody.get());
    retExpr->SetTy({TypeManager::GetNothingTy()});
    retExpr->refFuncBody = fun.funcBody.get();
    blockNodes.push_back(std::move(methodCallRes));
    blockNodes.push_back(std::move(retExpr));
    // Return type has to be specified, so it's safe to use it below
    fun.funcBody->body = CreateBlock(std::move(blockNodes), fun.funcBody->retType->GetTy());
    fun.funcBody->SetTy({TypeManager::GetNothingTy()});
    if (fun.TestAttr(Attribute::CJ_MIRROR_JAVA_INTERFACE_FWD)) {
        fun.funcBody->parentClassLike = &mirror;
    }
    if (IsSynthetic(mirror)) {
>>>>>>> 0c41e6d (feat: add local mode implementation)
        fun.DisableAttr(Attribute::ABSTRACT);
    }
}

void JavaDesugarManager::DesugarJavaMirrorMethod(FuncDecl& fun, ClassLikeDecl& mirror)
{
    CJC_ASSERT(!fun.TestAttr(Attribute::CONSTRUCTOR) && mirror.IsJavaMirror());
    AddJavaMirrorMethodBody(mirror, fun, CreateJavaRefCall(mirror, mirror.curFile));
}

void JavaDesugarManager::InsertJavaMirrorPropGetter(PropDecl& prop)
{
    auto& getter = *prop.getters.begin();
    getter->funcBody->body = CreateBlock({}, prop.GetTy());
    getter->funcBody->SetTy(prop.GetTy());
    auto& body = getter->funcBody->body->body;
    auto curFile = prop.curFile;
    auto& jniEnv = *StaticAs<ASTKind::VAR_DECL>(
        body.emplace_back(CreateTmpVarDecl(nullptr, lib.CreateGetJniEnvCall(curFile))).get());

    auto& mirror = *StaticAs<ASTKind::CLASS_LIKE_DECL>(prop.outerDecl);
    OwnedPtr<CallExpr> jniGetterCall;
    auto isArrayGetLength =
        IsJArray(*prop.outerDecl) && GetArrayOperationKind(prop) == ArrayOperationKind::GET_LENGTH;
    if (isArrayGetLength) {
        jniGetterCall = lib.CreateCFFIArrayLengthGetCall(CreateJavaRefCall(mirror, curFile), curFile);
    } else if (prop.TestAttr(Attribute::STATIC)) {
        jniGetterCall = lib.CreateGetStaticFieldCall(WithinFile(CreateRefExpr(jniEnv), curFile),
            utils.GetJavaClassNormalizeSignature(*mirror.GetTy()), GetJavaMemberName(prop),
            utils.GetJavaTypeSignature(*prop.GetTy()));
    } else {
        jniGetterCall = lib.CreateGetFieldCall(WithinFile(CreateRefExpr(jniEnv), curFile),
            CreateJavaRefCall(mirror, curFile),
            utils.GetJavaClassNormalizeSignature(*prop.outerDecl->GetTy()), GetJavaMemberName(prop),
            utils.GetJavaTypeSignature(*prop.GetTy()));
    }

    auto& jniGetterCallRes = *StaticAs<ASTKind::VAR_DECL>(
        body.emplace_back(CreateTmpVarDecl(nullptr, std::move(jniGetterCall))).get());
    jniGetterCallRes.SetTy(jniGetterCallRes.initializer->GetTy());
    CopyBasicInfo(jniGetterCallRes.initializer.get(), &jniGetterCallRes);

    auto callResRef = CreateRefExpr(jniGetterCallRes);
    CopyBasicInfo(&jniGetterCallRes, callResRef.get());

    auto outerDecl = As<ASTKind::CLASS_LIKE_DECL>(prop.outerDecl);
    OwnedPtr<Expr> unwrapJavaEntityCall = isArrayGetLength || !outerDecl
        ? std::move(callResRef)
        : lib.UnwrapJavaEntity(std::move(callResRef), prop.DataTy(), *outerDecl);
    if (!unwrapJavaEntityCall) {
        prop.EnableAttr(Attribute::IS_BROKEN);
        prop.outerDecl->EnableAttr(Attribute::HAS_BROKEN);
        return;
    }
    body.emplace_back(CreateReturnExpr(std::move(unwrapJavaEntityCall), getter->funcBody.get()));
}

void JavaDesugarManager::InsertJavaMirrorPropSetter(PropDecl& prop)
{
    static auto unitTy = TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT);

    auto& mirror = *StaticAs<ASTKind::CLASS_LIKE_DECL>(prop.outerDecl);
    auto curFile = prop.curFile;

    auto& setter = *prop.setters.begin();
    setter->funcBody->body = CreateBlock({}, prop.GetTy());
    setter->funcBody->SetTy(unitTy);

    auto& body = setter->funcBody->body->body;
    auto& jniEnv = *StaticAs<ASTKind::VAR_DECL>(
        body.emplace_back(CreateTmpVarDecl(nullptr, lib.CreateGetJniEnvCall(curFile))).get());

    OwnedPtr<CallExpr> jniSetterCall;

    CJC_ASSERT_WITH_MSG(!prop.setters.empty(), "expected at least one setter");
    CJC_ASSERT_WITH_MSG(!prop.setters[0]->funcBody->paramLists.empty(), "setter paramLists cannot be empty");
    CJC_ASSERT_WITH_MSG(!prop.setters[0]->funcBody->paramLists[0]->params.empty(), "setter params cannot be empty");
    auto paramRef = WithinFile(CreateRefExpr(*prop.setters[0]->funcBody->paramLists[0]->params[0]), curFile);

    if (prop.TestAttr(Attribute::STATIC)) {
        jniSetterCall = lib.CreateSetStaticFieldCall(WithinFile(CreateRefExpr(jniEnv), curFile),
            utils.GetJavaClassNormalizeSignature(*mirror.GetTy()), GetJavaMemberName(prop),
            utils.GetJavaTypeSignature(*prop.GetTy()), std::move(paramRef));
    } else {
        jniSetterCall = lib.CreateSetFieldCall(WithinFile(CreateRefExpr(jniEnv), curFile),
            CreateJavaRefCall(mirror, curFile),
            MemberJNISignature(utils, prop),
            std::move(paramRef));
    }
<<<<<<< HEAD
    body.emplace_back(std::move(jniSetterCall));
=======

    auto block = CreateBlock({}, prop.GetTy());
    auto unitTy = TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT);

    auto& setter = *prop.setters.begin();
    auto& setterBody = setter->funcBody;
    setterBody->body = std::move(block);
    setterBody->SetTy({unitTy});

    setterBody->body->body.emplace_back(std::move(jniSetterCall));
>>>>>>> 0c41e6d (feat: add local mode implementation)
}

void JavaDesugarManager::DesugarJavaMirrorProp(PropDecl& prop)
{
    CJC_ASSERT(prop.outerDecl);
    InsertJavaMirrorPropGetter(prop);

    if (prop.isVar) {
        InsertJavaMirrorPropSetter(prop);
    }
}

void JavaDesugarManager::DesugarJavaMirror(ClassDecl& mirror)
{
    Ptr<FuncDecl> generatedCtor = GetJavaMirrorWrappingConstructor(mirror);
    for (auto& decl : mirror.GetMemberDecls()) {
        if (decl->TestAttr(Attribute::IS_BROKEN)) {
            continue;
        }
        if (auto fd = As<ASTKind::FUNC_DECL>(decl.get())) {
            if (fd->IsFinalizer() || IsJavaRefGetter(*fd) || IsWrappingConstructorOfJavaMirror(*fd)) {
                continue;
            }

            if (fd->TestAttr(Attribute::CONSTRUCTOR)) {
                DesugarJavaMirrorConstructor(*fd, *generatedCtor);
            } else {
                DesugarJavaMirrorMethod(*fd, mirror);
            }
        } else if (auto prop = As<ASTKind::PROP_DECL>(decl.get())) {
            DesugarJavaMirrorProp(*prop);
        }
    }
}

void JavaDesugarManager::DesugarJavaMirror(InterfaceDecl& mirror)
{
    for (auto& decl : mirror.GetMemberDecls()) {
        if (decl->TestAttr(Attribute::IS_BROKEN)) {
            continue;
        }
        bool isConstructor = decl->TestAttr(Attribute::CONSTRUCTOR);

        if (FuncDecl* fd = As<ASTKind::FUNC_DECL>(decl.get()); fd && !isConstructor) {
            bool isStatic = decl->TestAttr(Attribute::STATIC);
            bool isDefault = decl->TestAttr(Attribute::JAVA_HAS_DEFAULT);
            if (!IsJavaRefGetter(*fd) && (isStatic || isDefault)) {
                DesugarJavaMirrorMethod(*fd, mirror);
            }
        }
    }
}

void JavaDesugarManager::GenerateInMirror(ClassDecl& classDecl, bool doStub)
{
    // Note: on [doStub] = true, no javaref field references may exist
    if (IsJObject(classDecl) && doStub) {
        // Generate java reference field for JObject. This field is accessible inside predecessors
        InsertJavaRefVarDecl(classDecl);
    }

    InsertJavaMirrorCtor(classDecl, doStub);

    if (!doStub) {
        InsertJavaMirrorHasInited(classDecl);
        InsertJavaMirrorFinalizer(classDecl);
    }

    if (&classDecl == utils.GetJStringDecl()) {
        InsertJStringOfStringCtor(classDecl, doStub);
    }

    if (!doStub && IsJArray(classDecl)) {
        InsertArrayJavaEntityGet(classDecl);
        InsertArrayJavaEntitySet(classDecl);
    }

    if (!doStub && IsJObject(classDecl)) {
        InsertJavaRefGetterWithBody(classDecl);
    }
}

void JavaDesugarManager::GenerateInSynthetic(ClassDecl& cd)
{
    if (IsSyntheticMirrorWrapper(cd)) {
        if (auto id = DynamicCast<InheritableDecl*>(&cd)) {
            GenerateSyntheticClassMemberStubs(cd, memberMap.at(id));
        } else {
            CJC_ABORT_WITH_MSG("Illegal state: synthetic classes expected to be InheritableDecl*");
        }
    }
}

<<<<<<< HEAD
=======
void JavaDesugarManager::ReplaceCallsWithArrayJavaEntityGet(File& file)
{
    Walker(&file, Walker::GetNextWalkerID(), [this, &file](auto node) {
        if (!node->IsSamePackage(*file.curPackage)) {
            return VisitAction::WALK_CHILDREN;
        }

        Ptr<CallExpr> callExpr = As<ASTKind::CALL_EXPR>(node);
        if (!callExpr) {
            return VisitAction::WALK_CHILDREN;
        }

        auto funcDecl = callExpr->resolvedFunction;
        if (!funcDecl || !funcDecl->outerDecl || !IsJArray(*funcDecl->outerDecl) ||
            funcDecl->identifier != "[]" || callExpr->args.size() != 1
        ) {
            return VisitAction::WALK_CHILDREN;
        }

        auto ma = As<ASTKind::MEMBER_ACCESS>(callExpr->baseFunc);
        CJC_ASSERT_WITH_MSG(!ma->baseExpr->DataTy()->TyArgs().empty(), "JArray type must be generic");
        auto arrayElementType = ma->baseExpr->DataTy()->TyArg(0);
        if (!arrayElementType->IsClass() && !arrayElementType->IsCoreOptionType()) {
            return VisitAction::WALK_CHILDREN;
        }

        static auto arrayJavaEntityGetDecl = lib.FindArrayJavaEntityGetDecl(
            *As<ASTKind::CLASS_DECL>(funcDecl->outerDecl));
        CJC_ASSERT(arrayJavaEntityGetDecl);
        auto newCallExpr = ASTCloner::Clone(callExpr);
        newCallExpr->resolvedFunction = arrayJavaEntityGetDecl;
        auto base = As<ASTKind::MEMBER_ACCESS>(newCallExpr->baseFunc);
        base->target = arrayJavaEntityGetDecl;
        base->SetTy(arrayJavaEntityGetDecl->GetTy());
        callExpr->desugarExpr = lib.UnwrapJavaEntity(
            std::move(newCallExpr), arrayElementType, *As<ASTKind::CLASS_LIKE_DECL>(funcDecl->outerDecl));
        callExpr->desugarArgs = std::nullopt;

        return VisitAction::WALK_CHILDREN;
    }).Walk();
}

void JavaDesugarManager::ReplaceCallsWithArrayJavaEntitySet(File& file)
{
    Walker(&file, Walker::GetNextWalkerID(), [this, &file](auto node) {
        if (!node->IsSamePackage(*file.curPackage)) {
            return VisitAction::WALK_CHILDREN;
        }

        Ptr<CallExpr> callExpr = As<ASTKind::CALL_EXPR>(node);
        if (!callExpr) {
            return VisitAction::WALK_CHILDREN;
        }

        auto funcDecl = callExpr->resolvedFunction;
        if (!funcDecl || !funcDecl->outerDecl || !IsJArray(*funcDecl->outerDecl) ||
            funcDecl->identifier != "[]" || callExpr->args.size() != 2 // index and value
        ) {
            // Only calls to correct array "set" should be desugared.
            // For example, do not desugar if arguments count is wrong (in case difference from index and value)
            return VisitAction::WALK_CHILDREN;
        }

        auto ma = As<ASTKind::MEMBER_ACCESS>(callExpr->baseFunc);
        CJC_ASSERT_WITH_MSG(!ma->baseExpr->DataTy()->TyArgs().empty(), "JArray type must be generic");
        auto arrayElementType = ma->baseExpr->DataTy()->TyArg(0);
        if (!arrayElementType->IsClass() && !arrayElementType->IsCoreOptionType()) {
            return VisitAction::WALK_CHILDREN;
        }

        static auto arrayJavaEntitySetDecl = lib.FindArrayJavaEntitySetDecl(
            *As<ASTKind::CLASS_DECL>(funcDecl->outerDecl));
        CJC_ASSERT(arrayJavaEntitySetDecl);
        auto newCallExpr = ASTCloner::Clone(callExpr);
        newCallExpr->resolvedFunction = arrayJavaEntitySetDecl;

        newCallExpr->args[1] = ASTCloner::Clone(newCallExpr->args[1].get());
        newCallExpr->args[1]->expr = lib.WrapJavaEntity(std::move(newCallExpr->args[1]->expr));
        newCallExpr->desugarArgs = std::nullopt;
        callExpr->desugarArgs = std::nullopt;
        auto base = As<ASTKind::MEMBER_ACCESS>(newCallExpr->baseFunc);
        base->target = arrayJavaEntitySetDecl;
        base->SetTy(arrayJavaEntitySetDecl->GetTy());
        callExpr->args[1] = ASTCloner::Clone(newCallExpr->args[1].get());

        callExpr->baseFunc = ASTCloner::Clone(newCallExpr->baseFunc.get());
        callExpr->resolvedFunction = newCallExpr->resolvedFunction;
        callExpr->desugarExpr = std::move(newCallExpr);

        return VisitAction::WALK_CHILDREN;
    }).Walk();
}

>>>>>>> 0c41e6d (feat: add local mode implementation)
void JavaDesugarManager::GenerateInMirrors(File& file, bool doStub)
{
    auto pkg = file.curPackage;

    if (!doStub) {
        std::once_flag flag;
        std::call_once(flag, [this, &pkg]() {
            TypeCheckUtil::GenerateGetTypeForTypeParamIntrinsic(*pkg, typeManager);
        });
    }

    for (auto& decl : file.decls) {
        if (auto cldecl = As<ASTKind::CLASS_LIKE_DECL>(decl.get())) {
            if (!cldecl->IsJavaMirror()) {
                continue;
            }
            if (cldecl->TestAttr(Attribute::IS_BROKEN)) {
                return;
            }
            if (auto classDecl = As<ASTKind::CLASS_DECL>(cldecl)) {
                GenerateInMirror(*classDecl, doStub);
                if (doStub) {
                    GenerateInSynthetic(*classDecl);
                }
            }
            if (doStub && cldecl->astKind == ASTKind::INTERFACE_DECL) {
                InsertAbstractJavaRefGetter(*cldecl);
            }
        }
    }
}

void JavaDesugarManager::DesugarMirrors(File& file)
{
    for (auto& decl : file.decls) {
        if (auto cldecl = As<ASTKind::CLASS_LIKE_DECL>(decl.get())) {
            if (cldecl->IsJavaMirror()) {
                if (cldecl->TestAttr(Attribute::IS_BROKEN)) {
                    return;
                }
                if (auto cd = As<ASTKind::CLASS_DECL>(cldecl)) {
                    DesugarJavaMirror(*cd);
                } else if (auto iDecl = As<ASTKind::INTERFACE_DECL>(cldecl)) {
                    DesugarJavaMirror(*iDecl);
                }
            }
        }
    }
}

void JavaDesugarManager::InsertJStringOfStringCtor(ClassDecl& decl, bool doStub)
{
    static const std::string stringParamName = "s";
    static Ptr<FuncDecl> generatedCtor;

    CJC_ASSERT(&decl == utils.GetJStringDecl());

    if (!doStub) {
        auto curFile = decl.curFile;
        // After constructor stub insertion, its body is filled on doStub = false
        CJC_NULLPTR_CHECK(generatedCtor);
        auto& param = generatedCtor->funcBody->paramLists[0]->params[0];
        auto jObjectCtor = GetJavaMirrorWrappingConstructor(decl);
        auto convertCall = CreateCall(lib.GetCangjieStringToJava(), curFile,
            lib.CreateGetJniEnvCall(curFile), WithinFile(CreateRefExpr(*param), curFile));

        auto superCall = CreateSuperCall(decl, *jObjectCtor, jObjectCtor->DataTy());
        superCall->args.emplace_back(CreateFuncArg(std::move(convertCall)));

        generatedCtor->funcBody->body->body.emplace_back(std::move(superCall));
        return;
    }

    // Initially, on doStub = true, it inserts constructor stub
    auto& stringDecl = utils.GetStringDecl();
    auto param = CreateFuncParam(stringParamName, CreateRefType(stringDecl), nullptr, stringDecl.GetTy());

    auto ctorFuncTy = typeManager.GetFunctionTy({param->GetTy()}, decl.GetTy());

    std::vector<OwnedPtr<FuncParam>> ctorParams;
    ctorParams.emplace_back(std::move(param));

    std::vector<OwnedPtr<FuncParamList>> paramLists;
    paramLists.emplace_back(CreateFuncParamList(std::move(ctorParams)));

    auto ctorFuncBody =
        CreateFuncBody(std::move(paramLists), CreateRefType(decl), CreateBlock({}, decl.GetTy()), decl.GetTy());

    auto fd = CreateFuncDecl("init", std::move(ctorFuncBody), {ctorFuncTy});
    fd->fullPackageName = decl.fullPackageName;
    fd->outerDecl = &decl;
    fd->funcBody->funcDecl = fd.get();
    fd->constructorCall = ConstructorCall::SUPER;
    fd->funcBody->parentClassLike = &decl;
    fd->EnableAttr(Attribute::PUBLIC, Attribute::CONSTRUCTOR, Attribute::IN_CLASSLIKE);
    fd->curFile = decl.curFile;
    generatedCtor = fd.get();
    decl.body->decls.emplace_back(std::move(fd));
}

} // namespace Cangjie::Interop::Java
