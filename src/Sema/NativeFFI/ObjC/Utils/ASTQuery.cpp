// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements predicates and lookups over the AST entities of Cangjie <-> Objective-C interopability.
 */

#include "ASTQuery.h"
#include "NativeFFI/ObjC/Utils/NameGenerator.h"
#include "cangjie/AST/ASTCasting.h"
#include "cangjie/AST/Symbol.h"
#include "cangjie/Utils/CheckUtils.h"
#include "cangjie/Utils/ConstantsUtils.h"

using namespace Cangjie::AST;

namespace Cangjie::Interop::ObjC {

bool IsObjCCompatible(const Ty& ty) noexcept
{
    switch (ty.kind) {
        case TypeKind::TYPE_UNIT:
        case TypeKind::TYPE_INT8:
        case TypeKind::TYPE_INT16:
        case TypeKind::TYPE_INT32:
        case TypeKind::TYPE_INT64:
        case TypeKind::TYPE_INT_NATIVE:
        case TypeKind::TYPE_IDEAL_INT:
        case TypeKind::TYPE_UINT8:
        case TypeKind::TYPE_UINT16:
        case TypeKind::TYPE_UINT32:
        case TypeKind::TYPE_UINT64:
        case TypeKind::TYPE_UINT_NATIVE:
        case TypeKind::TYPE_FLOAT32:
        case TypeKind::TYPE_FLOAT64:
        case TypeKind::TYPE_IDEAL_FLOAT:
        case TypeKind::TYPE_BOOLEAN:
        case TypeKind::TYPE_CSTRING:
            return true;
        case TypeKind::TYPE_STRUCT:
            if (IsObjCPointer(ty)) {
                CJC_ASSERT(ty.typeArgs.size() == 1);
                return IsObjCCompatible(*ty.typeArgs[0]);
            }
            if (Ty::IsCStructType(ty)) {
                return true;
            }
            if (IsObjCFunc(ty)) {
                CJC_ASSERT(ty.typeArgs.size() == 1);
                auto tyArg = ty.typeArgs[0];
                if (!tyArg->IsFunc() || tyArg->IsCFunc()) {
                    return false;
                }
                return std::all_of(std::begin(tyArg->typeArgs), std::end(tyArg->typeArgs),
                    [](auto ty) { return IsObjCCompatible(*ty); });
            }
            return false;
        case TypeKind::TYPE_CLASS:
        case TypeKind::TYPE_INTERFACE:
            if (IsObjCMirror(ty) || IsObjCImpl(ty)) {
                return true;
            }
            if (IsObjCBlock(ty)) {
                CJC_ASSERT(ty.typeArgs.size() == 1);
                auto tyArg = ty.typeArgs[0];
                if (!tyArg->IsFunc() || tyArg->IsCFunc()) {
                    return false;
                }
                return std::all_of(std::begin(tyArg->typeArgs), std::end(tyArg->typeArgs),
                    [](auto ty) { return IsObjCCompatible(*ty); });
            }
            return false;
        case TypeKind::TYPE_ENUM:
            if (!ty.IsCoreOptionType()) {
                return false;
            };
            CJC_ASSERT(ty.typeArgs[0]);
            if (IsObjCMirror(*ty.typeArgs[0]) || IsObjCImpl(*ty.typeArgs[0])) {
                return true;
            }
            return false;
        case TypeKind::TYPE_FUNC:
            return ty.IsCFunc();
        default:
            return false;
    }
}

bool IsObjCObjectType(const Ty& ty) noexcept
{
    if (ty.IsCoreOptionType()) {
        CJC_ASSERT_WITH_MSG(ty.typeArgs.size() == 1, "Option<T> has exactly one type arg");
        return IsObjCObjectType(*ty.typeArgs[0]);
    }
    return IsObjCMirror(ty) || IsObjCImpl(ty) || IsObjCMirrorInterfaceHandleWrapper(ty);
}

bool IsObjCMirror(const Decl& decl) noexcept
{
    return decl.TestAttr(Attribute::OBJ_C_MIRROR);
}

bool IsObjCMirror(const Ty& ty) noexcept
{
    const auto classLikeTy = DynamicCast<ClassLikeTy*>(&ty);
    return classLikeTy && classLikeTy->commonDecl && IsObjCMirror(*classLikeTy->commonDecl);
}

bool IsValidObjCMirror(const Ty& ty) noexcept
{
    const auto classLikeTy = DynamicCast<ClassLikeTy*>(&ty);
    if (!classLikeTy || !classLikeTy->commonDecl || !IsObjCMirror(*classLikeTy->commonDecl)) {
        return false;
    }

    for (auto superInterfaceTy : classLikeTy->GetSuperInterfaceTys()) {
        if (!IsValidObjCMirror(*superInterfaceTy)) {
            return false;
        }
    }

    // The super class of an @ObjCMirror class must be an @ObjCMirror as well.
    if (const auto classTy = DynamicCast<ClassTy*>(&ty)) {
        // Hierarchy root @ObjCMirror class.
        if (!classTy->GetSuperClassTy() || classTy->GetSuperClassTy()->IsObject()) {
            return true;
        }
        return IsValidObjCMirror(*classTy->GetSuperClassTy());
    }

    return true;
}

bool IsObjCMirrorSubtype(const Decl& decl) noexcept
{
    const auto ty = decl.GetTy();
    return IsObjCMirrorSubtype(*ty);
}

bool IsObjCMirrorSubtype(const Ty& ty) noexcept
{
    if (const auto interfaceTy = DynamicCast<InterfaceTy*>(&ty)) {
        return HasObjCMirrorSuperInterface(*interfaceTy->declPtr);
    }

    const auto classTy = DynamicCast<ClassTy*>(&ty);
    if (!classTy) {
        return false;
    }

    auto hasMirrorSuperInterface = HasObjCMirrorSuperInterface(*classTy->declPtr);
    if (!classTy->GetSuperClassTy() || classTy->GetSuperClassTy()->IsObject()) {
        return hasMirrorSuperInterface;
    }

    return IsObjCMirrorSubtype(*classTy->GetSuperClassTy()) || IsObjCMirror(*classTy->GetSuperClassTy());
}

bool IsObjCImpl(const ClassDecl& decl) noexcept
{
    return decl.TestAttr(Attribute::OBJ_C_IMPL);
}

bool IsObjCImpl(const Ty& ty) noexcept
{
    const auto classTy = DynamicCast<ClassTy*>(&ty);
    return classTy && classTy->declPtr && IsObjCImpl(*classTy->declPtr);
}

bool IsObjCMirrorInterfaceHandleWrapper(const ClassDecl& decl) noexcept
{
    return decl.TestAttr(Attribute::OBJ_C_MIRROR_INTERFACE_HANDLE_WRAPPER);
}

bool IsObjCMirrorInterfaceHandleWrapper(const Ty& ty) noexcept
{
    const auto classTy = DynamicCast<ClassTy*>(&ty);
    return classTy && classTy->declPtr && IsObjCMirrorInterfaceHandleWrapper(*classTy->declPtr);
}

bool IsObjCImplRegistryCompanion(const Decl& decl) noexcept
{
    return decl.TestAttr(Attribute::OBJ_C_IMPL_REGISTRY_COMPANION);
}

bool IsObjCImplRegistryCompanion(const Ty& ty) noexcept
{
    const auto classTy = DynamicCast<ClassTy*>(&ty);
    return classTy && classTy->declPtr && IsObjCImplRegistryCompanion(*classTy->declPtr);
}

namespace {

bool InObjCLangPackage(const Decl& decl) noexcept
{
    return decl.fullPackageName == OBJ_C_LANG_PACKAGE_IDENT;
}

bool IsObjCPointerImpl(const StructDecl& structDecl) noexcept
{
    return InObjCLangPackage(structDecl) && structDecl.identifier == OBJ_C_POINTER_IDENT;
}

bool IsObjCFuncImpl(const StructDecl& structDecl) noexcept
{
    return InObjCLangPackage(structDecl) && structDecl.identifier == OBJ_C_FUNC_IDENT;
}

bool IsObjCBlockImpl(const ClassDecl& cd) noexcept
{
    return InObjCLangPackage(cd) && cd.identifier == OBJ_C_BLOCK_IDENT;
}

bool IsObjCIdImpl(const InterfaceDecl& id) noexcept
{
    return InObjCLangPackage(id) && id.identifier == OBJ_C_ID_IDENT;
}

} // namespace

bool IsObjCPointer(const Decl& decl) noexcept
{
    if (const auto structDecl = DynamicCast<StructDecl*>(&decl)) {
        return IsObjCPointerImpl(*structDecl);
    }
    return false;
}

bool IsObjCPointer(const Ty& ty) noexcept
{
    if (const auto structTy = DynamicCast<StructTy*>(&ty)) {
        return IsObjCPointerImpl(*structTy->decl);
    }
    return false;
}

bool IsObjCFunc(const Decl& decl) noexcept
{
    if (const auto structDecl = DynamicCast<StructDecl*>(&decl)) {
        return IsObjCFuncImpl(*structDecl);
    }
    return false;
}

bool IsObjCFunc(const Ty& ty) noexcept
{
    if (const auto structTy = DynamicCast<StructTy*>(&ty)) {
        return IsObjCFuncImpl(*structTy->decl);
    }
    return false;
}

bool IsObjCBlock(const Decl& decl) noexcept
{
    if (const auto classDecl = DynamicCast<ClassDecl*>(&decl)) {
        return IsObjCBlockImpl(*classDecl);
    }
    return false;
}

bool IsObjCBlock(const Ty& ty) noexcept
{
    if (const auto classTy = DynamicCast<ClassTy*>(&ty)) {
        return IsObjCBlockImpl(*classTy->decl);
    }
    return false;
}

bool IsObjCId(const Ty& ty) noexcept
{
    if (const auto iTy = DynamicCast<InterfaceTy*>(&ty)) {
        return IsObjCId(*iTy->decl);
    }
    return false;
}

bool IsObjCId(const Decl& decl) noexcept
{
    if (const auto id = DynamicCast<InterfaceDecl*>(&decl)) {
        return IsObjCIdImpl(*id);
    }
    return false;
}

bool IsObjCFuncOrBlock(const Decl& decl) noexcept
{
    return IsObjCFunc(decl) || IsObjCBlock(decl);
}

bool IsObjCFuncOrBlock(const Ty& ty) noexcept
{
    return IsObjCFunc(ty) || IsObjCBlock(ty);
}

bool IsNSObject(const ClassDecl& cd) noexcept
{
    return NameGenerator::GetObjCDeclName(cd) == NSOBJECT_CLASS_IDENT;
}

bool IsNSString(const ClassDecl& cd) noexcept
{
    return NameGenerator::GetObjCDeclName(cd) == NSSTRING_CLASS_IDENT;
}

Ptr<ClassDecl> GetObjCMirrorInterfaceHandleWrapper(ImportManager& importManager, const InterfaceDecl& decl) noexcept
{
    CJC_ASSERT(IsObjCMirror(*decl.GetTy()));
    auto synthetic =
        importManager.GetImportedDecl<ClassDecl>(decl.fullPackageName, NameGenerator::GenerateHandleWrapperName(decl));

    CJC_NULLPTR_CHECK(synthetic);
    CJC_ASSERT(IsObjCMirrorInterfaceHandleWrapper(*synthetic));

    return Ptr(synthetic);
}

bool HasObjCMirrorSuperInterface(const ClassLikeDecl& decl) noexcept
{
    for (auto parentTy : decl.GetSuperInterfaceTys()) {
        if (IsObjCMirror(*parentTy)) {
            return true;
        }
    }

    return false;
}

Ptr<ClassDecl> GetObjCMirrorSuperClass(const ClassDecl& decl) noexcept
{
    auto superClass = decl.GetSuperClassDecl();
    if (superClass && IsObjCMirror(*superClass->GetTy())) {
        return superClass;
    }

    if (superClass && IsObjCImpl(*superClass->GetTy())) {
        return GetObjCMirrorSuperClass(*superClass);
    }

    return nullptr;
}

bool HasObjCMirrorSuperClass(const ClassDecl& decl) noexcept
{
    return GetObjCMirrorSuperClass(decl) != nullptr;
}

Ptr<ClassDecl> GetObjCImplSuperClass(const ClassDecl& decl) noexcept
{
    auto superCd = decl.GetSuperClassDecl();
    if (!superCd || !IsObjCImpl(*superCd)) {
        return nullptr;
    }
    return superCd;
}

bool HasObjCImplSuperClass(const ClassDecl& decl) noexcept
{
    return GetObjCImplSuperClass(decl) != nullptr;
}

Ptr<ClassDecl> GetObjCImplRegistryCompanionSuperClass(const ClassDecl& decl) noexcept
{
    auto superClass = decl.GetSuperClassDecl();
    if (superClass && IsObjCImplRegistryCompanion(*superClass->GetTy())) {
        return superClass;
    }
    return nullptr;
}

bool HasObjCImplRegistryCompanionSuperClass(const ClassDecl& decl) noexcept
{
    return GetObjCImplRegistryCompanionSuperClass(decl) != nullptr;
}

bool IsNativeHandleGetter(const Decl& decl) noexcept
{
    return decl.identifier == NATIVE_HANDLE_GETTER_IDENT;
}

Ptr<FuncDecl> GetNativeHandleGetter(const ClassLikeDecl& decl) noexcept
{
    // The getter is declared once per class hierarchy, on the root @ObjCMirror class owning the `$obj` field.
    if (auto classDecl = DynamicCast<ClassDecl*>(&decl); classDecl) {
        if (auto mirrorSuperClass = GetObjCMirrorSuperClass(*classDecl); mirrorSuperClass) {
            return GetNativeHandleGetter(*mirrorSuperClass);
        }
    }

    return GetMemberDecl<ASTKind::FUNC_DECL>(decl, IsNativeHandleGetter);
}

namespace {
bool IsNativeHandleFieldDecl(Decl& decl) noexcept
{
    return decl.identifier == NATIVE_HANDLE_IDENT;
}
} // namespace

Ptr<VarDecl> GetNativeHandleField(const ClassDecl& decl) noexcept
{
    if (auto classDecl = DynamicCast<ClassDecl*>(&decl); classDecl) {
        if (auto mirrorSuperClass = GetObjCMirrorSuperClass(*classDecl); mirrorSuperClass) {
            return GetNativeHandleField(*mirrorSuperClass);
        }
    }

    return GetMemberDecl<ASTKind::VAR_DECL>(decl, IsNativeHandleFieldDecl);
}

Ptr<VarDecl> GetObjCImplRegCompanionField(const ClassDecl& decl) noexcept
{
    return GetMemberDecl<ASTKind::VAR_DECL>(decl, IsObjCImplRegistryCompanionField);
}

bool IsObjCImplRegistryCompanionField(const Decl& decl) noexcept
{
    return decl.identifier == REGISTRY_COMPANION_FIELD_IDENT;
}

Ptr<FuncDecl> GetFinalizer(const ClassDecl& decl) noexcept
{
    return GetMemberDecl<ASTKind::FUNC_DECL>(decl, [](auto& fd) { return fd.IsFinalizer(); });
}

bool IsObjCInitMethod(const FuncDecl& decl) noexcept
{
    return decl.TestAttr(Attribute::OBJ_C_INIT);
}

namespace {

bool HasNativeHandleBaseCtorShape(const FuncDecl& fd) noexcept
{
    if (!fd.TestAttr(Attribute::CONSTRUCTOR) || !fd.funcBody || fd.funcBody->paramLists.empty()) {
        return false;
    }

    auto& params = fd.funcBody->paramLists[0]->params;
    return params.size() == 1 && params[0]->identifier == NATIVE_HANDLE_IDENT;
}

bool HasObjCImplBaseCtorShape(const FuncDecl& fd) noexcept
{
    if (!fd.TestAttr(Attribute::CONSTRUCTOR) || !fd.funcBody || fd.funcBody->paramLists.empty()) {
        return false;
    }

    auto& params = fd.funcBody->paramLists[0]->params;
    return params.size() == 2 && params[0]->identifier == NATIVE_HANDLE_IDENT &&
        params[1]->identifier == REGISTRY_ID_IDENT;
}

Ptr<ClassDecl> GetOwnerClass(const FuncDecl& fd) noexcept
{
    return DynamicCast<ClassDecl*>(fd.outerDecl);
}

} // namespace

bool IsObjCMirrorBaseCtor(const FuncDecl& fd) noexcept
{
    auto owner = GetOwnerClass(fd);
    return owner && IsObjCMirror(*owner) && HasNativeHandleBaseCtorShape(fd);
}

bool IsObjCMirrorInterfaceHandleWrapperBaseCtor(const FuncDecl& fd) noexcept
{
    auto owner = GetOwnerClass(fd);
    return owner && IsObjCMirrorInterfaceHandleWrapper(*owner) && HasNativeHandleBaseCtorShape(fd);
}

bool IsObjCImplRegCompanionBaseCtor(const FuncDecl& fd) noexcept
{
    auto owner = GetOwnerClass(fd);
    return owner && IsObjCImplRegistryCompanion(*owner) && HasNativeHandleBaseCtorShape(fd);
}

bool IsObjCImplBaseCtor(const FuncDecl& fd) noexcept
{
    auto owner = GetOwnerClass(fd);
    return owner && IsObjCImpl(*owner) && HasObjCImplBaseCtorShape(fd);
}

bool IsBaseCtor(const FuncDecl& fd) noexcept
{
    return IsObjCImplBaseCtor(fd) || IsObjCMirrorBaseCtor(fd) || IsObjCMirrorInterfaceHandleWrapperBaseCtor(fd) ||
        IsObjCImplRegCompanionBaseCtor(fd);
}

Ptr<FuncDecl> GetBaseCtor(const ClassDecl& decl) noexcept
{
    return GetMemberDecl<ASTKind::FUNC_DECL>(decl, IsBaseCtor);
}

Ptr<FuncDecl> GetObjCMirrorInterfaceHandleWrapperBaseCtor(const ClassDecl& handleWrapper) noexcept
{
    return GetMemberDecl<ASTKind::FUNC_DECL>(handleWrapper, IsObjCMirrorInterfaceHandleWrapperBaseCtor);
}

Ptr<FuncDecl> GetObjCMirrorBaseCtor(const ClassDecl& decl) noexcept
{
    return GetMemberDecl<ASTKind::FUNC_DECL>(decl, IsObjCMirrorBaseCtor);
}

Ptr<FuncDecl> GetObjCImplBaseCtor(const ClassDecl& decl) noexcept
{
    return GetMemberDecl<ASTKind::FUNC_DECL>(decl, IsObjCImplBaseCtor);
}

Ptr<FuncDecl> GetObjCImplRegCompanionBaseCtor(const ClassDecl& decl) noexcept
{
    return GetMemberDecl<ASTKind::FUNC_DECL>(decl, IsObjCImplRegCompanionBaseCtor);
}

bool IsObjCImplRegDataCtor(const FuncDecl& fd) noexcept
{
    if (!fd.funcBody || fd.funcBody->paramLists.empty()) {
        return false;
    }

    auto& params = fd.funcBody->paramLists[0]->params;
    if (params.empty()) {
        return false;
    }

    return params[0]->identifier == REGISTRY_DATA_PARAM_IDENT;
}

Ptr<FuncDecl> GetObjCImplRegDataCtor(const ClassDecl& impl, const FuncDecl& origin) noexcept
{
    CJC_ASSERT_WITH_MSG(!origin.funcBody->paramLists.empty(), "expected at least one param list");
    auto& originParams = origin.funcBody->paramLists[0]->params;

    for (auto& member : impl.GetMemberDeclPtrs()) {
        auto fd = As<ASTKind::FUNC_DECL>(member);
        if (!fd || !IsObjCImplRegDataCtor(*fd)) {
            continue;
        }

        CJC_ASSERT_WITH_MSG(!fd->funcBody->paramLists.empty(), "expected at least one param list");
        // the reg data ctor is `origin` cloned with a single `$regData` param prepended, so the rest of the
        // params must line up 1-to-1 with `origin`'s.
        auto& fdParams = fd->funcBody->paramLists[0]->params;
        if (fdParams.size() != originParams.size() + 1) {
            continue;
        }

        auto matched = true;
        for (size_t i = 1; i < fdParams.size(); ++i) {
            auto& fdParam = fdParams[i];
            auto& originParam = originParams[i - 1];
            if (fdParam->identifier != originParam->identifier || fdParam->GetTy() != originParam->GetTy()) {
                matched = false;
                break;
            }
        }

        if (matched) {
            return fd;
        }
    }

    return nullptr;
}

bool IsObjCImplUserCtor(const FuncDecl& fd) noexcept
{
    return fd.TestAttr(Attribute::CONSTRUCTOR) && !IsObjCImplBaseCtor(fd) && !IsObjCImplRegDataCtor(fd);
}

bool IsGeneratedNSStringCtor(const FuncDecl& fd) noexcept
{
    if (fd.identifier.Val() != INIT_IDENT || !fd.TestAttr(Attribute::CONSTRUCTOR, Attribute::COMPILER_ADD) ||
        !fd.funcBody || fd.funcBody->paramLists.empty()) {
        return false;
    }

    auto& params = fd.funcBody->paramLists[0]->params;
    return params.size() == 1 && params[0]->type->symbol->name == STD_LIB_STRING;
}

bool IsGeneratedNSObjectToString(const FuncDecl& fd) noexcept
{
    if (fd.identifier.Val() != TO_STRING_METHOD_IDENT || !fd.TestAttr(Attribute::COMPILER_ADD) || !fd.funcBody ||
        fd.funcBody->paramLists.empty()) {
        return false;
    }

    return fd.funcBody->paramLists[0]->params.empty();
}

namespace {

// `init(str: String)` of the NSString mirror and `toString()` of the NSObject mirror are generated before the type
// check, see BeforeTypeCheck InsertFromStringCtor and InsertToString.
bool IsGeneratedStringConversion(const FuncDecl& fd) noexcept
{
    auto owner = GetOwnerClass(fd);
    if (!owner) {
        return false;
    }

    return (IsNSString(*owner) && IsGeneratedNSStringCtor(fd)) ||
        (IsNSObject(*owner) && IsGeneratedNSObjectToString(fd));
}

} // namespace

bool IsGeneratedMember(const Decl& decl) noexcept
{
    if (decl.identifier == NATIVE_HANDLE_IDENT || decl.identifier == GET_OBJ_C_CLASS_IDENT) {
        return true;
    }

    if (IsNativeHandleGetter(decl) || IsObjCImplRegistryCompanionField(decl)) {
        return true;
    }

    if (decl.TestAttr(Attribute::HAS_INITED_FIELD)) {
        return true;
    }

    const auto fd = DynamicCast<const FuncDecl*>(&decl);
    return fd && (IsBaseCtor(*fd) || IsObjCImplRegDataCtor(*fd) || IsGeneratedStringConversion(*fd));
}

} // namespace Cangjie::Interop::ObjC