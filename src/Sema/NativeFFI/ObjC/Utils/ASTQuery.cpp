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
        case TypeKind::TYPE_POINTER:
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
    auto handleWrapper =
        importManager.GetImportedDecl<ClassDecl>(decl.fullPackageName, NameGenerator::GenerateHandleWrapperName(decl));

    CJC_NULLPTR_CHECK(handleWrapper);
    CJC_ASSERT(IsObjCMirrorInterfaceHandleWrapper(*handleWrapper));

    return Ptr(handleWrapper);
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
    CJC_ASSERT_WITH_MSG(IsObjCImpl(decl), "only @ObjCImpl can inherit @ObjCImpl");
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
    // The getter is declared once per class hierarchy, on the root @ObjCMirror class owning the `$obj` field,
    // so climb up to that root before looking the member up.
    const ClassLikeDecl* owner = &decl;
    while (auto classDecl = DynamicCast<ClassDecl*>(owner)) {
        auto mirrorSuperClass = GetObjCMirrorSuperClass(*classDecl);
        if (!mirrorSuperClass) {
            break;
        }
        owner = mirrorSuperClass.get();
    }

    return GetMemberDecl<ASTKind::FUNC_DECL>(*owner, IsNativeHandleGetter);
}

namespace {
bool IsNativeHandleFieldDecl(Decl& decl) noexcept
{
    return decl.identifier == NATIVE_HANDLE_IDENT;
}
} // namespace

Ptr<VarDecl> GetNativeHandleField(const ClassDecl& decl) noexcept
{
    for (auto cur = &decl; cur; cur = cur->GetSuperClassDecl()) {
        if (auto field = GetMemberDecl<ASTKind::VAR_DECL>(*cur, IsNativeHandleFieldDecl)) {
            return field;
        }
    }

    return nullptr;
}

Ptr<VarDecl> GetObjCImplRegCompanionField(const ClassDecl& decl) noexcept
{
    return GetMemberDecl<ASTKind::VAR_DECL>(decl, IsObjCImplRegistryCompanionField);
}

bool IsObjCImplRegistryCompanionField(const Decl& decl) noexcept
{
    return decl.identifier == REGISTRY_COMPANION_FIELD_IDENT;
}

bool IsObjCImplMovedMemberProxy(const Decl& decl) noexcept
{
    return decl.TestAttr(Attribute::OBJ_C_IMPL_MOVED_MEMBER_PROXY);
}

Ptr<FuncDecl> GetFinalizer(const ClassDecl& decl) noexcept
{
    return GetMemberDecl<ASTKind::FUNC_DECL>(decl, [](auto& fd) { return fd.IsFinalizer(); });
}

bool IsObjCInitMethod(const FuncDecl& decl) noexcept
{
    return decl.TestAttr(Attribute::OBJ_C_INIT);
}

bool IsNativeObjCIdMarker(const Ty& ty) noexcept
{
    const auto structTy = DynamicCast<StructTy*>(&ty);
    return structTy && structTy->decl && structTy->decl->fullPackageName == OBJ_C_INTERNAL_PACKAGE_IDENT &&
        structTy->decl->identifier == NATIVE_OBJ_C_ID_MARKER_IDENT;
}

namespace {

/// The params of @p fd if it is a ctor with a param list, nullptr otherwise.
const std::vector<OwnedPtr<FuncParam>>* GetCtorParams(const FuncDecl& fd) noexcept
{
    if (!fd.TestAttr(Attribute::CONSTRUCTOR) || !fd.funcBody || fd.funcBody->paramLists.empty()) {
        return nullptr;
    }

    // taking first param list probably is not the best idea
    return &fd.funcBody->paramLists[0]->params;
}

bool IsNativeObjCIdMarkerParam(const OwnedPtr<FuncParam>& param) noexcept
{
    return Ty::IsTyCorrect(param->GetTy()) && IsNativeObjCIdMarker(*param->GetTy());
}

bool IsNativeHandleParam(const OwnedPtr<FuncParam>& param) noexcept
{
    // `NativeObjCId` is an alias of `CPointer<Unit>`.
    return Ty::IsTyCorrect(param->GetTy()) && param->GetTy()->IsPointer();
}

/**
 * `init(NativeObjCId)`, the base ctor of a class we generate whole - a registry companion or a mirror
 * interface handle wrapper - and, BACKCOMPAT, of a legacy @ObjCMirror, @see IsLegacyObjCMirror.
 *
 * The signature is writable by a user, so on its own it proves nothing; what makes it unambiguous is that
 * the owner has no user-written members at all.
 */
bool HasNativeHandleBaseCtorSignature(const FuncDecl& fd) noexcept
{
    auto params = GetCtorParams(fd);
    return params && params->size() == 1 && IsNativeHandleParam(params->front());
}

/// `init(NativeObjCId, NativeObjCIdMarker)`.
bool HasObjCMirrorBaseCtorSignature(const FuncDecl& fd) noexcept
{
    constexpr size_t paramCount = 2;
    auto params = GetCtorParams(fd);
    return params && params->size() == paramCount && IsNativeHandleParam(params->front()) &&
        IsNativeObjCIdMarkerParam(params->back());
}

/// `init(NativeObjCId, RegistryId, NativeObjCIdMarker)`.
bool HasObjCImplBaseCtorSignature(const FuncDecl& fd) noexcept
{
    constexpr size_t paramCount = 3;
    auto params = GetCtorParams(fd);
    return params && params->size() == paramCount && IsNativeHandleParam(params->front()) &&
        IsNativeObjCIdMarkerParam(params->back());
}

/**
 * A @ObjCMirror compiled before `CPointer<T>` became ObjC-compatible, so that its generated base ctor carries
 * no marker. Such a mirror can only come from a cjo, and back then `init(NativeObjCId)` was still
 * unambiguous there: a user ctor taking a `CPointer<T>` was rejected as ObjC-incompatible. Compiled from
 * source that no longer holds, hence both conditions.
 */
bool IsLegacyObjCMirror(const ClassDecl& mirror) noexcept
{
    return mirror.TestAttr(Attribute::IMPORTED) && !GetMemberDecl<ASTKind::FUNC_DECL>(mirror,
        HasObjCMirrorBaseCtorSignature);
}

} // namespace

bool HasNativeObjCIdMarkerParam(const FuncDecl& fd) noexcept
{
    if (!fd.funcBody || fd.funcBody->paramLists.empty()) {
        return false;
    }

    auto& params = fd.funcBody->paramLists[0]->params;
    return std::any_of(params.begin(), params.end(), IsNativeObjCIdMarkerParam);
}

Ptr<ClassDecl> GetOwnerClass(const Decl& decl) noexcept
{
    return As<ASTKind::CLASS_DECL>(decl.outerDecl);
}

bool IsObjCMirrorBaseCtor(const FuncDecl& fd) noexcept
{
    auto owner = GetOwnerClass(fd);
    if (!owner || !IsObjCMirror(*owner)) {
        return false;
    }

    return HasObjCMirrorBaseCtorSignature(fd) || (HasNativeHandleBaseCtorSignature(fd) && IsLegacyObjCMirror(*owner));
}

bool IsObjCMirrorInterfaceHandleWrapperBaseCtor(const FuncDecl& fd) noexcept
{
    auto owner = GetOwnerClass(fd);
    return owner && IsObjCMirrorInterfaceHandleWrapper(*owner) && HasNativeHandleBaseCtorSignature(fd);
}

bool IsObjCImplRegCompanionBaseCtor(const FuncDecl& fd) noexcept
{
    auto owner = GetOwnerClass(fd);
    return owner && IsObjCImplRegistryCompanion(*owner) && HasNativeHandleBaseCtorSignature(fd);
}

bool IsObjCImplBaseCtor(const FuncDecl& fd) noexcept
{
    auto owner = GetOwnerClass(fd);
    return owner && IsObjCImpl(*owner) && HasObjCImplBaseCtorSignature(fd);
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
    constexpr size_t regDataElemCount = 2;
    auto params = GetCtorParams(fd);
    if (!params || params->empty()) {
        return false;
    }

    // `$regData: (NativeObjCId, <impl>$reg)`. The registry companion class cannot be named from source, which
    // is what keeps the signature unwritable by a user and so tells this ctor apart from the one it clones.
    auto regDataTy = DynamicCast<TupleTy*>(params->front()->GetTy());
    return regDataTy && regDataTy->typeArgs.size() == regDataElemCount &&
        IsObjCImplRegistryCompanion(*regDataTy->typeArgs[1]);
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
            if (fdParam->GetTy() != originParam->GetTy()) {
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

    const auto fd = DynamicCast<FuncDecl*>(&decl);
    return fd && (IsBaseCtor(*fd) || IsObjCImplRegDataCtor(*fd) || IsGeneratedStringConversion(*fd));
}

bool IsObjCCompatibleFuncTy(const Ty& ty) noexcept
{
    auto fTy = DynamicCast<FuncTy>(&ty);
    if (fTy == nullptr) {
        return false;
    }
    if (fTy->isC) {
        return false;
    }

    if (!IsObjCCompatible(*fTy->retTy)) {
        return false;
    }
    for (auto argTy : fTy->paramTys) {
        if (!IsObjCCompatible(*argTy)) {
            return false;
        }
    }
    return true;
}

} // namespace Cangjie::Interop::ObjC
