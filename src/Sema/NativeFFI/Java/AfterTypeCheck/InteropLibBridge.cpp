// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "InteropLibBridge.h"
#include "Desugar/AfterTypeCheck.h"
#include "JavaDesugarManager.h"
#include "NativeFFI/Java/AfterTypeCheck/Utils.h"
#include "NativeFFI/Utils.h"
#include "TypeCheckUtil.h"
#include "cangjie/AST/Clone.h"
#include "cangjie/AST/Create.h"
#include "cangjie/AST/Utils.h"
#include "cangjie/Utils/CastingTemplate.h"
#include "cangjie/Utils/CheckUtils.h"
#include <utility>

namespace {

// vars
constexpr auto INTEROPLIB_VERSION_FIELD_ID = "INTEROPLIB_VERSION";
constexpr auto JAVA_CONSTRUCTOR = "<init>";

// types
constexpr auto INTEROPLIB_JNI_ENV_PTR_ID = "JNIEnv_ptr";
constexpr auto INTEROPLIB_JNI_JLONG_ID = "jlong";
constexpr auto INTEROPLIB_JNI_JOBJECT_ID = "jobject";
constexpr auto INTEROPLIB_JAVA_ENTITY_KIND = "Java_CFFI_JavaEntityKind";
constexpr auto INTEROPLIB_JAVA_ENTITY_KIND_JOBJECT = "JOBJECT";
constexpr auto INTEROPLIB_JAVA_ENTITY_KIND_JBYTE = "JBYTE";
constexpr auto INTEROPLIB_JAVA_ENTITY_KIND_JSHORT = "JSHORT";
constexpr auto INTEROPLIB_JAVA_ENTITY_KIND_JCHAR = "JCHAR";
constexpr auto INTEROPLIB_JAVA_ENTITY_KIND_JINT = "JINT";
constexpr auto INTEROPLIB_JAVA_ENTITY_KIND_JLONG = "JLONG";
constexpr auto INTEROPLIB_JAVA_ENTITY_KIND_JFLOAT = "JFLOAT";
constexpr auto INTEROPLIB_JAVA_ENTITY_KIND_JDOUBLE = "JDOUBLE";
constexpr auto INTEROPLIB_JAVA_ENTITY_KIND_JBOOLEAN = "JBOOLEAN";
constexpr auto INTEROPLIB_JNI_JNINativeInterface_ID = "JNINativeInterface_";

// funcs
constexpr auto INTEROPLIB_JNI_GET_ENV_ID = "Java_CFFI_get_env";
constexpr auto INTEROPLIB_CFFI_GET_METHOD_ID = "Java_CFFI_getMethodID";
constexpr auto INTEROPLIB_JNI_HANDLE_PENDING_EXCEPTION_DECL_ID = "handlePendingException";
constexpr auto INTEROPLIB_CFFI_NEW_GLOBAL_REF_ID = "Java_CFFI_newGlobalReference";
constexpr auto INTEROPLIB_CFFI_DELETE_GLOBAL_REF_ID = "Java_CFFI_deleteGlobalReference";
constexpr auto INTEROPLIB_CFFI_NEW_JAVA_OBJECT_ID = "Java_CFFI_newJavaObject";
constexpr auto INTEROPLIB_CFFI_NEW_JAVA_ARRAY_ID = "Java_CFFI_newJavaArray";
constexpr auto INTEROPLIB_CFFI_NEW_JAVA_PROXY_OBJECT_FOR_CJMAPPING_ID = "Java_CFFI_newJavaProxyObjectForCJMapping";
constexpr auto INTEROPLIB_CFFI_JAVA_ARRAY_GET_ID = "Java_CFFI_arrayGet";
constexpr auto INTEROPLIB_CFFI_JAVA_ARRAY_SET_ID = "Java_CFFI_arraySet";
constexpr auto INTEROPLIB_CFFI_JAVA_ARRAY_GET_LENGTH = "Java_CFFI_arrayGetLength";
constexpr auto INTEROPLIB_CFFI_JAVA_ENTITY_JOBJECT_ID = "Java_CFFI_JavaEntityJobject";
constexpr auto INTEROPLIB_CFFI_JAVA_ENTITY_NULL_ID = "Java_CFFI_JavaEntityJobjectNull";
constexpr auto INTEROPLIB_CFFI_JAVA_ENTITY_IS_NULL_ID = "isNull";
constexpr auto INTEROPLIB_JNI_PUT_TO_REGISTRY_DECL_ID = "Java_CFFI_put_to_registry_1";
constexpr auto INTEROPLIB_JNI_PUT_SET_TO_REGISTRY_DECL_ID = "Java_CFFI_putToRegistry";
constexpr auto INTEROPLIB_JNI_REMOVE_FROM_REGISTRY_DECL_ID = "Java_CFFI_removeFromRegistry";
constexpr auto INTEROPLIB_CFFI_CALL_STATIC_METHOD_DECL_ID = "Java_CFFI_callStaticMethod";
constexpr auto INTEROPLIB_CFFI_UNWRAP_JAVA_ENTITY_METHOD_DECL_ID = "Java_CFFI_unwrapJavaEntityAsValue";
constexpr auto INTEROPLIB_CFFI_GET_FROM_REGISTRY_METHOD_DECL_ID = "Java_CFFI_getFromRegistry";
constexpr auto INTEROPLIB_CFFI_GET_FROM_REGISTRY_OPTION_METHOD_DECL_ID = "Java_CFFI_getFromRegistryOption";
constexpr auto INTEROPLIB_CFFI_GET_FIELD_METHOD_DECL_ID = "Java_CFFI_getField";
constexpr auto INTEROPLIB_CFFI_GET_STATIC_FIELD_METHOD_DECL_ID = "Java_CFFI_getStaticField";
constexpr auto INTEROPLIB_CFFI_SET_FIELD_METHOD_DECL_ID = "Java_CFFI_setField";
constexpr auto INTEROPLIB_CFFI_SET_STATIC_FIELD_METHOD_DECL_ID = "Java_CFFI_setStaticField";
constexpr auto INTEROPLIB_CFFI_ENSURE_NOT_NULL_METHOD_DECL_ID = "Java_CFFI_ensure_not_null";
constexpr auto INTEROPLIB_CFFI_GET_JAVA_ENTITY_OR_NULL_METHOD_DECL_ID = "Java_CFFI_getJavaEntityOrNull";
constexpr auto INTEROPLIB_CFFI_IS_INSTANCE_OF_DECL_ID = "Java_CFFI_isInstanceOf";
constexpr auto INTEROPLIB_CFFI_GET_FROM_REGISTRY_BY_ENTITY_OPTION_DECL_ID = "Java_CFFI_getFromRegistryByObjOption";
constexpr auto INTEROPLIB_CFFI_GET_FROM_REGISTRY_BY_ENTITY_DECL_ID = "Java_CFFI_getFromRegistryByObj";
constexpr auto INTEROPLIB_CFFI_GET_REGISTRY_ID = "Java_CFFI_getRegistryId";
constexpr auto INTEROPLIB_CFFI_GET_REGISTRY_ID_OR_NONE = "Java_CFFI_getRegistryIdOrNone";
constexpr auto INTEROPLIB_CFFI_JAVA_STRING_TO_CANGJIE = "Java_CFFI_JavaStringToCangjie";
constexpr auto INTEROPLIB_CFFI_CANGJIE_STRING_TO_JAVA = "Java_CFFI_CangjieStringToJava";
constexpr auto INTEROPLIB_CFFI_WITH_EXCEPTION_HANDLING_ID = "withExceptionHandling";
constexpr auto INTEROPLIB_CFFI_JAVA_CFFI_CLASS_ID = "Java_CFFI_ClassInit";
constexpr auto INTEROPLIB_CFFI_PARSE_METHOD_SIGNATURE_ID = "Java_CFFI_parseMethodSignature";
constexpr auto INTEROPLIB_CFFI_PARSE_COMPONENT_SIGNATURE_ID = "Java_CFFI_parseComponentSignature";
constexpr auto INTEROPLIB_CFFI_JAVA_CALLNEST_ID = "Java_CFFI_JavaCallNestInit";
constexpr auto INTEROPLIB_CFFI_JAVA_METHODID_CONSTR_ID = "Java_CFFI_MethodIDConstr";
constexpr auto INTEROPLIB_CFFI_JAVA_METHODID_CONSTR_STATIC_ID = "Java_CFFI_MethodIDConstrStatic";
constexpr auto INTEROPLIB_CFFI_JAVA_FIELDID_CONSTR_ID = "Java_CFFI_FieldIDConstr";
constexpr auto INTEROPLIB_CFFI_JAVA_FIELDID_CONSTR_STATIC_ID = "Java_CFFI_FieldIDConstrStatic";
constexpr auto INTEROPLIB_JAVA_OBJECT_CONTROLLER = "JavaObjectController";
constexpr auto DELETE_LOCAL_REF = "deleteLocalRef";
constexpr auto DETACH_CJ_OBJECT = "detachCJObject";
constexpr auto ATTACH_CJ_OBJECT = "attachCJObject";
constexpr auto GET_JAVALAMBDA_OBJECT_DECL_ID = "getJavaLambdaObject";
constexpr auto GET_JAVALAMBDA_ENTITY_DECL_ID = "getJavaLambdaEntity";

} // namespace

using namespace Cangjie;
using namespace AST;
using namespace TypeCheckUtil;
using namespace Sema::Desugar::AfterTypeCheck;

namespace Cangjie::Interop::Java {

using namespace Cangjie::Native::FFI;

// declarations

Ptr<TypeAliasDecl> InteropLibBridge::GetJobjectDecl()
{
    return GetInteropLibDecl<ASTKind::TYPE_ALIAS_DECL>(INTEROPLIB_JNI_JOBJECT_ID);
}

Ptr<StructDecl> InteropLibBridge::GetJavaEntityDecl() const
{
    return GetInteropLibDecl<ASTKind::STRUCT_DECL>(INTEROPLIB_CFFI_JAVA_ENTITY);
}

Ptr<ClassDecl> InteropLibBridge::GetJavaObjectControllerDecl()
{
    return GetInteropLibDecl<ASTKind::CLASS_DECL>(INTEROPLIB_JAVA_OBJECT_CONTROLLER);
}

Ptr<TypeAliasDecl> InteropLibBridge::GetJniEnvPtrDecl() const
{
    return GetInteropLibDecl<ASTKind::TYPE_ALIAS_DECL>(INTEROPLIB_JNI_ENV_PTR_ID);
}

Ptr<EnumDecl> InteropLibBridge::GetJavaEntityKindDecl()
{
    return GetInteropLibDecl<ASTKind::ENUM_DECL>(INTEROPLIB_JAVA_ENTITY_KIND);
}

Decl& InteropLibBridge::GetJavaEntityKindJObject()
{
    auto entityKindDecl = GetJavaEntityKindDecl();
    return *LookupEnumMember(entityKindDecl, INTEROPLIB_JAVA_ENTITY_KIND_JOBJECT);
}

Ptr<FuncDecl> InteropLibBridge::GetNewGlobalRefDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_NEW_GLOBAL_REF_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetDeleteGlobalRefDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_DELETE_GLOBAL_REF_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetGetJniEnvDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_JNI_GET_ENV_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetCreateJavaEntityJobjectDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_JAVA_ENTITY_JOBJECT_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetCreateJavaEntityNullDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_JAVA_ENTITY_NULL_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetNewJavaObjectDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_NEW_JAVA_OBJECT_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetNewJavaArrayDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_NEW_JAVA_ARRAY_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetNewJavaProxyObjectForCJMappingDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_NEW_JAVA_PROXY_OBJECT_FOR_CJMAPPING_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetJavaArrayGetDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_JAVA_ARRAY_GET_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetJavaArraySetDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_JAVA_ARRAY_SET_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetJavaArrayGetLengthDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_JAVA_ARRAY_GET_LENGTH);
}

Ptr<StructDecl> InteropLibBridge::GetJNINativeInterfaceDecl()
{
    return GetInteropLibDecl<ASTKind::STRUCT_DECL>(INTEROPLIB_JNI_JNINativeInterface_ID);
}

Ptr<FuncDecl> InteropLibBridge::FindAsJValueDecl() const
{
    static Ptr<FuncDecl> result = nullptr;
    if (result != nullptr) {
        return result;
    }

    auto entityDecl = GetJavaEntityDecl();
    for (auto& decl : entityDecl->GetMemberDeclPtrs()) {
        if (decl->identifier == "asJValue") {
            result = As<ASTKind::FUNC_DECL>(decl.get());
            break;
        }
    }

    CJC_NULLPTR_CHECK(result);
    return result;
}

Ptr<FuncDecl> InteropLibBridge::FindAsJObjectDecl() const
{
    static Ptr<FuncDecl> result = nullptr;
    if (result != nullptr) {
        return result;
    }

    auto entityDecl = GetJavaEntityDecl();
    for (auto& decl : entityDecl->GetMemberDeclPtrs()) {
        if (decl->identifier == "asJObject") {
            result = As<ASTKind::FUNC_DECL>(decl.get());
            break;
        }
    }

    CJC_NULLPTR_CHECK(result);
    return result;
}

Ptr<VarDecl> InteropLibBridge::GetJNINativeInterfaceField(const std::string& name){
    auto st = GetJNINativeInterfaceDecl();
    if (!st) {
        return nullptr;
    }

    for (auto& decl : st->GetMemberDecls()) {
        auto vd = DynamicCast<VarDecl*>(decl.get());
        if (vd && vd->identifier.Val() == name) {
            return vd;
        }
    }

    return nullptr;
}

Ptr<FuncDecl> InteropLibBridge::GetJNIHandlePendingExceptionDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(
        INTEROPLIB_JNI_HANDLE_PENDING_EXCEPTION_DECL_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetGetMethodIdDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_GET_METHOD_ID);
}

Ptr<Ty> InteropLibBridge::GetJValueTy()
{
    return typeManager.GetPrimitiveTy(TypeKind::TYPE_UINT64);
}

Ptr<FuncDecl> InteropLibBridge::GetCallStaticMethodDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_CALL_STATIC_METHOD_DECL_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetRemoveFromRegistryDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_JNI_REMOVE_FROM_REGISTRY_DECL_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetPutToRegistryDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_JNI_PUT_TO_REGISTRY_DECL_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetPutSetToRegistryDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_JNI_PUT_SET_TO_REGISTRY_DECL_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetUnwrapJavaEntityDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_UNWRAP_JAVA_ENTITY_METHOD_DECL_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetGetRegistryIdDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_GET_REGISTRY_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetGetRegistryIdOrNoneDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_GET_REGISTRY_ID_OR_NONE);
}

Ptr<FuncDecl> InteropLibBridge::GetGetFromRegistryByEntityOptionDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_GET_FROM_REGISTRY_BY_ENTITY_OPTION_DECL_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetGetFromRegistryByEntityDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_GET_FROM_REGISTRY_BY_ENTITY_DECL_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetJavaStringToCangjie()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_JAVA_STRING_TO_CANGJIE);
}

Ptr<FuncDecl> InteropLibBridge::GetCangjieStringToJava()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_CANGJIE_STRING_TO_JAVA);
}

Ptr<FuncDecl> InteropLibBridge::GetGetFromRegistryDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_GET_FROM_REGISTRY_METHOD_DECL_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetGetFromRegistryOptionDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_GET_FROM_REGISTRY_OPTION_METHOD_DECL_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetGetFieldDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_GET_FIELD_METHOD_DECL_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetGetStaticFieldDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_GET_STATIC_FIELD_METHOD_DECL_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetSetFieldDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_SET_FIELD_METHOD_DECL_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetSetStaticFieldDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_SET_STATIC_FIELD_METHOD_DECL_ID);
}

Ptr<VarDecl> InteropLibBridge::GetInteropLibVersionVarDecl()
{
    return GetInteropLibDecl<ASTKind::VAR_DECL>(INTEROPLIB_VERSION_FIELD_ID, true);
}

Ptr<FuncDecl> InteropLibBridge::GetEnsureNotNullDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_ENSURE_NOT_NULL_METHOD_DECL_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetGetJavaEntityOrNullDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_GET_JAVA_ENTITY_OR_NULL_METHOD_DECL_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetIsInstanceOf()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_IS_INSTANCE_OF_DECL_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetWithExceptionHandlingDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_WITH_EXCEPTION_HANDLING_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetJClassDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_JAVA_CFFI_CLASS_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetParseMethodSignatureDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_PARSE_METHOD_SIGNATURE_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetParseComponentSignatureDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_PARSE_COMPONENT_SIGNATURE_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetCallNestDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_JAVA_CALLNEST_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetMethodIdConstr()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_JAVA_METHODID_CONSTR_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetMethodIdConstrStatic()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_JAVA_METHODID_CONSTR_STATIC_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetFieldIdConstr()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_JAVA_FIELDID_CONSTR_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetFieldIdConstrStatic()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(INTEROPLIB_CFFI_JAVA_FIELDID_CONSTR_STATIC_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetJavaLambdaObjectDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(GET_JAVALAMBDA_OBJECT_DECL_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetJavaLambdaEntityDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(GET_JAVALAMBDA_ENTITY_DECL_ID);
}

Ptr<FuncDecl> InteropLibBridge::GetJavaObjectControllerMethodDecl(std::string methodName)
{
    auto classDecl = GetJavaObjectControllerDecl();
    for (auto& member : classDecl->GetMemberDecls()) {
        auto funcDecl = As<ASTKind::FUNC_DECL>(member.get());
        if (funcDecl && funcDecl->identifier == methodName) {
            return funcDecl;
        }
    }

    CJC_ASSERT(false && "JavaObjectController doesn't have method");
    return nullptr;
}

Ptr<FuncDecl> InteropLibBridge::GetJavaObjectControllerInitDecl()
{
    return GetJavaObjectControllerMethodDecl("init");
}

Ptr<FuncDecl> InteropLibBridge::GetAttachCJObjectDecl()
{
    return GetJavaObjectControllerMethodDecl(ATTACH_CJ_OBJECT);
}

Ptr<FuncDecl> InteropLibBridge::GetDetachCJObjectDecl()
{
    return GetJavaObjectControllerMethodDecl(DETACH_CJ_OBJECT);
}

Ptr<FuncDecl> InteropLibBridge::GetDeleteLocalRefDecl()
{
    return GetInteropLibDecl<ASTKind::FUNC_DECL>(DELETE_LOCAL_REF);
}
// ty

Ptr<Ty> InteropLibBridge::GetJNIEnvPtrTy()
{
    auto decl = GetJniEnvPtrDecl();
    if (!decl) {
        return nullptr;
    }
    return decl->type->GetTy();
}

Ptr<Ty> InteropLibBridge::GetJavaEntityTy()
{
    auto decl = GetJavaEntityDecl();
    if (!decl) {
        return nullptr;
    }

    return decl->GetTy();
}

Ty& InteropLibBridge::GetJniJobjectTy() const
{
    static auto ty = typeManager.GetPointerTy(typeManager.GetPrimitiveTy(TypeKind::TYPE_UNIT));
    CJC_NULLPTR_CHECK(ty);
    return *ty;
}

OwnedPtr<CallExpr> InteropLibBridge::CreateGetJavaLambdaObjectCall(
    OwnedPtr<RefExpr> refExpr, std::string classSign, Ptr<File> curFile)
{
    auto fd = GetJavaLambdaObjectDecl();
    return CreateGetJavaLambdaCall(fd, std::move(refExpr), classSign, curFile);
}

OwnedPtr<CallExpr> InteropLibBridge::CreateGetJavaLambdaEntityCall(
    OwnedPtr<RefExpr> refExpr, std::string classSign, Ptr<File> curFile)
{
    auto fd = GetJavaLambdaEntityDecl();
    return CreateGetJavaLambdaCall(fd, std::move(refExpr), classSign, curFile);
}

OwnedPtr<CallExpr> InteropLibBridge::CreateGetJavaLambdaCall(
    Ptr<FuncDecl> fd, OwnedPtr<RefExpr> refExpr, std::string classSign, Ptr<File> curFile)
{
    auto strTy = fd->funcBody->paramLists[0]->params[1]->GetTy();
    auto classSignExpr = CreateLitConstExpr(LitConstKind::STRING, classSign, strTy);
    auto call = CreateCall(fd, curFile, std::move(refExpr), std::move(classSignExpr));
    CJC_NULLPTR_CHECK(call);
    return call;
}

OwnedPtr<PointerExpr> InteropLibBridge::CreateJobjectNull()
{
    auto pointerExpr = MakeOwnedNode<PointerExpr>();
    pointerExpr->type = MakeOwnedNode<Type>();
    pointerExpr->type->SetTy(&GetJniJobjectTy());
    pointerExpr->SetTy(pointerExpr->type->GetTy());
    return pointerExpr;
}


Ptr<Ty> InteropLibBridge::GetJlongTy() const
{
    return typeManager.GetPrimitiveTy(TypeKind::TYPE_INT64);
}

// type

OwnedPtr<Type> InteropLibBridge::CreateJobjectType() const
{
    Ptr<TypeAliasDecl> jobjectDecl = GetInteropLibDecl<ASTKind::TYPE_ALIAS_DECL>(INTEROPLIB_JNI_JOBJECT_ID);
    if (!jobjectDecl) {
        return nullptr;
    }
    return ASTCloner::Clone(Ptr(StaticAs<ASTKind::TYPE_ALIAS_DECL>(jobjectDecl)->type.get()));
}

OwnedPtr<Type> InteropLibBridge::CreateJlongType() const
{
    Ptr<TypeAliasDecl> jlongDecl = GetInteropLibDecl<ASTKind::TYPE_ALIAS_DECL>(INTEROPLIB_JNI_JLONG_ID);
    if (!jlongDecl) {
        return nullptr;
    }
    return ASTCloner::Clone(Ptr(StaticAs<ASTKind::TYPE_ALIAS_DECL>(jlongDecl)->type.get()));
}

// applications

OwnedPtr<CallExpr> InteropLibBridge::CreateGetJniEnvCall(Ptr<File> curFile)
{
    return CreateCall(GetGetJniEnvDecl(), curFile);
}

OwnedPtr<CallExpr> InteropLibBridge::CreateJavaEntityCall(Ptr<File> file)
{
    auto javaEntityDecl = GetJavaEntityDecl();
    if (!javaEntityDecl) {
        return nullptr;
    }

    Ptr<FuncDecl> suitableCtor;

    for (auto& decl : javaEntityDecl->body->decls) {
        if (auto ctor = As<ASTKind::FUNC_DECL>(decl.get())) {
            CJC_ASSERT_WITH_MSG(!ctor->funcBody->paramLists.empty(), "at least one paramLists expected");
            if (ctor->funcBody->paramLists[0]->params.empty()) {
                suitableCtor = ctor;
                break;
            }
        }
    }

    if (!suitableCtor) {
        diag.DiagnoseRefactor(
            DiagKindRefactor::sema_member_not_imported, DEFAULT_POSITION, INTEROPLIB_CFFI_JAVA_ENTITY);
    }

    auto call = CreateCall(suitableCtor, file);
    if (call) {
        call->callKind = CallKind::CALL_STRUCT_CREATION;
    }
    return call;
}

OwnedPtr<CallExpr> InteropLibBridge::CreateJavaEntityJobjectCall(OwnedPtr<Expr> arg)
{
    auto curFile = arg->curFile;
    return CreateCall(GetCreateJavaEntityJobjectDecl(), curFile, std::move(arg));
}

OwnedPtr<Expr> InteropLibBridge::CreateJavaEntityNullCall(Ptr<File> curFile)
{
    return CreateCall(GetCreateJavaEntityNullDecl(), curFile);
}

OwnedPtr<Expr> InteropLibBridge::CreateJavaEntityFromOptionMirror(OwnedPtr<Expr> option, ClassLikeDecl& mirror)
{
    auto curFile = option->curFile;
    CJC_NULLPTR_CHECK(curFile);
    CJC_ASSERT_WITH_MSG(!option->GetTy()->typeArgs.empty(), "Option type must be generic");
    auto mirrorTy = option->GetTy()->typeArgs[0];
    // `case Some(argv) => argv.javaref`
    auto vp = CreateVarPattern(V_COMPILER, mirrorTy);
    vp->curFile = curFile;
    vp->varDecl->curFile = curFile;
    auto javarefAccess = CreateJavaRefCall(WithinFile(CreateRefExpr(*vp->varDecl), curFile), mirror);

    auto somePattern = MakeOwnedNode<EnumPattern>();
    somePattern->SetTy(utils.GetOptionTy(mirrorTy));
    somePattern->constructor = utils.CreateOptionSomeRef(mirrorTy);
    somePattern->patterns.emplace_back(std::move(vp));
    somePattern->curFile = curFile;
    auto caseSome = CreateMatchCase(std::move(somePattern), std::move(javarefAccess));

    // `case None => Java_CFFI_JavaEntityJobjectNull()`
    auto nonePattern = MakeOwnedNode<EnumPattern>();
    nonePattern->constructor = utils.CreateOptionNoneRef(mirrorTy);
    nonePattern->SetTy(nonePattern->constructor->GetTy());
    nonePattern->curFile = curFile;
    auto caseNone = CreateMatchCase(std::move(nonePattern), CreateJavaEntityNullCall(curFile));

    // `match`
    std::vector<OwnedPtr<MatchCase>> matchCases;
    matchCases.emplace_back(std::move(caseSome));
    matchCases.emplace_back(std::move(caseNone));
    return WithinFile(CreateMatchExpr(std::move(option), std::move(matchCases), GetJavaEntityTy()), curFile);
}

OwnedPtr<Expr> InteropLibBridge::CreateJavaEntityCall(OwnedPtr<Expr> arg)
{
    auto javaEntityDecl = GetJavaEntityDecl();
    if (!javaEntityDecl) {
        return nullptr;
    }

    if (auto classLTy = DynamicCast<ClassLikeTy*>(arg->GetTy())) {
        if (auto decl = classLTy->commonDecl;
            decl && decl->TestAnyAttr(Attribute::JAVA_MIRROR, Attribute::JAVA_MIRROR_SUBTYPE)) {
            return CreateJavaRefCall(std::move(arg));
        }
    } else if (arg->GetTy()->IsCoreOptionType()) {
        CJC_ASSERT_WITH_MSG(!arg->GetTy()->typeArgs.empty(), "Option type must be generic");
        if (auto classALTy = DynamicCast<ClassLikeTy*>(arg->GetTy()->typeArgs[0])) {
            if (auto decl = classALTy->commonDecl;
                decl && decl->TestAnyAttr(Attribute::JAVA_MIRROR, Attribute::JAVA_MIRROR_SUBTYPE)) {
                return CreateJavaEntityFromOptionMirror(std::move(arg), *decl);
            }
        }
    }

    Ptr<FuncDecl> suitableCtor;

    if (arg->GetTy()->IsTuple()) {
        auto curFile = arg->curFile;
        auto clazzName = curFile->GetFullPackageName() + "." + GetCjMappingTupleName(*arg->GetTy());
        std::replace(clazzName.begin(), clazzName.end(), '.', '/');
        auto entity = CreateCFFINewJavaCFFINewJavaProxyObjectForCJMappingCall(
            WithinFile(CreateGetJniEnvCall(curFile), curFile), std::move(arg), clazzName, true);
        return CreateJavaEntityJobjectCall(std::move(entity));
    }

    for (auto& decl : javaEntityDecl->body->decls) {
        if (auto ctor = As<ASTKind::FUNC_DECL>(decl.get()); ctor && ctor->TestAttr(Attribute::CONSTRUCTOR)) {
            CJC_ASSERT_WITH_MSG(!ctor->funcBody->paramLists.empty(), "paramLists cannot be empty");
            if (ctor->funcBody->paramLists[0]->params.size() != 1) {
                continue;
            }
            if (typeManager.IsTyEqual(arg->GetTy(), ctor->funcBody->paramLists[0]->params[0]->GetTy())) {
                suitableCtor = ctor;
            }
        }
    }

    if (!suitableCtor) {
        diag.DiagnoseRefactor(
            DiagKindRefactor::sema_java_interop_not_supported, arg->begin, "Type " + arg->GetTy()->name);
        return nullptr;
    }

    auto curFile = arg->curFile;
    auto call = CreateCall(suitableCtor, curFile, std::move(arg));
    if (call) {
        call->callKind = CallKind::CALL_STRUCT_CREATION;
    }
    return call;
}

OwnedPtr<Expr> InteropLibBridge::JObjectToString(OwnedPtr<Expr> jobjectExpr, Ptr<File> curFile)
{
    auto env = CreateGetJniEnvCall(curFile);
    auto entity = CreateJavaEntityJobjectCall(WithinFile(std::move(jobjectExpr), curFile));
    return CreateJavaStringToCangjieCall(std::move(env), std::move(entity));
}

OwnedPtr<Expr> InteropLibBridge::OptionStringToJObject(OwnedPtr<Expr> optionExpr,
                                                       FuncParam& jniEnv,
                                                       const Decl& outerDecl)
{
    auto curFile = optionExpr->curFile;
    return utils.CreateOptionMatch(std::move(optionExpr),
        [&](VarDecl& v) -> OwnedPtr<Expr> {
            auto inner = WithinFile(CreateRefExpr(v), curFile);
            auto env = WithinFile(CreateRefExpr(jniEnv), curFile);
            auto entity = CreateCangjieStringToJavaCall(std::move(env), std::move(inner));
            return UnwrapJavaEntity(std::move(entity), &GetJniJobjectTy(), outerDecl, true);
        },
        [&]() -> OwnedPtr<Expr> { return CreateJobjectNull(); },
        &GetJniJobjectTy()
    );
}

OwnedPtr<Expr> InteropLibBridge::StringToJObject(OwnedPtr<Expr> cjStringExpr, Ptr<File> curFile,
                                                 FuncParam& jniEnvParam, const Decl& outerDecl)
{
    auto env = WithinFile(CreateRefExpr(jniEnvParam), curFile);
    auto entity = CreateCangjieStringToJavaCall(std::move(env), WithinFile(std::move(cjStringExpr), curFile));
    return UnwrapJavaEntity(WithinFile(std::move(entity), curFile), &GetJniJobjectTy(), outerDecl, true);
}

/**
 * Lower CJ expression into Java boundary representation (JavaEntity / jobject).
 */
 OwnedPtr<Expr> InteropLibBridge::WrapJavaEntity(OwnedPtr<Expr> cjExpr)
{
    CJC_NULLPTR_CHECK(cjExpr->curFile);
    if (cjExpr->TyKind() == TypeKind::TYPE_UNIT) {
        return CreateJavaEntityCall(cjExpr->curFile);
    }

    if (IsOptionOfString(cjExpr->GetTy())) {
        auto curFile = cjExpr->curFile;
        auto match = utils.CreateOptionMatch(std::move(cjExpr),
            [&](VarDecl& v) -> OwnedPtr<Expr> {
                auto ref = WithinFile(CreateRefExpr(v), curFile);
                return WrapJavaEntity(std::move(ref));
            },
            [&]() -> OwnedPtr<Expr> { return CreateJavaEntityNullCall(curFile); },
            GetJavaEntityTy()
        );
        return WithinFile(std::move(match), curFile);
    }

    if (cjExpr->GetTy()->IsString()) {
        // Special handling for String: convert Cangjie String to jstring
        // before wrapping it into JavaEntity for JNI interop.
        auto curFile = cjExpr->curFile;
        auto env = CreateGetJniEnvCall(curFile);
        return CreateCangjieStringToJavaCall(std::move(env), std::move(cjExpr));
    }

    return CreateJavaEntityCall(std::move(cjExpr));
}

OwnedPtr<CallExpr> InteropLibBridge::CreateNewJavaObjectCall(Ptr<Expr> env, const std::string& classTypeSignature,
    const std::string& constructorSignature, std::vector<OwnedPtr<Expr>> args)
{
    auto curFile = env->curFile;
    auto funcDecl = GetNewJavaObjectDecl();
    auto jclassInitFuncDecl = GetJClassDecl();
    auto parseSignatureFuncDecl = GetParseMethodSignatureDecl();
    auto callNestFuncDecl = GetCallNestDecl();
    auto methodIDFuncDecl = GetMethodIdConstr();
    if (!funcDecl || !jclassInitFuncDecl || !parseSignatureFuncDecl || !callNestFuncDecl || !methodIDFuncDecl) {
        return nullptr;
    }

    CJC_ASSERT_WITH_MSG(!jclassInitFuncDecl->funcBody->paramLists.empty(), "paramLists cannot be empty");
    CJC_ASSERT_WITH_MSG(jclassInitFuncDecl->funcBody->paramLists[0]->params.size() > 1,
        "at least 2 parameters expected for classInit function");
    CJC_ASSERT(jclassInitFuncDecl->funcBody->paramLists[0] && jclassInitFuncDecl->funcBody->paramLists[0]->params[1]);
    CJC_ASSERT_WITH_MSG(!callNestFuncDecl->funcBody->paramLists.empty(), "paramLists cannot be empty");
    CJC_ASSERT_WITH_MSG(!callNestFuncDecl->funcBody->paramLists[0]->params.empty(),
        "at least 1 parameter expected for callNestInit function");
    CJC_ASSERT(callNestFuncDecl->funcBody->paramLists[0] && callNestFuncDecl->funcBody->paramLists[0]->params[0]);
    auto strTy = jclassInitFuncDecl->funcBody->paramLists[0]->params[1]->GetTy();
    auto intTy = callNestFuncDecl->funcBody->paramLists[0]->params[0]->GetTy();

    auto lclassName = CreateLitConstExpr(LitConstKind::STRING, classTypeSignature, strTy);
    lclassName->curFile = curFile;
    auto lConstructorSignature = CreateLitConstExpr(LitConstKind::STRING, constructorSignature, strTy);
    lConstructorSignature->curFile = curFile;
    auto lmethodName = CreateLitConstExpr(LitConstKind::STRING, JAVA_CONSTRUCTOR, strTy);
    lmethodName->curFile = curFile;
    auto jclass = CreateCall(jclassInitFuncDecl, curFile, ASTCloner::Clone(env), std::move(lclassName));
    auto methodSignature = CreateCall(parseSignatureFuncDecl, curFile, std::move(lConstructorSignature));
    auto methodID = CreateCall(methodIDFuncDecl, curFile, ASTCloner::Clone(env), ASTCloner::Clone(jclass.get()),
        std::move(lmethodName), std::move(methodSignature));

    auto arrayStruct = importManager.GetCoreDecl<StructDecl>(STD_LIB_ARRAY);
    if (!arrayStruct) {
        return nullptr;
    }
    std::vector<Ptr<Ty>> arrParamsTy;
    arrParamsTy.push_back(GetJavaEntityTy());
    auto arrayTy = typeManager.GetStructTy(*arrayStruct, arrParamsTy);
    auto argsSize = CreateLitConstExpr(LitConstKind::INTEGER, std::to_string(args.size()), intTy);
    auto argsAsArray = WithinFile(CreateArrayLit(std::move(args), arrayTy), curFile);
    auto callNest = CreateCall(callNestFuncDecl, curFile, std::move(argsSize));

    return CreateCall(funcDecl, curFile,
        ASTCloner::Clone(env), std::move(jclass), std::move(methodID), std::move(argsAsArray), std::move(callNest));
}

OwnedPtr<Expr> InteropLibBridge::SelectJSigByTypeKind([[maybe_unused]] TypeKind kind, Ptr<Ty> ty)
{
    static auto strTy = utils.GetStringDecl().GetTy();
    return CreateLitConstExpr(LitConstKind::STRING, utils.GetJavaTypeSignature(*ty), strTy);
}

OwnedPtr<Expr> InteropLibBridge::SelectJPrimitiveNameByTypeKind(TypeKind kind, [[maybe_unused]] Ptr<Ty> ty)
{
    static auto javaEntityKindDecl = GetJavaEntityKindDecl();
    std::string typeName;
    switch (kind) {
        case TypeKind::TYPE_INT8:
            typeName = INTEROPLIB_JAVA_ENTITY_KIND_JBYTE;
            break;
        case TypeKind::TYPE_INT16:
            typeName = INTEROPLIB_JAVA_ENTITY_KIND_JSHORT;
            break;
        case TypeKind::TYPE_UINT16:
            typeName = INTEROPLIB_JAVA_ENTITY_KIND_JCHAR;
            break;
        case TypeKind::TYPE_INT32:
            typeName = INTEROPLIB_JAVA_ENTITY_KIND_JINT;
            break;
        case TypeKind::TYPE_INT64:
            typeName = INTEROPLIB_JAVA_ENTITY_KIND_JLONG;
            break;
        case TypeKind::TYPE_FLOAT32:
            typeName = INTEROPLIB_JAVA_ENTITY_KIND_JFLOAT;
            break;
        case TypeKind::TYPE_FLOAT64:
            typeName = INTEROPLIB_JAVA_ENTITY_KIND_JDOUBLE;
            break;
        case TypeKind::TYPE_BOOLEAN:
            typeName = INTEROPLIB_JAVA_ENTITY_KIND_JBOOLEAN;
            break;
        case TypeKind::TYPE_ENUM:
            typeName = INTEROPLIB_JAVA_ENTITY_KIND_JOBJECT;
            break;
        default:
            CJC_ABORT();
            break;
    }
    return CreateRefExpr(*LookupEnumMember(javaEntityKindDecl, typeName));
}

OwnedPtr<Expr> InteropLibBridge::SelectEntityWrapperByTypeKind(
    [[maybe_unused]] TypeKind kind, [[maybe_unused]] Ptr<Ty> ty, Ptr<FuncParam> param, Ptr<File> file)
{
    auto paramRef = CreateRefExpr(*param);
    paramRef->curFile = file;
    return WrapJavaEntity(CreateMatchWithTypeCast(std::move(paramRef), ty));
}

OwnedPtr<Expr> InteropLibBridge::SelectEntityUnwrapperByTypeKind(
    [[maybe_unused]] TypeKind kind, [[maybe_unused]] Ptr<Ty> ty, Ptr<Expr> entity, const ClassLikeDecl& mirror)
{
    auto cEntity = ASTCloner::Clone(entity);
    cEntity->curFile = entity->curFile;
    return CreateMatchWithTypeCast(UnwrapJavaEntity(std::move(cEntity), ty, mirror), ty);
}

std::map<std::string, OwnedPtr<Expr>> InteropLibBridge::GenerateTypeMappingWithSelector(
    std::function<OwnedPtr<Expr>(TypeKind, Ptr<Ty>)> selector)
{
    std::map<std::string, OwnedPtr<Expr>> typeMapping;
    for (auto& type : supportedArrayPrimitiveElementType) {
        auto ty = TypeManager::GetPrimitiveTy(type);
        typeMapping.insert(std::make_pair(Ty::ToString(ty), selector(type, ty)));
    }
    return typeMapping;
}

OwnedPtr<CallExpr> InteropLibBridge::CreateCFFIArrayLengthGetCall(OwnedPtr<Expr> javarefExpr, Ptr<File> curFile)
{
    return CreateCall(GetJavaArrayGetLengthDecl(), curFile, CreateGetJniEnvCall(curFile), std::move(javarefExpr));
}

OwnedPtr<CallExpr> InteropLibBridge::CreateCFFINewJavaArrayCall(OwnedPtr<Expr> jniEnv, FuncParamList& params)
{
    constexpr int expectedParamsSize = 2;
    CJC_ASSERT_WITH_MSG(params.params.size() == expectedParamsSize, "expected 2 params: 'length', '$jniType'.");
    CJC_ASSERT_WITH_MSG(jniEnv->curFile, "'curFile' expected to be not null in 'jniEnv' param");

    static auto funcDecl = GetNewJavaArrayDecl();
    auto curFile = jniEnv->curFile;
    auto getNthParam = [&params, &curFile](size_t ind) {
        return WithinFile(CreateRefExpr(*params.params[ind]), curFile);
    };
    auto sizeParam = getNthParam(0);
    auto jniTypeParam = getNthParam(1);

    return CreateCall(funcDecl, curFile, std::move(jniEnv), std::move(jniTypeParam), std::move(sizeParam));
}

OwnedPtr<CallExpr> InteropLibBridge::CreateCFFINewJavaCFFINewJavaProxyObjectForCJMappingCall(
    OwnedPtr<Expr> jniEnv, OwnedPtr<Expr> entity, std::string name, bool withMarkerParam)
{
    auto curFile = entity->curFile;
    static auto funcDecl = GetNewJavaProxyObjectForCJMappingDecl();
    if (!funcDecl) {
        return nullptr;
    }
    static auto retTy = utils.GetStringDecl().GetTy();
    auto expr = CreateLitConstExpr(LitConstKind::STRING, name, retTy);
    std::string hasArg = withMarkerParam ? "true" : "false";
    static const auto BOOL_TY = TypeManager::GetPrimitiveTy(TypeKind::TYPE_BOOLEAN);
    auto literal = CreateLitConstExpr(LitConstKind::BOOL, hasArg, BOOL_TY);
    return CreateCall(funcDecl, curFile, std::move(jniEnv), std::move(entity), std::move(expr), std::move(literal));
}

Ptr<FuncDecl> InteropLibBridge::FindArrayJavaEntityGetDecl(ClassDecl& jArrayDecl) const
{
    for (auto& member : jArrayDecl.body->decls) {
        if (member->identifier == JAVA_ARRAY_GET_FOR_REF_TYPES) {
            return As<ASTKind::FUNC_DECL>(member);
        }
    }
    return nullptr;
}

Ptr<FuncDecl> InteropLibBridge::FindArrayJavaEntitySetDecl(ClassDecl& jArrayDecl) const
{
    for (auto& member : jArrayDecl.body->decls) {
        if (member->identifier == JAVA_ARRAY_SET_FOR_REF_TYPES) {
            return As<ASTKind::FUNC_DECL>(member);
        }
    }
    return nullptr;
}

OwnedPtr<CallExpr> InteropLibBridge::CreateCFFINewJavaObjectCall(
    OwnedPtr<Expr> jniEnv, std::string classTypeSignature, FuncParamList& params, bool isMirror, File& curFile)
{
    static auto markerClassDecl = CreateConstructorMarkerClassDecl();
    std::vector<OwnedPtr<Expr>> cffiCtorFuncArgs;

    for (auto& param : params.params) {
        cffiCtorFuncArgs.push_back(WrapJavaEntity(WithinFile(CreateRefExpr(*param), Ptr(&curFile))));
    }

    std::vector<Ptr<Ty>> paramTys = Native::FFI::GetParamTys(params);

    if (!isMirror) {
        // in java, the constructor with additional fake null argument
        // of special marker types is created.
        cffiCtorFuncArgs.push_back(CreateJavaEntityNullCall(&curFile));
        paramTys.push_back(typeManager.GetClassTy(*markerClassDecl, {}));
    }

    return CreateNewJavaObjectCall(std::move(jniEnv), classTypeSignature,
        utils.GetJavaTypeSignature(*TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT), paramTys),
        std::move(cffiCtorFuncArgs));
}

OwnedPtr<CallExpr> InteropLibBridge::CreateNewGlobalRefCall(OwnedPtr<Expr> env, OwnedPtr<Expr> obj, bool isWeak)
{
    auto curFile = obj->curFile;
    auto isWeakBoolValue = CreateBoolLit(isWeak);
    isWeakBoolValue->curFile = curFile;
    isWeakBoolValue->SetTy(TypeManager::GetPrimitiveTy(TypeKind::TYPE_BOOLEAN));

    return CreateCall(GetNewGlobalRefDecl(), curFile, std::move(env), std::move(obj), std::move(isWeakBoolValue));
}

OwnedPtr<CallExpr> InteropLibBridge::CreateDeleteGlobalRefCall(OwnedPtr<Expr> env, OwnedPtr<Expr> obj)
{
    auto curFile = obj->curFile;
    return CreateCall(GetDeleteGlobalRefDecl(), curFile, std::move(env), std::move(obj));
}

OwnedPtr<FuncArg> InteropLibBridge::CreateInoutFuncArg(OwnedPtr<Expr> expr)
{
    auto arg = CreateFuncArg(std::move(expr));

    if (auto* varrayTy = DynamicCast<VArrayTy>(arg->GetTy())) {
        arg->withInout = true;
        arg->SetTy(typeManager.GetPointerTy(varrayTy->typeArgs[0]));
    }

    return arg;
}    

OwnedPtr<CallExpr> InteropLibBridge::CreateJNIMethodCall(OwnedPtr<Expr> env, OwnedPtr<Expr> obj, const MemberJNISignature& signature,
    OwnedPtr<Expr> argsExpr, Ptr<Ty> retTy, File& curFile, bool virt)
{
    auto pcurFile = Ptr(&curFile);
    auto callMethodVar = GetJNINativeInterfaceField(SelectJNIInstanceMethodName(retTy->kind, virt, argsExpr != nullptr));
    if (!callMethodVar) {
        return nullptr;
    }
    auto jclassInitFuncDecl = GetJClassDecl();
    auto getMethodIdFuncDecl = GetGetMethodIdDecl();

    if (!jclassInitFuncDecl || !getMethodIdFuncDecl) {
        return nullptr;
    }

    CJC_ASSERT(!jclassInitFuncDecl->funcBody->paramLists.empty());
    CJC_ASSERT(jclassInitFuncDecl->funcBody->paramLists[0]->params.size() > 1);

    auto strTy =  utils.GetStringDecl().GetTy();
    auto envForClass  = ASTCloner::Clone(env.get());
    auto envForMethod = ASTCloner::Clone(env.get());
    auto envForJNI    = ASTCloner::Clone(env.get());
    auto className = CreateLitConstExpr(LitConstKind::STRING, signature.classTypeSignature, strTy);
    auto methodName = CreateLitConstExpr(LitConstKind::STRING, signature.name, strTy);
    auto signatureStr = CreateLitConstExpr(LitConstKind::STRING, signature.signature, strTy);
    auto boolTy = typeManager.GetPrimitiveTy(TypeKind::TYPE_BOOLEAN);
    auto falseExpr = CreateLitConstExpr(LitConstKind::BOOL, "false", boolTy);

    auto jclass = CreateCall(jclassInitFuncDecl, pcurFile, std::move(envForClass), std::move(className));
    auto methodID = CreateCall(getMethodIdFuncDecl, pcurFile, std::move(envForMethod), std::move(jclass),
        std::move(methodName), std::move(signatureStr), std::move(falseExpr));

    auto envCJ = CreateJNIEnvReadCall(ASTCloner::Clone(env.get()));
    if (!envCJ) {
        return nullptr;
    }
    envCJ->curFile = pcurFile;

    auto methodAccess = WithinFile(CreateMemberAccess(std::move(envCJ), *callMethodVar), pcurFile);
    methodAccess->EnableAttr(Attribute::UNSAFE);

    auto asJObjectDecl = FindAsJObjectDecl();
    auto asJObjectMember = WithinFile(CreateMemberAccess(std::move(obj), *asJObjectDecl), pcurFile);
    auto asJObjectFuncTy = StaticCast<FuncTy*>(asJObjectDecl->GetTy());
    auto asJObjectCall = CreateCallExpr(std::move(asJObjectMember), {}, asJObjectDecl, asJObjectFuncTy->retTy, CallKind::CALL_DECLARED_FUNCTION);
    asJObjectCall->curFile = pcurFile;

    OwnedPtr<Expr> envForClass2 = nullptr;
    if (!virt) {
        envForClass2 = ASTCloner::Clone(env.get());
    }

    std::vector<OwnedPtr<FuncArg>> callArgs;
    callArgs.emplace_back(CreateFuncArg(std::move(env)));
    callArgs.emplace_back(CreateFuncArg(std::move(asJObjectCall)));

    if (!virt) {
        auto className2 = CreateLitConstExpr(LitConstKind::STRING, signature.classTypeSignature, strTy);
        auto jclass2 = CreateCall(jclassInitFuncDecl, pcurFile, std::move(envForClass2), std::move(className2));
        callArgs.emplace_back(CreateFuncArg(std::move(jclass2)));
    }

    callArgs.emplace_back(CreateFuncArg(std::move(methodID)));

    if (argsExpr) {
        callArgs.emplace_back(CreateInoutFuncArg(std::move(argsExpr)));
    }

    auto funcTy = StaticCast<FuncTy*>(callMethodVar->GetTy());
    auto callExpr = CreateCallExpr(std::move(methodAccess), std::move(callArgs), nullptr, funcTy->retTy, CallKind::CALL_FUNCTION_PTR);
    callExpr->EnableAttr(Attribute::UNSAFE);

    if (retTy->IsPrimitive()) {
        return callExpr;
    }

    // Object JNI calls return jobject. Wrap it into Java_CFFI_JavaEntity
    // to preserve the old API contract.
    return CreateJavaEntityJobjectCall(std::move(callExpr));
}

OwnedPtr<CallExpr> InteropLibBridge::CreateJNIEnvReadCall(OwnedPtr<Expr> env)
{
    auto readPointerDecl = importManager.GetCoreDecl<FuncDecl>("readPointer");
    if (!readPointerDecl) {
        return nullptr;
    }
    // env : CPointer<CPointer<JNINativeInterface_>>
    auto envTy = env->GetTy();
    CJC_ASSERT(envTy && envTy->typeArgs.size() == 1);

    // CPointer<JNINativeInterface_>
    auto ptrTy = envTy->typeArgs[0];
    CJC_ASSERT(ptrTy && ptrTy->typeArgs.size() == 1);

    // JNINativeInterface_
    auto interfaceTy = ptrTy->typeArgs[0];

    // readPointer<CPointer<JNINativeInterface_>>(env,0)
    auto ref1 = CreateRefExpr(*readPointerDecl);
    ref1->instTys.emplace_back(ptrTy);
    ref1->typeArguments.emplace_back(CreateType(ptrTy));
    ref1->SetTy(typeManager.GetInstantiatedTy(readPointerDecl->GetTy(),
        GenerateTypeMapping(*readPointerDecl, ref1->instTys)));
    auto int64Ty = typeManager.GetPrimitiveTy(TypeKind::TYPE_INT64);
    auto zero = CreateLitConstExpr(LitConstKind::INTEGER, "0", int64Ty);
    std::vector<OwnedPtr<FuncArg>> args1;
    args1.emplace_back(CreateFuncArg(std::move(env)));
    args1.emplace_back(CreateFuncArg(ASTCloner::Clone(zero.get())));
    auto read1 = CreateCallExpr(std::move(ref1), std::move(args1), readPointerDecl,
        ptrTy, CallKind::CALL_INTRINSIC_FUNCTION);
    read1->EnableAttr(Attribute::UNSAFE);

    // readPointer<JNINativeInterface_>(read1,0)
    auto ref2 = CreateRefExpr(*readPointerDecl);
    ref2->instTys.emplace_back(interfaceTy);
    ref2->typeArguments.emplace_back(CreateType(interfaceTy));
    ref2->SetTy(typeManager.GetInstantiatedTy(readPointerDecl->GetTy(),
        GenerateTypeMapping(*readPointerDecl, ref2->instTys)));
    std::vector<OwnedPtr<FuncArg>> args2;
    args2.emplace_back(CreateFuncArg(std::move(read1)));
    args2.emplace_back(CreateFuncArg(std::move(zero)));
    auto read2 = CreateCallExpr(std::move(ref2), std::move(args2), readPointerDecl,
        interfaceTy, CallKind::CALL_INTRINSIC_FUNCTION);
    read2->EnableAttr(Attribute::UNSAFE);
    return read2;
}

OwnedPtr<CallExpr> InteropLibBridge::CreateJNIStaticMethodCall(OwnedPtr<Expr> env, OwnedPtr<Expr> clazz,
    const MemberJNISignature& signature, OwnedPtr<Expr> argsExpr, Ptr<Ty> retTy, File& curFile)
{
    auto pcurFile = Ptr(&curFile);
    auto getMethodIdFuncDecl = GetGetMethodIdDecl();
    if (!getMethodIdFuncDecl) {
        return nullptr;
    }

    auto callMethodVar = GetJNINativeInterfaceField(SelectJNIStaticMethodName(retTy->kind, argsExpr != nullptr));
    if (!callMethodVar) {
        return nullptr;
    }

    auto strTy =  utils.GetStringDecl().GetTy();
    auto envForMethod = ASTCloner::Clone(env.get());
    auto envForJNI = ASTCloner::Clone(env.get());
    auto clazzForMethod = ASTCloner::Clone(clazz.get());
    auto methodName = CreateLitConstExpr(LitConstKind::STRING, signature.name, strTy);
    auto signatureStr = CreateLitConstExpr(LitConstKind::STRING, signature.signature, strTy);
    auto boolTy = typeManager.GetPrimitiveTy(TypeKind::TYPE_BOOLEAN);
    auto trueExpr = CreateLitConstExpr(LitConstKind::BOOL, "true", boolTy);

    auto methodID = CreateCall(getMethodIdFuncDecl, pcurFile, std::move(envForMethod), std::move(clazzForMethod),
        std::move(methodName), std::move(signatureStr), std::move(trueExpr));

    auto envCJ = CreateJNIEnvReadCall(ASTCloner::Clone(env.get()));
    if (!envCJ) {
        return nullptr;
    }
    envCJ->curFile = pcurFile;

    auto methodAccess = WithinFile(CreateMemberAccess(std::move(envCJ), *callMethodVar), pcurFile);
    methodAccess->EnableAttr(Attribute::UNSAFE);

    std::vector<OwnedPtr<FuncArg>> callArgs;
    callArgs.emplace_back(CreateFuncArg(std::move(env)));
    callArgs.emplace_back(CreateFuncArg(std::move(clazz)));
    callArgs.emplace_back(CreateFuncArg(std::move(methodID)));
    if (argsExpr) {
        callArgs.emplace_back(CreateInoutFuncArg(std::move(argsExpr)));
    }
    auto funcTy = StaticCast<FuncTy*>(callMethodVar->GetTy());
    auto callExpr = CreateCallExpr(std::move(methodAccess), std::move(callArgs), nullptr, funcTy->retTy,
        CallKind::CALL_FUNCTION_PTR);
    callExpr->EnableAttr(Attribute::UNSAFE);
    if (retTy->IsPrimitive()) {
        return callExpr;
    }
    return CreateJavaEntityJobjectCall(std::move(callExpr));
}

OwnedPtr<MatchExpr> InteropLibBridge::CreateMatchWithTypeCast(OwnedPtr<Expr> exprToCast, Ptr<Ty> castTy)
{
    static auto exceptionDecl = importManager.GetCoreDecl<ClassDecl>(CLASS_EXCEPTION);
    if (!exceptionDecl) {
        return nullptr;
    }
    auto castType = MakeOwned<Type>();
    castType->SetTy(castTy);
    auto varPattern = CreateVarPattern(V_COMPILER, castType->GetTy());
    auto curFile = exprToCast->curFile;
    CJC_NULLPTR_CHECK(curFile);
    varPattern->curFile = curFile;
    varPattern->varDecl->curFile = curFile;
    auto varPatternRef = WithinFile(CreateRefExpr(*(varPattern->varDecl)), curFile);

    std::vector<OwnedPtr<MatchCase>> matchCases;

    auto typePattern = CreateTypePattern(std::move(varPattern), std::move(castType), *exprToCast);
    typePattern->matchBeforeRuntime = false;

    std::vector<OwnedPtr<Expr>> exceptionCallArgs;
    exceptionCallArgs.emplace_back(CreateLitConstExpr(LitConstKind::STRING,
        "internal error: CreateMatchWithTypeCast(" + Ty::KindName(exprToCast->TyKind()) + " -> " +
            Ty::KindName(castTy->kind) + ")",
        utils.GetStringDecl().GetTy()));
    exceptionCallArgs.back()->curFile = curFile;

    matchCases.emplace_back(CreateMatchCase(std::move(typePattern), std::move(varPatternRef)));
    matchCases[0]->curFile = curFile;
    matchCases.emplace_back(CreateMatchCase(MakeOwned<WildcardPattern>(),
        CreateThrowException(*exceptionDecl, std::move(exceptionCallArgs), *exprToCast->curFile, typeManager)));
    matchCases[1]->curFile = curFile;

    return WithinFile(CreateMatchExpr(std::move(exprToCast), std::move(matchCases), castTy), curFile);
}

OwnedPtr<Expr> InteropLibBridge::CreateCFFICallArrayMethodCall(OwnedPtr<Expr> jniEnv, OwnedPtr<Expr> obj,
    FuncParamList& params, const Ptr<GenericParamDecl> genericParam, ArrayOperationKind kind)
{
    CJC_ASSERT(kind == ArrayOperationKind::GET || kind == ArrayOperationKind::SET);
    static auto javaEntityKindDecl = GetJavaEntityKindDecl();
    static auto jObject = utils.GetJObjectDecl();
    static auto javaRef = GetJavaRefField(*jObject);
    CJC_ASSERT(javaRef);
    auto curFile = genericParam->curFile;

    auto funcDecl = kind == ArrayOperationKind::GET ? GetJavaArrayGetDecl() : GetJavaArraySetDecl();
    auto indexArg = params.params[0].get();
    auto matchWithJPrimitive = CreateMatchByTypeArgument(genericParam,
        GenerateTypeMappingWithSelector(
            [this](TypeKind kind, Ptr<Ty> ty) { return SelectJPrimitiveNameByTypeKind(kind, ty); }),
        javaEntityKindDecl->GetTy(), WithinFile(CreateRefExpr(GetJavaEntityKindJObject()), curFile));

    if (kind == ArrayOperationKind::GET) {
        return CreateCall(funcDecl, curFile, std::move(jniEnv), std::move(matchWithJPrimitive), std::move(obj),
            WithinFile(CreateRefExpr(*indexArg), curFile));
    }

    auto valueArg = params.params[1].get();
    auto paramRef = WithinFile(CreateRefExpr(*valueArg), curFile);

    OwnedPtr<Expr> valueEntity;
    if (valueArg->GetTy()->name == INTEROPLIB_CFFI_JAVA_ENTITY) {
        // for generated function - value argument was replaced with java entity
        valueEntity = std::move(paramRef);
    } else {
        valueEntity = CreateMatchByTypeArgument(genericParam,
            GenerateTypeMappingWithSelector([this, &valueArg, &curFile](TypeKind kind, Ptr<Ty> ty) {
                return SelectEntityWrapperByTypeKind(kind, ty, valueArg, curFile);
            }),
            GetJavaEntityDecl()->GetTy(),
            CreateMemberAccess(CreateMatchWithTypeCast(std::move(paramRef), jObject->GetTy()), *javaRef));
    }

    return CreateCall(funcDecl, curFile, std::move(jniEnv), std::move(matchWithJPrimitive), std::move(obj),
        WithinFile(CreateRefExpr(*indexArg), curFile), std::move(valueEntity));
}

struct JNIMethodNames {
    const char* virtualNoArgs;
    const char* virtualArgs;
    const char* nonVirtualNoArgs;
    const char* nonVirtualArgs;
    const char* staticNoArgs;
    const char* staticArgs;
};

namespace {

constexpr JNIMethodNames BYTE_METHODS{
    "CallByteMethod",          "CallByteMethodA",
    "CallNonvirtualByteMethod","CallNonvirtualByteMethodA",
    "CallStaticByteMethod",    "CallStaticByteMethodA"
};

constexpr JNIMethodNames CHAR_METHODS{
    "CallCharMethod",          "CallCharMethodA",
    "CallNonvirtualCharMethod","CallNonvirtualCharMethodA",
    "CallStaticCharMethod",    "CallStaticCharMethodA"
};

constexpr JNIMethodNames SHORT_METHODS{
    "CallShortMethod",          "CallShortMethodA",
    "CallNonvirtualShortMethod","CallNonvirtualShortMethodA",
    "CallStaticShortMethod",    "CallStaticShortMethodA"
};

constexpr JNIMethodNames INT_METHODS{
    "CallIntMethod",          "CallIntMethodA",
    "CallNonvirtualIntMethod","CallNonvirtualIntMethodA",
    "CallStaticIntMethod",    "CallStaticIntMethodA"
};

constexpr JNIMethodNames LONG_METHODS{
    "CallLongMethod",          "CallLongMethodA",
    "CallNonvirtualLongMethod","CallNonvirtualLongMethodA",
    "CallStaticLongMethod",    "CallStaticLongMethodA"
};

constexpr JNIMethodNames FLOAT_METHODS{
    "CallFloatMethod",          "CallFloatMethodA",
    "CallNonvirtualFloatMethod","CallNonvirtualFloatMethodA",
    "CallStaticFloatMethod",    "CallStaticFloatMethodA"
};

constexpr JNIMethodNames DOUBLE_METHODS{
    "CallDoubleMethod",          "CallDoubleMethodA",
    "CallNonvirtualDoubleMethod","CallNonvirtualDoubleMethodA",
    "CallStaticDoubleMethod",    "CallStaticDoubleMethodA"
};

constexpr JNIMethodNames BOOLEAN_METHODS{
    "CallBooleanMethod",          "CallBooleanMethodA",
    "CallNonvirtualBooleanMethod","CallNonvirtualBooleanMethodA",
    "CallStaticBooleanMethod",    "CallStaticBooleanMethodA"
};

constexpr JNIMethodNames VOID_METHODS{
    "CallVoidMethod",          "CallVoidMethodA",
    "CallNonvirtualVoidMethod","CallNonvirtualVoidMethodA",
    "CallStaticVoidMethod",    "CallStaticVoidMethodA"
};

constexpr JNIMethodNames OBJECT_METHODS{
    "CallObjectMethod",          "CallObjectMethodA",
    "CallNonvirtualObjectMethod","CallNonvirtualObjectMethodA",
    "CallStaticObjectMethod",    "CallStaticObjectMethodA"
};

} // namespace

const JNIMethodNames& GetJNIMethodNames(TypeKind kind)
{
    switch (kind) {
        case TypeKind::TYPE_INT8:     return BYTE_METHODS;
        case TypeKind::TYPE_UINT16:   return CHAR_METHODS;
        case TypeKind::TYPE_INT16:    return SHORT_METHODS;
        case TypeKind::TYPE_INT32:    return INT_METHODS;
        case TypeKind::TYPE_INT64:    return LONG_METHODS;
        case TypeKind::TYPE_FLOAT32:  return FLOAT_METHODS;
        case TypeKind::TYPE_FLOAT64:  return DOUBLE_METHODS;
        case TypeKind::TYPE_BOOLEAN:  return BOOLEAN_METHODS;
        case TypeKind::TYPE_UNIT:     return VOID_METHODS;
        default:                      return OBJECT_METHODS;
    }
}

const char* InteropLibBridge::SelectJNIInstanceMethodName(TypeKind kind, bool virt, bool hasArgs)
{
    const auto& m = GetJNIMethodNames(kind);

    if (virt) {
        return hasArgs ? m.virtualArgs : m.virtualNoArgs;
    }

    return hasArgs ? m.nonVirtualArgs : m.nonVirtualNoArgs;
}

const char* InteropLibBridge::SelectJNIStaticMethodName(TypeKind kind, bool hasArgs)
{
    const auto& m = GetJNIMethodNames(kind);
    return hasArgs ? m.staticArgs : m.staticNoArgs;
}

OwnedPtr<Expr> InteropLibBridge::CreateReinterpretCastExpr(OwnedPtr<Expr> expr,
    Ptr<Ty> resultTy, Ptr<Ty> valueTy, Ptr<File> curFile)
{
    static auto reinterpretCastDecl = GetInteropLibDecl<ASTKind::FUNC_DECL>("reinterpretCast");
    if (!reinterpretCastDecl) {
        return nullptr;
    }

    auto ref = CreateRefExpr(*reinterpretCastDecl);
    ref->instTys.emplace_back(resultTy);
    ref->instTys.emplace_back(valueTy);
    ref->typeArguments.emplace_back(CreateType(resultTy));
    ref->typeArguments.emplace_back(CreateType(valueTy));
    ref->SetTy(typeManager.GetInstantiatedTy(reinterpretCastDecl->GetTy(),
        GenerateTypeMapping(*reinterpretCastDecl, ref->instTys)));

    std::vector<OwnedPtr<FuncArg>> args;
    args.emplace_back(CreateFuncArg(std::move(expr)));
    auto call = CreateCallExpr(std::move(ref), std::move(args),
        reinterpretCastDecl, resultTy, CallKind::CALL_DECLARED_FUNCTION);
    call->curFile = curFile;
    return call;
}

OwnedPtr<VarDecl> InteropLibBridge::CreateJNIClassVar(OwnedPtr<Expr> env, const std::string& className, File& curFile)
{
    auto pcurFile = Ptr(&curFile);
    auto jclassInitFuncDecl = GetJClassDecl();
    if (!jclassInitFuncDecl) {
        return nullptr;
    }

    auto strTy = jclassInitFuncDecl->funcBody->paramLists[0]->params[1]->GetTy();
    auto classNameExpr = CreateLitConstExpr(LitConstKind::STRING, className, strTy);
    classNameExpr->curFile = pcurFile;
    auto initCall = CreateCall(jclassInitFuncDecl, pcurFile, std::move(env), std::move(classNameExpr));
    return CreateTmpVarDecl(nullptr, std::move(initCall));
}

OwnedPtr<VarDecl> InteropLibBridge::CreateJNIArgsVar(std::vector<OwnedPtr<Expr>> args, File& curFile)
{
    if (args.empty()) {
        return nullptr;
    }
    auto jvalueTy = GetJValueTy();
    CJC_NULLPTR_CHECK(jvalueTy);
    auto varrayTy = typeManager.GetVArrayTy(*jvalueTy, static_cast<int64_t>(args.size()));
    auto argsArray = CreateArrayLit(std::move(args), varrayTy);
    auto argsVar = CreateVarDecl("argsPtr", std::move(argsArray), nullptr);
    argsVar->isVar = true;
    argsVar->SetTy(argsVar->initializer->GetTy());
    CopyBasicInfo(argsVar->initializer.get(), argsVar.get());
    argsVar->curFile = Ptr(&curFile);
    return argsVar;
}

OwnedPtr<Expr> InteropLibBridge::CreateJValueExpr(OwnedPtr<Expr> expr, Ptr<File> curFile)
{
    CJC_NULLPTR_CHECK(expr);
    auto ty = expr->GetTy();
    if (ty->IsPrimitive()) {
        switch (ty->kind) {
            case TypeKind::TYPE_FLOAT32:
                return CreateReinterpretCastExpr(std::move(expr),
                    typeManager.GetPrimitiveTy(TypeKind::TYPE_UINT32),
                    typeManager.GetPrimitiveTy(TypeKind::TYPE_FLOAT32),
                    curFile);
            case TypeKind::TYPE_FLOAT64:
                return CreateReinterpretCastExpr(std::move(expr),
                    typeManager.GetPrimitiveTy(TypeKind::TYPE_UINT64),
                    typeManager.GetPrimitiveTy(TypeKind::TYPE_FLOAT64),
                    curFile);
            default:
                return expr;
        }
    }

    auto entityExpr = WrapJavaEntity(std::move(expr));
    auto asJValueDecl = FindAsJValueDecl();
    CJC_NULLPTR_CHECK(asJValueDecl);
    auto member =
        WithinFile(CreateMemberAccess(std::move(entityExpr), *asJValueDecl), curFile);

    auto call = CreateCallExpr(std::move(member), {}, asJValueDecl, GetJValueTy(), CallKind::CALL_DECLARED_FUNCTION);
    call->curFile = curFile;
    return call;
}

OwnedPtr<Expr> InteropLibBridge::CreateJNIArgsRef(FuncParamList& paramList, File& curFile, OwnedPtr<VarDecl>& argsVar)
{
    CJC_ASSERT_WITH_MSG(!paramList.params.empty(), "JNI argument list cannot be empty");
    std::vector<OwnedPtr<Expr>> args;
    args.reserve(paramList.params.size());
    for (auto& param : paramList.params) {
        args.emplace_back(CreateJValueExpr(WithinFile(CreateRefExpr(*param), Ptr(&curFile)), Ptr(&curFile)));
    }

    argsVar = CreateJNIArgsVar(std::move(args), curFile);
    if (!argsVar) {
        return nullptr;
    }
    auto argsRef = CreateRefExpr(*argsVar);
    CopyBasicInfo(argsVar.get(), argsRef.get());
    return argsRef;
}

OwnedPtr<Expr> InteropLibBridge::ConvertJNIResult(OwnedPtr<Expr> result, Ptr<Ty> resultTy, Decl& scope)
{
    CJC_NULLPTR_CHECK(result);
    CJC_NULLPTR_CHECK(resultTy);
    if (!resultTy->IsPrimitive()) {
        return UnwrapJavaEntity(std::move(result), resultTy, scope);
    }
    if (resultTy->kind == TypeKind::TYPE_BOOLEAN) {
        auto zero = CreateLitConstExpr(LitConstKind::INTEGER, "0", result->GetTy());
        auto cmp = CreateBinaryExpr(std::move(result), std::move(zero), TokenKind::NOTEQ);
        cmp->SetTy(typeManager.GetPrimitiveTy(TypeKind::TYPE_BOOLEAN));
        return cmp;
    }
    return result;
}

OwnedPtr<CallExpr> InteropLibBridge::CreateRemoveFromRegistryCall(OwnedPtr<Expr> registryId)
{
    auto curFile = registryId->curFile;
    return CreateCall(GetRemoveFromRegistryDecl(), curFile, std::move(registryId));
}

OwnedPtr<CallExpr> InteropLibBridge::CreatePutToRegistryCall(OwnedPtr<Expr> obj)
{
    auto curFile = obj->curFile;
    return CreateCall(GetPutToRegistryDecl(), curFile, std::move(obj));
}

OwnedPtr<CallExpr> InteropLibBridge::CreatePutSetToRegistryCall(
    OwnedPtr<Expr> env, OwnedPtr<Expr> entity, OwnedPtr<Expr> obj)
{
    auto curFile = entity->curFile;
    return CreateCall(GetPutSetToRegistryDecl(), curFile, std::move(env), std::move(entity), std::move(obj));
}

OwnedPtr<CallExpr> InteropLibBridge::CreateGetRegistryIdCall(OwnedPtr<Expr> env, OwnedPtr<Expr> entity, bool nullable)
{
    auto curFile = entity->curFile;
    auto decl = nullable ? GetGetRegistryIdOrNoneDecl() : GetGetRegistryIdDecl();
    return CreateCall(decl, curFile, std::move(env), std::move(entity));
}

OwnedPtr<CallExpr> InteropLibBridge::CreateGetFromRegistryByEntityCall(
    OwnedPtr<Expr> env, OwnedPtr<Expr> obj, Ptr<Ty> ty, bool retAsOption)
{
    auto curFile = obj->curFile;
    auto funcDecl = retAsOption ? GetGetFromRegistryByEntityOptionDecl() : GetGetFromRegistryByEntityDecl();
    if (!funcDecl) {
        return nullptr;
    }

    std::vector<OwnedPtr<FuncArg>> callArgs;
    callArgs.push_back(CreateFuncArg(std::move(env)));
    callArgs.push_back(CreateFuncArg(std::move(obj)));

    auto fdRef = WithinFile(CreateRefExpr(*funcDecl), curFile);
    fdRef->instTys.push_back(ty);
    fdRef->SetTy(typeManager.GetInstantiatedTy(funcDecl->GetTy(), GenerateTypeMapping(*funcDecl, fdRef->instTys)));
    auto retTy = retAsOption ? utils.GetOptionTy(ty) : ty;
    return CreateCallExpr(std::move(fdRef), std::move(callArgs), funcDecl, retTy, CallKind::CALL_DECLARED_FUNCTION);
}

OwnedPtr<CallExpr> InteropLibBridge::CreateCangjieStringToJavaCall(OwnedPtr<Expr> env, OwnedPtr<Expr> string)
{
    auto curFile = string->curFile;
    auto funcDecl = GetCangjieStringToJava();
    if (!funcDecl) {
        return nullptr;
    }

    return CreateCall(funcDecl, curFile, std::move(env), std::move(string));
}


OwnedPtr<CallExpr> InteropLibBridge::CreateJavaStringToCangjieCall(OwnedPtr<Expr> env, OwnedPtr<Expr> jstring)
{
    auto curFile = jstring->curFile;
    auto funcDecl = GetJavaStringToCangjie();
    if (!funcDecl) {
        return nullptr;
    }

    return CreateCall(funcDecl, curFile, std::move(env), std::move(jstring));
}

OwnedPtr<CallExpr> InteropLibBridge::CreateGetFromRegistryCall(OwnedPtr<Expr> env, OwnedPtr<Expr> self, Ptr<Ty> ty)
{
    auto curFile = self->curFile;
    auto funcDecl = GetGetFromRegistryDecl();
    if (!funcDecl) {
        return nullptr;
    }

    std::vector<OwnedPtr<FuncArg>> callArgs;
    callArgs.push_back(CreateFuncArg(std::move(env)));
    callArgs.push_back(CreateFuncArg(std::move(self)));

    auto fdRef = WithinFile(CreateRefExpr(*funcDecl), curFile);
    fdRef->instTys.push_back(ty);
    fdRef->SetTy(typeManager.GetInstantiatedTy(funcDecl->GetTy(), GenerateTypeMapping(*funcDecl, fdRef->instTys)));
    return CreateCallExpr(std::move(fdRef), std::move(callArgs), funcDecl, ty, CallKind::CALL_DECLARED_FUNCTION);
}

OwnedPtr<CallExpr> InteropLibBridge::CreateGetFromRegistryOptionCall(OwnedPtr<Expr> self, Ptr<Ty> ty)
{
    auto curFile = self->curFile;
    CJC_ASSERT(ty->IsCoreOptionType());
    auto funcDecl = GetGetFromRegistryOptionDecl();
    if (!funcDecl) {
        return nullptr;
    }

    std::vector<OwnedPtr<FuncArg>> callArgs;
    callArgs.push_back(CreateFuncArg(std::move(self)));

    auto fdRef = WithinFile(CreateRefExpr(*funcDecl), curFile);
    CJC_ASSERT_WITH_MSG(!ty->typeArgs.empty(), "Option type expected to be generic");
    fdRef->instTys.push_back(ty->typeArgs[0]);
    fdRef->SetTy(typeManager.GetInstantiatedTy(funcDecl->GetTy(), GenerateTypeMapping(*funcDecl, fdRef->instTys)));
    return CreateCallExpr(std::move(fdRef), std::move(callArgs), funcDecl, ty, CallKind::CALL_DECLARED_FUNCTION);
}

OwnedPtr<CallExpr> InteropLibBridge::CreateGetFieldCall(Ptr<Expr> env, OwnedPtr<Expr> obj,
    std::string typeSignature, std::string fieldName, std::string fieldSignature)
{
    auto curFile = obj->curFile;
    auto funcDecl = GetGetFieldDecl();
    auto jclassInitFuncDecl = GetJClassDecl();
    auto parseSignatureFuncDecl = GetParseComponentSignatureDecl();
    auto fieldIDFuncDecl = GetFieldIdConstr();
    if (!funcDecl || !jclassInitFuncDecl || !parseSignatureFuncDecl || !fieldIDFuncDecl) {
        return nullptr;
    }

    CJC_ASSERT_WITH_MSG(!jclassInitFuncDecl->funcBody->paramLists.empty(), "paramLists cannot be empty");
    CJC_ASSERT_WITH_MSG(jclassInitFuncDecl->funcBody->paramLists[0]->params.size() > 1,
        "at least 2 parameters expected for classInit function");
    CJC_ASSERT(jclassInitFuncDecl->funcBody->paramLists[0] && jclassInitFuncDecl->funcBody->paramLists[0]->params[1]);
    auto strTy = jclassInitFuncDecl->funcBody->paramLists[0]->params[1]->GetTy();

    auto tsigExpr = CreateLitConstExpr(LitConstKind::STRING, typeSignature, strTy);
    auto fieldNameExpr = CreateLitConstExpr(LitConstKind::STRING, fieldName, strTy);
    auto fsigExpr = CreateLitConstExpr(LitConstKind::STRING, fieldSignature, strTy);

    auto jclass = CreateCall(jclassInitFuncDecl, curFile, ASTCloner::Clone(env), std::move(tsigExpr));
    auto filedSignature = CreateCall(parseSignatureFuncDecl, curFile, std::move(fsigExpr));
    auto fieldID = CreateCall(fieldIDFuncDecl, curFile, ASTCloner::Clone(env), std::move(jclass),
        std::move(fieldNameExpr), std::move(filedSignature));

    return CreateCall(funcDecl, curFile, ASTCloner::Clone(env), std::move(obj), std::move(fieldID));
}

OwnedPtr<CallExpr> InteropLibBridge::CreateGetStaticFieldCall(
    Ptr<Expr> env, std::string typeSignature, std::string fieldName, std::string fieldSignature)
{
    auto curFile = env->curFile;
    auto funcDecl = GetGetStaticFieldDecl();
    auto jclassInitFuncDecl = GetJClassDecl();
    auto parseSignatureFuncDecl = GetParseComponentSignatureDecl();
    auto fieldIDFuncDecl = GetFieldIdConstrStatic();
    if (!funcDecl || !jclassInitFuncDecl || !parseSignatureFuncDecl || !fieldIDFuncDecl) {
        return nullptr;
    }

    auto entityTy = GetJavaEntityTy();
    if (!entityTy) {
        return nullptr;
    }

    CJC_ASSERT_WITH_MSG(!jclassInitFuncDecl->funcBody->paramLists.empty(), "paramLists cannot be empty");
    CJC_ASSERT_WITH_MSG(jclassInitFuncDecl->funcBody->paramLists[0]->params.size() > 1,
        "at least 2 parameters expected for classInit function");
    CJC_ASSERT(jclassInitFuncDecl->funcBody->paramLists[0] && jclassInitFuncDecl->funcBody->paramLists[0]->params[1]);
    auto strTy = jclassInitFuncDecl->funcBody->paramLists[0]->params[1]->GetTy();

    auto tsigExpr = CreateLitConstExpr(LitConstKind::STRING, typeSignature, strTy);
    auto fieldNameExpr = CreateLitConstExpr(LitConstKind::STRING, fieldName, strTy);
    auto fsigExpr = CreateLitConstExpr(LitConstKind::STRING, fieldSignature, strTy);

    auto jclass = CreateCall(jclassInitFuncDecl, curFile, ASTCloner::Clone(env), std::move(tsigExpr));
    auto filedSignature = CreateCall(parseSignatureFuncDecl, curFile, std::move(fsigExpr));
    auto fieldID = CreateCall(fieldIDFuncDecl, curFile, ASTCloner::Clone(env), ASTCloner::Clone(jclass.get()),
        std::move(fieldNameExpr), std::move(filedSignature));

    return CreateCall(funcDecl, curFile, ASTCloner::Clone(env), std::move(jclass), std::move(fieldID));
}

OwnedPtr<CallExpr> InteropLibBridge::CreateSetFieldCall(
    Ptr<Expr> env, OwnedPtr<Expr> obj, const MemberJNISignature& signature, OwnedPtr<Expr> value)
{
    auto curFile = value->curFile;
    auto funcDecl = GetSetFieldDecl();
    auto jclassInitFuncDecl = GetJClassDecl();
    auto parseSignatureFuncDecl = GetParseComponentSignatureDecl();
    auto fieldIDFuncDecl = GetFieldIdConstr();
    if (!funcDecl || !jclassInitFuncDecl || !parseSignatureFuncDecl || !fieldIDFuncDecl) {
        return nullptr;
    }

    CJC_ASSERT_WITH_MSG(!jclassInitFuncDecl->funcBody->paramLists.empty(), "paramLists cannot be empty");
    CJC_ASSERT_WITH_MSG(jclassInitFuncDecl->funcBody->paramLists[0]->params.size() > 1,
        "at least 2 parameters expected for classInit function");
    CJC_ASSERT(jclassInitFuncDecl->funcBody->paramLists[0] && jclassInitFuncDecl->funcBody->paramLists[0]->params[1]);
    auto strTy = jclassInitFuncDecl->funcBody->paramLists[0]->params[1]->GetTy();

    auto tsigExpr = CreateLitConstExpr(LitConstKind::STRING, signature.classTypeSignature, strTy);
    tsigExpr->curFile = curFile;
    auto fieldNameExpr = CreateLitConstExpr(LitConstKind::STRING, signature.name, strTy);
    fieldNameExpr->curFile = curFile;
    auto fsigExpr = CreateLitConstExpr(LitConstKind::STRING, signature.signature, strTy);
    fsigExpr->curFile = curFile;

    auto jclass = CreateCall(jclassInitFuncDecl, curFile, ASTCloner::Clone(env), std::move(tsigExpr));
    auto filedSignature = CreateCall(parseSignatureFuncDecl, curFile, std::move(fsigExpr));
    auto fieldID = CreateCall(fieldIDFuncDecl, curFile, ASTCloner::Clone(env), std::move(jclass),
        std::move(fieldNameExpr), std::move(filedSignature));

    return CreateCall(funcDecl,
        curFile, ASTCloner::Clone(env), std::move(obj), std::move(fieldID), WrapJavaEntity(std::move(value)));
}

OwnedPtr<CallExpr> InteropLibBridge::CreateSetStaticFieldCall(Ptr<Expr> env, std::string typeSignature,
    std::string fieldName, std::string fieldSignature, OwnedPtr<Expr> value)
{
    auto curFile = value->curFile;
    auto funcDecl = GetSetStaticFieldDecl();
    auto jclassInitFuncDecl = GetJClassDecl();
    auto parseSignatureFuncDecl = GetParseComponentSignatureDecl();
    auto fieldIDFuncDecl = GetFieldIdConstrStatic();
    if (!funcDecl || !jclassInitFuncDecl || !parseSignatureFuncDecl || !fieldIDFuncDecl) {
        return nullptr;
    }

    CJC_ASSERT_WITH_MSG(!jclassInitFuncDecl->funcBody->paramLists.empty(), "paramLists cannot be empty");
    CJC_ASSERT_WITH_MSG(jclassInitFuncDecl->funcBody->paramLists[0]->params.size() > 1,
        "at least 2 parameters expected for classInit function");
    CJC_ASSERT(jclassInitFuncDecl->funcBody->paramLists[0] && jclassInitFuncDecl->funcBody->paramLists[0]->params[1]);
    auto strTy = jclassInitFuncDecl->funcBody->paramLists[0]->params[1]->GetTy();

    auto tsigExpr = CreateLitConstExpr(LitConstKind::STRING, typeSignature, strTy);
    tsigExpr->curFile = curFile;
    auto fieldNameExpr = CreateLitConstExpr(LitConstKind::STRING, fieldName, strTy);
    fieldNameExpr->curFile = curFile;
    auto fsigExpr = CreateLitConstExpr(LitConstKind::STRING, fieldSignature, strTy);
    fsigExpr->curFile = curFile;

    auto jclass = CreateCall(jclassInitFuncDecl, curFile, ASTCloner::Clone(env), std::move(tsigExpr));
    auto filedSignature = CreateCall(parseSignatureFuncDecl, curFile, std::move(fsigExpr));
    auto fieldID = CreateCall(fieldIDFuncDecl, curFile, ASTCloner::Clone(env), ASTCloner::Clone(jclass.get()),
        std::move(fieldNameExpr), std::move(filedSignature));
    return CreateCall(funcDecl,
        curFile, ASTCloner::Clone(env), std::move(jclass), std::move(fieldID), WrapJavaEntity(std::move(value)));
}

OwnedPtr<Expr> InteropLibBridge::WrapExceptionHandling(OwnedPtr<Expr> env, OwnedPtr<LambdaExpr> action)
{
    auto curFile = env->curFile;
    auto retTy = action->funcBody->GetTy();
    auto fd = GetWithExceptionHandlingDecl();
    auto fdRef = WithinFile(CreateRefExpr(*fd), curFile);
    fdRef->instTys.push_back(retTy);
    fdRef->SetTy(typeManager.GetInstantiatedTy(fd->GetTy(), GenerateTypeMapping(*fd, fdRef->instTys)));

    std::vector<OwnedPtr<FuncArg>> args;
    args.emplace_back(CreateFuncArg(std::move(env)));
    args.emplace_back(CreateFuncArg(std::move(action)));
    return CreateCallExpr(std::move(fdRef), std::move(args), nullptr, retTy, CallKind::CALL_DECLARED_FUNCTION);
}

OwnedPtr<CallExpr> InteropLibBridge::CreateJavaObjectControllerCall(
    OwnedPtr<Expr> javaEntity, OwnedPtr<Expr> className, ClassDecl& classDecl)
{
    auto funcDecl = GetJavaObjectControllerInitDecl();
    auto javaObjectCtroDecl = GetJavaObjectControllerDecl();

    auto curFile = classDecl.curFile;
    auto instantiationRefType = CreateRefType(classDecl);
    auto instantiationTy = typeManager.GetClassTy(classDecl, classDecl.GetTy()->typeArgs);

    std::vector<OwnedPtr<FuncArg>> callArgs;
    callArgs.push_back(CreateFuncArg(std::move(javaEntity)));
    callArgs.push_back(CreateFuncArg(std::move(className)));

    auto fdRefexpr = CreateRefExpr(*funcDecl);
    fdRefexpr->ref.identifier = javaObjectCtroDecl->identifier;
    fdRefexpr->typeArguments.emplace_back(std::move(instantiationRefType));
    auto fdRef = WithinFile(std::move(fdRefexpr), curFile);

    auto callTy = typeManager.GetClassTy(*javaObjectCtroDecl, {std::move(instantiationTy)});
    return CreateCallExpr(std::move(fdRef), std::move(callArgs), funcDecl, callTy, CallKind::CALL_OBJECT_CREATION);
}

bool InteropLibBridge::IsJavaEntityTy(Ty& ty)
{
    auto javaEntityTy = DynamicCast<StructTy>(&ty);
    if (!javaEntityTy) {
        return false;
    }
    auto javaEntity = javaEntityTy->decl;
    if (!javaEntity || javaEntity->fullPackageName != INTEROPLIB_PACKAGE_NAME) {
        return false;
    }
    return javaEntity->identifier == INTEROPLIB_CFFI_JAVA_ENTITY;
}

namespace {

Ptr<PropDecl> GetJavaEntityIsNullCall(StructDecl& entityDecl)
{
    for (auto& member : entityDecl.GetMemberDecls()) {
        if (auto isNullField = As<ASTKind::PROP_DECL>(member.get())) {
            if (isNullField->identifier == INTEROPLIB_CFFI_JAVA_ENTITY_IS_NULL_ID) {
                return isNullField;
            }
        }
    }

    return nullptr;
}

[[maybe_unused]] OwnedPtr<Expr> CreateJavaEntityIsNullExpr(OwnedPtr<Expr> entity, Ptr<StructDecl> entityDecl)
{
    auto isNullProp = GetJavaEntityIsNullCall(*entityDecl);
    if (!isNullProp) {
        return nullptr;
    }
    CJC_ASSERT_WITH_MSG(!isNullProp->getters.empty(), "getters cannot be empty");
    auto access = CreateMemberAccess(std::move(entity), *isNullProp->getters[0]);
    CopyBasicInfo(access->baseExpr.get(), access.get());
    auto call = CreateCallExpr(
        std::move(access), {}, isNullProp->getters[0], isNullProp->GetTy(), CallKind::CALL_DECLARED_FUNCTION);
    CopyBasicInfo(call->baseFunc.get(), call.get());
    return call;
}

bool IsWrappingConstructorOfJavaImpl(Decl& refWrapperMember)
{
    if (!refWrapperMember.TestAttr(Attribute::CONSTRUCTOR, Attribute::COMPILER_ADD)) {
        return false;
    }

    auto method = As<ASTKind::FUNC_DECL>(&refWrapperMember);
    if (!method) {
        return false;
    }
    auto& ctor = *method;

    if (!ctor.funcBody || ctor.funcBody->paramLists.empty()) {
        return false;
    }

    auto& params = ctor.funcBody->paramLists[0]->params;
    if (params.size() != 2) {
        return false;
    }

    auto& javaRefParam = *params[0];
    if (!InteropLibBridge::IsJavaEntityTy(*javaRefParam.GetTy())) {
        return false;
    }

    auto& regIdParam = *params[1];
    if (regIdParam.GetTy()->kind != TypeKind::TYPE_INT64) {
        return false;
    }

    return true;
}

FuncDecl& GetJavaImplWrappingConstructor(ClassDecl& refWrapper)
{
    CJC_ASSERT(IsImplReferenceWrapper(refWrapper));

    Ptr<FuncDecl> ctor;
    for (auto member : refWrapper.GetMemberDeclPtrs()) {
        if (!IsWrappingConstructorOfJavaImpl(*member)) {
            continue;
        }
        ctor = StaticAs<ASTKind::FUNC_DECL>(member);
        break;
    }

    CJC_NULLPTR_CHECK(ctor);
    return *ctor;
}

} // namespace

OwnedPtr<Expr> InteropLibBridge::UnwrapJavaMirrorOption(
    OwnedPtr<Expr> entity, Ptr<Ty> ty, const ClassLikeDecl& mirror, bool toRaw)
{
    CJC_ASSERT(ty->IsCoreOptionType());
    auto curFile = entity->curFile;
    CJC_NULLPTR_CHECK(curFile);
    CJC_ASSERT_WITH_MSG(!ty->typeArgs.empty(), "Option type must be generic");
    auto declTy = ty->typeArgs[0];
    auto decl = Ty::GetDeclOfTy(declTy);
    CJC_ASSERT(IsMirror(*decl) || declTy->IsString() || (toRaw && IsImpl(*decl)));

    auto actualTy = toRaw ? Ptr(&GetJniJobjectTy()) : ty;

    return utils.CreateOptionMatch(
        CreateGetJavaEntityOrNullCall(std::move(entity)),
        [this, curFile, declTy, &mirror, toRaw](VarDecl& e) {
            auto unwrapped = UnwrapJavaEntity(WithinFile(CreateRefExpr(e), curFile), declTy, mirror, toRaw);
            return unwrapped;
        },
        [this, ty, toRaw]() { return toRaw ? CreateJobjectNull() : utils.CreateOptionNoneRef(ty->typeArgs[0]); },
        actualTy);
}

OwnedPtr<Expr> InteropLibBridge::UnwrapJavaImplOption(
    OwnedPtr<Expr> env, OwnedPtr<Expr> entityOption, Ptr<Ty> ty, const ClassLikeDecl& mirror, bool toRaw)
{
    CJC_ASSERT_WITH_MSG(!ty->typeArgs.empty(), "Option type must be generic");
    auto implTy = ty->typeArgs[0];
    CJC_ASSERT(implTy->IsClass());
    auto actualTy = toRaw ? Ptr(&GetJniJobjectTy()) : ty;
    auto curFile = entityOption->curFile;

    return utils.CreateOptionMatch(
        CreateGetJavaEntityOrNullCall(std::move(entityOption)),
        [this, curFile, implTy, &env, &mirror, toRaw](VarDecl& e) {
            if (toRaw) {
                // To @C CPointer appropriate for JNI
                return UnwrapJavaEntity(WithinFile(CreateRefExpr(e), curFile), implTy, mirror, toRaw);
            }

            // as impl reference wrapper
            return UnwrapJavaImpl(std::move(env), WithinFile(CreateRefExpr(e), curFile), implTy);
        },
        [this, ty, toRaw]() { return toRaw ? CreateJobjectNull() : utils.CreateOptionNoneRef(ty->typeArgs[0]); },
        actualTy);
}

OwnedPtr<Expr> InteropLibBridge::UnwrapJavaImpl(OwnedPtr<Expr> env, OwnedPtr<Expr> entity, Ptr<Ty> ty)
{
    auto curFile = entity->curFile;
    auto classTy = StaticCast<ClassTy*>(ty.get());
    CJC_ASSERT(classTy->decl && IsImplReferenceWrapper(*classTy->decl));

    auto& ctor = GetJavaImplWrappingConstructor(*classTy->decl);

    std::vector<OwnedPtr<FuncArg>> refWrapperCtorArgs;
    refWrapperCtorArgs.push_back(CreateFuncArg(ASTCloner::Clone(entity.get())));
    refWrapperCtorArgs.push_back(CreateFuncArg(
        CreateGetRegistryIdCall(std::move(env), std::move(entity))));

    auto refWrapperCtorCall = CreateCallExpr(
        WithinFile(CreateRefExpr(ctor), curFile),
        std::move(refWrapperCtorArgs),
        &ctor, ty, CallKind::CALL_OBJECT_CREATION);

    return refWrapperCtorCall;
}

OwnedPtr<Expr> InteropLibBridge::CreateEnsureNotNullCall(OwnedPtr<Expr> entity)
{
    auto curFile = entity->curFile;
    return CreateCall(GetEnsureNotNullDecl(), curFile, std::move(entity));
}

OwnedPtr<Expr> InteropLibBridge::CreateGetJavaEntityOrNullCall(OwnedPtr<Expr> entity)
{
    auto curFile = entity->curFile;
    return CreateCall(GetGetJavaEntityOrNullDecl(), curFile, std::move(entity));
}

OwnedPtr<Expr> InteropLibBridge::UnwrapJavaArrayEntity(OwnedPtr<Expr> entity, Ptr<Ty> ty, const ClassLikeDecl& mirror)
{
    auto isSetOperation = ty == TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT);
    if (isSetOperation) {
        return CreateUnitExpr();
    }

    auto castType = MakeOwned<Type>();
    castType->SetTy(ty);
    auto varPattern = CreateVarPattern(V_COMPILER, ty);
    auto curFile = entity->curFile;
    CJC_NULLPTR_CHECK(curFile);
    varPattern->curFile = curFile;
    varPattern->varDecl->curFile = curFile;
    auto varPatternRef = WithinFile(CreateRefExpr(*(varPattern->varDecl)), curFile);
    CJC_ASSERT_WITH_MSG(!mirror.generic->typeParameters.empty(), "JArray type must be generic");
    auto genericParam = mirror.generic->typeParameters[0].get();
    auto entityPtr = entity.get();

    return CreateMatchByTypeArgument(genericParam,
        GenerateTypeMappingWithSelector([this, &entityPtr, &mirror](TypeKind kind, Ptr<Ty> ty) {
            return SelectEntityUnwrapperByTypeKind(kind, ty, entityPtr, mirror);
        }),
        ty, CreateMatchWithTypeCast(std::move(entity), ty));
}

OwnedPtr<Expr> InteropLibBridge::UnwrapJavaPrimitiveEntity(OwnedPtr<Expr> entity, Ptr<Ty> ty)
{
    auto funcDecl = GetUnwrapJavaEntityDecl();
    if (!funcDecl) {
        return nullptr;
    }
    auto isPrimitive = ty->IsPrimitive();
    auto unitTy = TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT);
    auto actualEntityTy = isPrimitive ? ty : typeManager.GetPointerTy(unitTy);
    auto curFile = entity->curFile;
    std::vector<OwnedPtr<FuncArg>> callArgs;
    callArgs.emplace_back(CreateFuncArg(std::move(entity)));
    auto fdRef = WithinFile(CreateRefExpr(*funcDecl), curFile);
    fdRef->instTys.push_back(actualEntityTy);
    fdRef->SetTy(typeManager.GetInstantiatedTy(funcDecl->GetTy(), GenerateTypeMapping(*funcDecl, fdRef->instTys)));
    auto callExpr = CreateCallExpr(
        std::move(fdRef), std::move(callArgs), funcDecl, actualEntityTy, CallKind::CALL_DECLARED_FUNCTION);
    callExpr->curFile = curFile;
    return std::move(callExpr);
}

/**
  * Lift Java boundary reference (possibly null) into a typed CJ value
  */
OwnedPtr<Expr> InteropLibBridge::UnwrapJavaEntity(OwnedPtr<Expr> entity, Ptr<Ty> ty, const Decl& outerDecl, bool toRaw)
{
    auto curFile = entity->curFile;
    if (!entity || !ty) {
        return nullptr;
    }

    auto isPrimitive = ty->IsPrimitive();
    if (isPrimitive || (toRaw && !ty->IsCoreOptionType())) {
        return UnwrapJavaPrimitiveEntity(std::move(entity), ty);
    }

    if (IsJArray(outerDecl) && ty->IsGeneric()) {
        return UnwrapJavaArrayEntity(std::move(entity), ty, static_cast<const ClassLikeDecl&>(outerDecl));
    } else if (ty == GetJavaEntityTy()) {
        return std::move(entity);
    }

    if (ty->IsString()) {
        entity = CreateEnsureNotNullCall(std::move(entity));
		// Convert jstring from JavaEntity to Cangjie String.
        return CreateJavaStringToCangjieCall(CreateGetJniEnvCall(curFile), std::move(entity));
    }

    if (ty->IsCoreOptionType()) {
        auto classLikeDecl = DynamicCast<const ClassLikeDecl*>(&outerDecl);
        CJC_NULLPTR_CHECK(classLikeDecl);
        CJC_ASSERT_WITH_MSG(!ty->typeArgs.empty(), "Option type must be generic");
        auto actualTy = ty->typeArgs[0];
        auto actualDecl = Ty::GetDeclOfTy(actualTy);
        if (IsMirror(*actualDecl) || actualTy->IsString()) {
            return UnwrapJavaMirrorOption(std::move(entity), ty, *classLikeDecl, toRaw);
        }

        return UnwrapJavaImplOption(CreateGetJniEnvCall(curFile), std::move(entity), ty, *classLikeDecl, toRaw);
    }

    if (ty->IsTuple()) {
        return CreateGetFromRegistryByEntityCall(CreateGetJniEnvCall(curFile), std::move(entity), ty, false);
    }

    auto classLikeTy = DynamicCast<ClassLikeTy*>(ty.get());
    if (!classLikeTy) {
        diag.DiagnoseRefactor(DiagKindRefactor::sema_java_interop_not_supported, DEFAULT_POSITION, "type " + ty->name);
        return nullptr;
    }

    auto decl = classLikeTy->commonDecl;
    if (!decl) {
        diag.DiagnoseRefactor(
            DiagKindRefactor::sema_java_interop_not_supported, DEFAULT_POSITION, "unknown decl " + ty->name);
        return nullptr;
    }

    if (IsMirror(*decl)) {
        return CreateMirrorConstructorCall(importManager, std::move(entity), ty);
    } else {
        CJC_ASSERT(IsImpl(*decl));
        return UnwrapJavaImpl(CreateGetJniEnvCall(curFile), std::move(entity), ty);
    }
}

OwnedPtr<FuncDecl> InteropLibBridge::CreateDeletingGlobalRefFinalizer(ClassDecl& decl)
{
    static auto unitTy = TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT);
    auto curFile = decl.curFile;
    auto fbody = CreateFuncBody({}, nullptr, CreateBlock({}, unitTy), unitTy);
    fbody->paramLists.emplace_back(MakeOwned<FuncParamList>());
    auto delCall = CreateDeleteGlobalRefCall(CreateGetJniEnvCall(curFile), CreateJavaRefCall(decl, curFile));
    fbody->body->body.emplace_back(std::move(delCall));
    auto fd = CreateFuncDecl("~init", std::move(fbody), typeManager.GetFunctionTy({}, unitTy));
    fd->EnableAttr(Attribute::PRIVATE, Attribute::FINALIZER, Attribute::IN_CLASSLIKE);
    fd->linkage = Linkage::EXTERNAL;
    fd->funcBody->funcDecl = fd.get();
    fd->fullPackageName = decl.fullPackageName;
    fd->outerDecl = Ptr(&decl);
    // The generated finalizer must be attached to a file, otherwise downstream passes (e.g. CHIR
    // AST2CHIR::CreateFuncSignatureAndSetGlobalCache) dereference a null `curFile`. `decl.curFile`
    // is guaranteed non-null here since the finalizer body was built from it above.
    fd->curFile = curFile;
    return fd;
}

bool InteropLibBridge::IsInteropLibAccessible(const ImportManager& importManager)
{
    return importManager.GetPackageDecl(INTEROPLIB_PACKAGE_NAME);
}

bool InteropLibBridge::IsInteropLibAccessible() const
{
    return IsInteropLibAccessible(importManager);
}

void InteropLibBridge::CheckInteropLibVersion()
{
    auto versionDecl = GetInteropLibVersionVarDecl();

    if (!versionDecl && !IsInteropLibAccessible()) {
        diag.DiagnoseRefactor(DiagKindRefactor::sema_java_mirror_interoplib_must_be_imported, DEFAULT_POSITION);
        return;
    }

    if (!versionDecl || !versionDecl->initializer || versionDecl->initializer->astKind != ASTKind::LIT_CONST_EXPR ||
        versionDecl->initializer->TyKind() != TypeKind::TYPE_INT64) {
        diag.DiagnoseRefactor(DiagKindRefactor::sema_java_interoplib_version_too_old, DEFAULT_POSITION,
            std::to_string(INTEROPLIB_VERSION));
        return;
    }

    auto libVersionLit = StaticAs<ASTKind::LIT_CONST_EXPR>(versionDecl->initializer.get());
    auto libVersion = std::stoi(libVersionLit->stringValue);
    if (libVersion != INTEROPLIB_VERSION) {
        diag.DiagnoseRefactor(DiagKindRefactor::sema_java_interoplib_version_mismatch, DEFAULT_POSITION,
            libVersionLit->stringValue, std::to_string(INTEROPLIB_VERSION));
    }
}

} // namespace Cangjie::Interop::Java
