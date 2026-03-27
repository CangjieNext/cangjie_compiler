// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/// @file
/// This file implements add indirect extend in CHIR.

#include "cangjie/CHIR/Transformation/AddIndirectExtend.h"
#include "cangjie/CHIR/CHIR.h"
#include "cangjie/CHIR/CHIRBuilder.h"
#include "cangjie/CHIR/AST2CHIR/GenerateVTable/VTableGenerator.h"
#include "cangjie/CHIR/DebugLocation.h"
#include "cangjie/CHIR/Type/Type.h"
#include "cangjie/CHIR/Type/CustomTypeDef.h"
#include "cangjie/CHIR/Type/ExtendDef.h"
#include "cangjie/CHIR/Utils.h"
#include "cangjie/CHIR/CHIRCasting.h"
#include "cangjie/CHIR/UserDefinedType.h"
#include "cangjie/CHIR/Value.h"
#include <deque>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <set>

namespace Cangjie::CHIR {

namespace {
enum class TypeCategory {
    GENERIC,      // GenericType
    SIMPLE,       // Type without typeArgs (or with empty typeArgs)
    CONSTRUCTOR   // Type with typeArgs
};

TypeCategory GetTypeCategory(const Type* ty)
{
    if (Is<GenericType>(ty)) {
        return TypeCategory::GENERIC;
    }
    if (ty->GetTypeArgs().empty()) {
        return TypeCategory::SIMPLE;
    }
    return TypeCategory::CONSTRUCTOR;
}

/// True iff the two types are the same constructor (same kind and same def).
/// @param ty1 First type.
/// @param ty2 Second type.
/// @return True if same constructor.
bool IsSameConstructor(const Type* ty1, const Type* ty2)
{
    if (ty1->GetTypeKind() != ty2->GetTypeKind()) {
        return false;
    }
    if (ty1->GetTypeKind() == Type::TypeKind::TYPE_CLASS) {
        auto c1 = StaticCast<ClassType>(ty1);
        auto c2 = StaticCast<ClassType>(ty2);
        return c1->GetClassDef() == c2->GetClassDef();
    }
    if (ty1->GetTypeKind() == Type::TypeKind::TYPE_STRUCT) {
        auto s1 = StaticCast<StructType>(ty1);
        auto s2 = StaticCast<StructType>(ty2);
        return s1->GetStructDef() == s2->GetStructDef();
    }
    if (ty1->GetTypeKind() == Type::TypeKind::TYPE_ENUM) {
        auto e1 = StaticCast<EnumType>(ty1);
        auto e2 = StaticCast<EnumType>(ty2);
        return e1->GetEnumDef() == e2->GetEnumDef();
    }
    if (ty1->GetTypeKind() == Type::TypeKind::TYPE_RAWARRAY) {
        auto a1 = StaticCast<RawArrayType>(ty1);
        auto a2 = StaticCast<RawArrayType>(ty2);
        return a1->GetDims() == a2->GetDims() &&
               IsSameConstructor(a1->GetElementType(), a2->GetElementType());
    }
    if (ty1->GetTypeKind() == Type::TypeKind::TYPE_VARRAY) {
        auto v1 = StaticCast<VArrayType>(ty1);
        auto v2 = StaticCast<VArrayType>(ty2);
        return v1->GetSize() == v2->GetSize() &&
               IsSameConstructor(v1->GetElementType(), v2->GetElementType());
    }
    if (ty1->GetTypeKind() == Type::TypeKind::TYPE_CPOINTER) {
        auto p1 = StaticCast<CPointerType>(ty1);
        auto p2 = StaticCast<CPointerType>(ty2);
        return IsSameConstructor(p1->GetElementType(), p2->GetElementType());
    }
    if (ty1->GetTypeKind() == Type::TypeKind::TYPE_BOXTYPE) {
        auto b1 = StaticCast<BoxType>(ty1);
        auto b2 = StaticCast<BoxType>(ty2);
        return IsSameConstructor(b1->GetBaseType(), b2->GetBaseType());
    }
    return true;
}

/// Collect all GenericTypes in a type (recursively into type args).
/// @param ty Type to traverse.
/// @param result Output: appended generic types.
void CollectGenericTypes(Type* ty, std::vector<GenericType*>& result)
{
    if (auto genTy = DynamicCast<GenericType>(ty)) {
        result.push_back(genTy);
        return;
    }
    for (auto arg : ty->GetTypeArgs()) {
        CollectGenericTypes(arg, result);
    }
}

/// Info for one extend declaration (from a parent class).
struct ExtendInfo {
    std::vector<GenericType*> extendParams;  ///< Extend's generic params.
    ClassType* clTy;                         ///< Extended type (e.g. A<SSS<R1>,R2,R3>).
    std::vector<ClassType*> interfTys;      ///< Implemented interface types.
    ExtendDef* extendDef;                   ///< Original extend def (for constraints).
};

/// Condition for adding indirect extend.
/// For class X, if X has a generic parent class Z (we check all generic parent classes), for each interface Y that Z
/// extends (through ExtendDef, including parent interfaces of extended interface, but not through inheritance in Z),
/// then in X's vtable find subtable T with key Y. Such T must exist.
/// For each function f in T: if f is declared in Y, and f is defined in a generic interface or generic extend
/// but not within Y's definition, then generate extend X<:Y. (A function in vtable for Y is declared in Y.)

/// @brief Build mapping from extended type's type arguments (eq2) to parent type's type arguments (eq1).
/// This is Step 1 of the overall AddIndirectExtend algorithm.
/// Overall Algorithm.
///   Given.
///     class B<B1,B2> <: A<B2,Q<B1>,B2>{}
///     extend<R1,R2,R3> A<SSS<R1>,R2,R3> <: I<R1,SSS<R2>>{}
///   Step 1: Build mapping from eq2 to eq1
///     - eq1 = parent type's type args: [B2, Q<B1>, B2] (from "A<B2,Q<B1>,B2>")
///     - eq2 = extended type's type args: [SSS<R1>, R2, R3] (from "A<SSS<R1>,R2,R3>")
///     - Create pairs: [(SSS<R1>, B2), (R2, Q<B1>), (R3, B2)]
///   Step 2: Match types and introduce new generics
///     - For each (eq2, eq1) pair, match types.
///       * (SSS<R1>, B2): R1 (generic) -> B2 (simple) => R1 maps to B2
///       * (R2, Q<B1>): R2 (generic) -> Q<B1> (constructor with generic B1) => R2 maps to Q<K1>, B1 maps to K1
///       * (R3, B2): R3 (generic) -> B2 (simple) => R3 maps to B2
///     - Create new generics K1, K2, ... for the generated extend
///   Step 3: Create substituted types
///     - newClassTy = B<K4,SSS<K1>> (where K4 comes from B1, K1 from R2->Q<B1>)
///     - newParentTy = A<SSS<K1>,Q<K4>,SSS<K1>>
///     - newInterfaceTys = [I<K1,SSS<Q<K4>>>]
///   Step 4: Generate extend declaration
///     - extend<K4,K1> B<K4,SSS<K1>> <: A<SSS<K1>,Q<K4>,SSS<K1>> <: I<K1,SSS<Q<K4>>> {}
/// Terminology.
///   - eq1: Parent type's type arguments (from class inheritance)
///          Example: In "class B<B1,B2> <: A<B2,Q<B1>,B2>{}", eq1 = [B2, Q<B1>, B2]
///   - eq2: Extended type's type arguments (from extend declaration)
///          Example: In "extend<R1,R2,R3> A<SSS<R1>,R2,R3> <: I<R1,SSS<R2>>{}", eq2 = [SSS<R1>, R2, R3]
/// This function (Step 1) creates pairs mapping each eq2 argument to its corresponding eq1 argument.
/// The goal is to represent A(eq2) using A(eq1), establishing the relationship between
/// the extended type's arguments and the parent type's arguments.
/// Example walkthrough.
///   Input.
///     parentTy = A<B2, Q<B1>, B2>  (eq1 = [B2, Q<B1>, B2])
///     extendedTy = A<SSS<R1>, R2, R3>  (eq2 = [SSS<R1>, R2, R3])
///   Algorithm.
///     1. Check that both types have the same number of type arguments (3 == 3) ✓
///     2. Create pairs by matching positions.
///        - Position 0: (eq2[0], eq1[0]) = (SSS<R1>, B2)
///        - Position 1: (eq2[1], eq1[1]) = (R2, Q<B1>)
///        - Position 2: (eq2[2], eq1[2]) = (R3, B2)
///   Returns: [(SSS<R1>, B2), (R2, Q<B1>), (R3, B2)]
///   These pairs will be processed in Step 2 to determine how to map generics.
std::vector<std::pair<Type*, Type*>> BuildEq2ToEq1TypeArgMapping(
    const ClassType& parentTy, const ClassType& extendedTy)
{
    std::vector<std::pair<Type*, Type*>> typeArgPairs;
    auto parentArgs = parentTy.GetTypeArgs();
    auto extendedArgs = extendedTy.GetTypeArgs();
    if (extendedArgs.size() != parentArgs.size()) {
        return typeArgPairs;
    }
    /// Match actual type arguments to preserve constructor wrappers
    for (size_t i = 0; i < extendedArgs.size(); ++i) {
        typeArgPairs.push_back({extendedArgs[i], parentArgs[i]});
    }
    return typeArgPairs;
}

/// Build extend id in the same style as user-written extends: _CN<pkgLen><pkg>X<extendedType>U<interface>$K_<n>_E
std::string BuildExtendId(const std::string& pkgName, const ClassDef& classDef,
    const ClassDef& interfaceDef, int counter)
{
    std::string id = "_C";
    id += std::string("N") + std::to_string(static_cast<int>(pkgName.size())) + pkgName;
    id += std::string("X") + classDef.GetIdentifierWithoutPrefix();
    id += std::string("U") + interfaceDef.GetIdentifierWithoutPrefix();
    id += std::string("$K_") + std::to_string(counter) + "_E";
    return id;
}

/// File name (without extension) from class def location, or "gen" if invalid.
std::string GetFileNameForExtendNaming(const ClassDef& classDef)
{
    std::string name = classDef.GetDebugLocation().GetFileName();
    auto dot = name.rfind('.');
    if (dot != std::string::npos) {
        name = name.substr(0, dot);
    }
    return name.empty() ? "gen" : name;
}

/// Base for naming generics in generated extend (same style as user: <base>2R<n>E).
std::string BuildExtendGenericNamingBase(const std::string& pkgName, const ClassDef& classDef)
{
    std::string filePart = GetFileNameForExtendNaming(classDef);
    return std::string("_C") + "N" + std::to_string(static_cast<int>(pkgName.size())) + pkgName +
           "X" + classDef.GetIdentifierWithoutPrefix() + "U" + filePart + "$K_";
}

/// Impl class to encapsulate type matching state and operations
class TypeMatchingImpl {
public:
    TypeMatchingImpl(CHIRBuilder& b, const std::string& pkgName, const ClassDef& classDef)
        : extendGenericNamingBase(BuildExtendGenericNamingBase(pkgName, classDef)), builder(b) {}

    /// Encapsulated state
    /// Maps from old generic types (from parent class) to new generic types (for generated extend)
    std::unordered_map<GenericType*, GenericType*> oldToNewGenericMap;
    /// Maps from eq2's generic types to concrete types (when eq2 generic maps to eq1 simple type)
    std::unordered_map<GenericType*, Type*> eq2GenericToNewTypeMap;
    /// Maps from eq2's generic types to new generic types (when eq2 generic maps to eq1 generic)
    std::unordered_map<GenericType*, GenericType*> eq2GenericToNewGenericMap;
    /// Maps from old generic types to wrapped types (for preserving constructor wrappers)
    std::unordered_map<GenericType*, Type*> oldGenericToWrappedTypeMap;
    std::set<std::string> usedGenericNames;
    std::string extendGenericNamingBase;
    int extendGenericParamIndex = 0;

    static std::string MakeValidGenericName(const std::string& base)
    {
        if (base.empty()) {
            return "T";
        }
        std::string out;
        for (size_t i = 0; i < base.size(); ++i) {
            char c = base[i];
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_') ||
                (i > 0 && c >= '0' && c <= '9')) {
                out += c;
            } else if (c == '-' || c == ' ') {
                out += '_';
            }
        }
        return out.empty() ? "T" : out;
    }

    std::string GetUniqueDisplayName(const std::string& base)
    {
        std::string name = MakeValidGenericName(base);
        std::string candidate = name;
        size_t suffix = 0;
        while (usedGenericNames.count(candidate)) {
            candidate = name + "_" + std::to_string(++suffix);
        }
        usedGenericNames.insert(candidate);
        return candidate;
    }

    GenericType* GetOrCreateGenericType(GenericType* genTy, std::unordered_map<GenericType*, GenericType*>& genTyMap)
    {
        if (auto it = genTyMap.find(genTy); it != genTyMap.end()) {
            return it->second;
        }
        int n = ++extendGenericParamIndex;
        std::string internalId = extendGenericNamingBase + "2R" + std::to_string(n) + "E";
        std::string displayName = "T" + std::to_string(n);
        auto newGenTy = builder.GetType<GenericType>(internalId, displayName);
        genTyMap[genTy] = newGenTy;
        return newGenTy;
    }

    GenericType* MapGenericsToSame(GenericType* genTy1, GenericType* genTy2,
        std::unordered_map<GenericType*, GenericType*>& genTyMap)
    {
        auto it1 = genTyMap.find(genTy1);
        auto it2 = genTyMap.find(genTy2);

        GenericType* newGenTy = nullptr;
        if (it1 != genTyMap.end() && it2 != genTyMap.end()) {
            if (it1->second != it2->second) {
                return nullptr;  ///< Conflict: same genTyMap entry maps to different new generics.
            }
            newGenTy = it1->second;
        } else if (it1 != genTyMap.end()) {
            newGenTy = it1->second;
            genTyMap[genTy2] = newGenTy;
        } else if (it2 != genTyMap.end()) {
            newGenTy = it2->second;
            genTyMap[genTy1] = newGenTy;
        } else {
            newGenTy = GetOrCreateGenericType(genTy1, genTyMap);
            genTyMap[genTy2] = newGenTy;
        }
        return newGenTy;
    }

    bool ProcessTypeMatching(const std::vector<std::pair<Type*, Type*>>& typeArgPairs);

    /// Result of CreateSubstitutedTypes (Step 3).
    struct CreateSubstitutedTypesResult {
        ClassType* newClassTy;       ///< Substituted class type.
        ClassType* newParentTy;     ///< Substituted parent type.
        std::vector<ClassType*> newInterfaceTys;  ///< Substituted interface types.
        std::unordered_map<const GenericType*, Type*> fullSubstMap;   ///< Full eq2/subst map.
        std::unordered_map<const GenericType*, Type*> classSubstMap;  ///< Class generics -> new types.
    };
    std::optional<CreateSubstitutedTypesResult> CreateSubstitutedTypes(
        ClassType* classTy, ClassType* parentTy, const std::vector<ClassType*>& interfaceTys);

private:
    void BuildClassSubstMap(std::unordered_map<const GenericType*, Type*>& classSubstMap);
    void BuildSubstMap(std::unordered_map<const GenericType*, Type*>& substMap);
    CHIRBuilder& builder;
    bool MatchExtendedAndParentTypeArgs(Type* leftTy, Type* rightTy,
        std::unordered_map<GenericType*, GenericType*>& genTyMap);
    bool MatchConstructorConstructor(Type* leftTy, Type* rightTy,
        std::unordered_map<GenericType*, GenericType*>& genTyMap);
    /// Match when extended type has GENERIC and parent type has CONSTRUCTOR (e.g. A<R1> vs B<SSS<R2>>).
    /// Creates new generic types for the constructor's arguments and propagates constraints.
    /// @param extendedGenTy GENERIC in extended type.
    /// @param parentConstructor CONSTRUCTOR in parent type.
    /// @param genTyMap Map to update with new generics.
    /// @return True if match succeeded.
    bool MatchGenericParamToConstructor(GenericType* extendedGenTy, Type* parentConstructor,
                                        std::unordered_map<GenericType*, GenericType*>& genTyMap);
    /// Match when extended type has CONSTRUCTOR and parent has GENERIC (e.g. A<SSS<R1>> vs B<R2>).
    /// Maps the constructor's generic to the parent's generic and wraps it.
    /// @param extendedConstructor CONSTRUCTOR in extended type.
    /// @param parentGenTy GENERIC in parent type.
    /// @param genTyMap Map to update.
    /// @return True if match succeeded.
    bool MatchConstructorToGenericParam(Type* extendedConstructor, GenericType* parentGenTy,
                                        std::unordered_map<GenericType*, GenericType*>& genTyMap);
    void PropagateConstructorUpperBounds(ClassType* classTy,
        const std::vector<GenericType*>& constructorGenTys,
        const std::vector<GenericType*>& newGenTys);
    std::vector<Type*> CollectConstructorUpperBounds(Type* extendedConstructor, GenericType* newGenTy);
};

bool TypeMatchingImpl::ProcessTypeMatching(const std::vector<std::pair<Type*, Type*>>& typeArgPairs)
{
    for (auto& [extendedTypeArg, parentTypeArg] : typeArgPairs) {
        if (!MatchExtendedAndParentTypeArgs(extendedTypeArg, parentTypeArg, oldToNewGenericMap)) {
            return false;
        }
    }
    return true;
}

bool TypeMatchingImpl::MatchConstructorConstructor(Type* leftTy, Type* rightTy,
    std::unordered_map<GenericType*, GenericType*>& genTyMap)
{
    if (!IsSameConstructor(leftTy, rightTy)) {
        return false;
    }
    auto leftArgs = leftTy->GetTypeArgs();
    auto rightArgs = rightTy->GetTypeArgs();
    if (leftArgs.size() != rightArgs.size()) {
        return false;
    }
    for (size_t i = 0; i < leftArgs.size(); ++i) {
        if (!MatchExtendedAndParentTypeArgs(leftArgs[i], rightArgs[i], genTyMap)) {
            return false;
        }
    }
    return true;
}

bool TypeMatchingImpl::MatchExtendedAndParentTypeArgs(Type* leftTy, Type* rightTy,
    std::unordered_map<GenericType*, GenericType*>& genTyMap)
{
    auto leftCat = GetTypeCategory(leftTy);
    auto rightCat = GetTypeCategory(rightTy);
    if (leftCat == TypeCategory::GENERIC && rightCat == TypeCategory::GENERIC) {
        auto leftGenTy = StaticCast<GenericType>(leftTy);
        auto rightGenTy = StaticCast<GenericType>(rightTy);
        CJC_ASSERT(leftGenTy != rightGenTy);
        GenericType* newGenTy = MapGenericsToSame(leftGenTy, rightGenTy, genTyMap);
        if (!newGenTy) {
            return false;
        }
        eq2GenericToNewGenericMap[leftGenTy] = newGenTy;
        return true;
    }
    if (leftCat == TypeCategory::GENERIC && rightCat == TypeCategory::SIMPLE) {
        eq2GenericToNewTypeMap[StaticCast<GenericType>(leftTy)] = rightTy;
        return true;
    }
    if (leftCat == TypeCategory::GENERIC && rightCat == TypeCategory::CONSTRUCTOR) {
        return MatchGenericParamToConstructor(StaticCast<GenericType>(leftTy), rightTy, genTyMap);
    }
    if (leftCat == TypeCategory::SIMPLE && rightCat == TypeCategory::SIMPLE) {
        return IsSameConstructor(leftTy, rightTy);
    }
    if (leftCat == TypeCategory::SIMPLE) {
        return false;
    }
    if (leftCat == TypeCategory::CONSTRUCTOR && rightCat == TypeCategory::GENERIC) {
        return MatchConstructorToGenericParam(leftTy, StaticCast<GenericType>(rightTy), genTyMap);
    }
    if (leftCat == TypeCategory::CONSTRUCTOR && rightCat == TypeCategory::CONSTRUCTOR) {
        return MatchConstructorConstructor(leftTy, rightTy, genTyMap);
    }
    return false;
}

void TypeMatchingImpl::PropagateConstructorUpperBounds(ClassType* classTy,
    const std::vector<GenericType*>& constructorGenTys,
    const std::vector<GenericType*>& newGenTys)
{
    auto classDef = classTy->GetClassDef();
    if (!classDef) {
        return;
    }
    auto constructorGenParams = classDef->GetGenericTypeParams();
    if (constructorGenParams.size() != constructorGenTys.size()) {
        return;
    }
    for (size_t i = 0; i < constructorGenParams.size(); ++i) {
        auto constructorGenParam = constructorGenParams[i];
        auto newGenTy = newGenTys[i];
        auto upperBounds = constructorGenParam->GetUpperBounds();
        if (upperBounds.empty()) {
            continue;
        }
        std::unordered_map<const GenericType*, Type*> substMap;
        substMap[constructorGenParam] = newGenTy;
        std::vector<Type*> newUpperBounds;
        for (auto ub : upperBounds) {
            newUpperBounds.push_back(ReplaceRawGenericArgType(*ub, substMap, builder));
        }
        newGenTy->SetUpperBounds(newUpperBounds);
    }
}

bool TypeMatchingImpl::MatchGenericParamToConstructor(GenericType* extendedGenTy, Type* parentConstructor,
    std::unordered_map<GenericType*, GenericType*>& genTyMap)
{
    std::vector<GenericType*> constructorGenTys;
    CollectGenericTypes(parentConstructor, constructorGenTys);
    if (constructorGenTys.empty()) {
        eq2GenericToNewTypeMap[extendedGenTy] = parentConstructor;
        return true;
    }
    std::vector<GenericType*> newGenTys;
    for (auto genTy : constructorGenTys) {
        newGenTys.push_back(GetOrCreateGenericType(genTy, genTyMap));
    }
    if (auto classTy = DynamicCast<ClassType>(parentConstructor)) {
        PropagateConstructorUpperBounds(classTy, constructorGenTys, newGenTys);
    }
    std::unordered_map<const GenericType*, Type*> constructorSubst;
    for (size_t i = 0; i < constructorGenTys.size(); ++i) {
        constructorSubst[constructorGenTys[i]] = newGenTys[i];
    }
    Type* substitutedConstructor = ReplaceRawGenericArgType(*parentConstructor, constructorSubst, builder);
    eq2GenericToNewTypeMap[extendedGenTy] = substitutedConstructor;
    return true;
}

std::vector<Type*> TypeMatchingImpl::CollectConstructorUpperBounds(
    Type* extendedConstructor, GenericType* newGenTy)
{
    std::vector<Type*> constructorUpperBounds;
    auto classTy = DynamicCast<ClassType>(extendedConstructor);
    if (!classTy) {
        return constructorUpperBounds;
    }
    auto classDef = classTy->GetClassDef();
    if (!classDef) {
        return constructorUpperBounds;
    }
    auto constructorGenParams = classDef->GetGenericTypeParams();
    if (constructorGenParams.size() != 1) {
        return constructorUpperBounds;
    }
    auto constructorGenParam = constructorGenParams[0];
    auto upperBounds = constructorGenParam->GetUpperBounds();
    if (upperBounds.empty()) {
        return constructorUpperBounds;
    }
    std::unordered_map<const GenericType*, Type*> substMap;
    substMap[constructorGenParam] = newGenTy;
    for (auto ub : upperBounds) {
        constructorUpperBounds.push_back(ReplaceRawGenericArgType(*ub, substMap, builder));
    }
    return constructorUpperBounds;
}

bool TypeMatchingImpl::MatchConstructorToGenericParam(Type* extendedConstructor, GenericType* parentGenTy,
    std::unordered_map<GenericType*, GenericType*>& genTyMap)
{
    std::vector<GenericType*> constructorGenTys;
    CollectGenericTypes(extendedConstructor, constructorGenTys);
    if (constructorGenTys.size() != 1) {
        return false;
    }
    if (oldGenericToWrappedTypeMap.find(parentGenTy) != oldGenericToWrappedTypeMap.end()) {
        return false;
    }
    auto extendedGenTy = constructorGenTys[0];
    GenericType* newGenTy = MapGenericsToSame(extendedGenTy, parentGenTy, genTyMap);
    if (!newGenTy) {
        return false;
    }
    eq2GenericToNewGenericMap[extendedGenTy] = newGenTy;
    std::vector<Type*> combinedUpperBounds = CollectConstructorUpperBounds(extendedConstructor, newGenTy);
    for (auto ub : parentGenTy->GetUpperBounds()) {
        combinedUpperBounds.push_back(ub);
    }
    if (!combinedUpperBounds.empty()) {
        newGenTy->SetUpperBounds(combinedUpperBounds);
    }
    std::unordered_map<const GenericType*, Type*> constructorSubst;
    constructorSubst[extendedGenTy] = newGenTy;
    Type* wrappedType = ReplaceRawGenericArgType(*extendedConstructor, constructorSubst, builder);
    oldGenericToWrappedTypeMap[parentGenTy] = wrappedType;
    return true;
}

void TypeMatchingImpl::BuildClassSubstMap(std::unordered_map<const GenericType*, Type*>& classSubstMap)
{
    /// Map old generics (from parent class) to new generics.
    for (auto& [oldGenTy, newGenTy] : oldToNewGenericMap) {
        classSubstMap[oldGenTy] = newGenTy;
    }
    /// Map eq2 generics (from extended type) to new generics.
    for (auto& [eq2GenTy, newGenTy] : eq2GenericToNewGenericMap) {
        classSubstMap[eq2GenTy] = newGenTy;
    }
    /// Map eq2 generics (from extended type) to substituted concrete types.
    for (auto& [eq2GenTy, newType] : eq2GenericToNewTypeMap) {
        Type* substitutedType = ReplaceRawGenericArgType(*newType, classSubstMap, builder);
        classSubstMap[eq2GenTy] = substitutedType;
    }
}

void TypeMatchingImpl::BuildSubstMap(std::unordered_map<const GenericType*, Type*>& substMap)
{
    /// Map old generics (from parent class) to new generics, using wrapped types when available.
    for (auto& [oldGenTy, newGenTy] : oldToNewGenericMap) {
        if (auto it = oldGenericToWrappedTypeMap.find(oldGenTy); it != oldGenericToWrappedTypeMap.end()) {
            /// Use wrapped type to preserve constructor wrappers (e.g., Box<T>, RawArray<T>).
            std::unordered_map<const GenericType*, Type*> wrapSubst;
            for (auto& [eq2GenTy, newGenTy2] : eq2GenericToNewGenericMap) {
                wrapSubst[eq2GenTy] = newGenTy2;
            }
            Type* substitutedWrapped = ReplaceRawGenericArgType(*it->second, wrapSubst, builder);
            substMap[oldGenTy] = substitutedWrapped;
        } else {
            substMap[oldGenTy] = newGenTy;
        }
    }

    /// Map eq2 generics (from extended type) to new generics.
    for (auto& [eq2GenTy, newGenTy] : eq2GenericToNewGenericMap) {
        substMap[eq2GenTy] = newGenTy;
    }

    /// Map eq2 generics (from extended type) to substituted concrete types.
    for (auto& [eq2GenTy, newType] : eq2GenericToNewTypeMap) {
        Type* substitutedType = ReplaceRawGenericArgType(*newType, substMap, builder);
        substMap[eq2GenTy] = substitutedType;
    }
}

/// Step 3: create substituted class, parent, and interface types from classSubstMap/substMap.
/// @param classTy Class type to substitute.
/// @param parentTy Parent type to substitute.
/// @param interfaceTys Extend's interface types to substitute.
/// @return Result with newClassTy, newParentTy, newInterfaceTys, and subst maps; nullopt if newClassTy or
///     newParentTy is null.
std::optional<TypeMatchingImpl::CreateSubstitutedTypesResult> TypeMatchingImpl::CreateSubstitutedTypes(
    ClassType* classTy, ClassType* parentTy, const std::vector<ClassType*>& interfaceTys)
{
    std::unordered_map<const GenericType*, Type*> classSubstMap;
    BuildClassSubstMap(classSubstMap);
    std::unordered_map<const GenericType*, Type*> substMap;
    BuildSubstMap(substMap);
    ClassType* newClassTy = StaticCast<ClassType>(ReplaceRawGenericArgType(*classTy, classSubstMap, builder));
    ClassType* newParentTy = StaticCast<ClassType>(ReplaceRawGenericArgType(*parentTy, substMap, builder));
    if (!newClassTy || !newParentTy) {
        return std::nullopt;
    }
    std::vector<ClassType*> newInterfaceTys;
    for (auto interfaceTy : interfaceTys) {
        auto newInterfTy = StaticCast<ClassType>(ReplaceRawGenericArgType(*interfaceTy, substMap, builder));
        if (newInterfTy) {
            newInterfaceTys.push_back(newInterfTy);
        }
    }
    return CreateSubstitutedTypesResult{
        newClassTy, newParentTy, std::move(newInterfaceTys), substMap, classSubstMap};
}

/// Collect generic types from class and interface types (used for new extend's generic params).
/// @param newClassTy Substituted class type.
/// @param newInterfaceTys Substituted interface types.
/// @return List of generic types in order of first use.
std::vector<GenericType*> CollectGenericTypesFromTypes(
    const ClassType* newClassTy, const std::vector<ClassType*>& newInterfaceTys)
{
    std::set<GenericType*> usedNewGenTys;
    std::vector<GenericType*> newGenTysList;

    for (auto typeArg : newClassTy->GetTypeArgs()) {
        if (auto genTy = DynamicCast<GenericType>(typeArg)) {
            if (usedNewGenTys.find(genTy) == usedNewGenTys.end()) {
                usedNewGenTys.insert(genTy);
                newGenTysList.push_back(genTy);
            }
        } else {
            std::vector<GenericType*> genTysInArg;
            CollectGenericTypes(typeArg, genTysInArg);
            for (auto genTyInArg : genTysInArg) {
                if (usedNewGenTys.find(genTyInArg) == usedNewGenTys.end()) {
                    usedNewGenTys.insert(genTyInArg);
                    newGenTysList.push_back(genTyInArg);
                }
            }
        }
    }

    for (auto interfaceTy : newInterfaceTys) {
        std::vector<GenericType*> genTysInInterf;
        CollectGenericTypes(interfaceTy, genTysInInterf);
        for (auto genTyInInterf : genTysInInterf) {
            if (usedNewGenTys.find(genTyInInterf) == usedNewGenTys.end()) {
                usedNewGenTys.insert(genTyInInterf);
                newGenTysList.push_back(genTyInInterf);
            }
        }
    }

    return newGenTysList;
}

/// Build the extended type for the generated extend: use the parent's type at each position where
/// the class param was the sole filler (eq1[k] == class param).
/// @param typeArgPairs Eq2-to-eq1 type arg mapping from BuildEq2ToEq1TypeArgMapping.
/// @param newClassTy Substituted class type.
/// @param newParentTy Substituted parent type.
/// @param classDef The class we are generating the extend for.
/// @return Adjusted extended type, or nullptr on mismatch.
ClassType* BuildAdjustedExtendedType(
    const std::vector<std::pair<Type*, Type*>>& typeArgPairs,
    const ClassType& newClassTy, const ClassType& newParentTy,
    const ClassDef& classDef, CHIRBuilder& builder)
{
    auto classParams = classDef.GetGenericTypeParams();
    auto parentArgs = newParentTy.GetTypeArgs();
    auto classArgs = newClassTy.GetTypeArgs();
    if (classParams.size() != classArgs.size()) {
        return nullptr;
    }
    std::vector<Type*> adjustedArgs;
    adjustedArgs.reserve(classArgs.size());
    for (size_t j = 0; j < classParams.size(); ++j) {
        Type* argTy = classArgs[j];
        for (size_t k = 0; k < typeArgPairs.size() && k < parentArgs.size(); ++k) {
            if (typeArgPairs[k].second == classParams[j]) {
                argTy = parentArgs[k];
                break;
            }
        }
        adjustedArgs.push_back(argTy);
    }
    return builder.GetType<ClassType>(const_cast<ClassDef*>(&classDef), adjustedArgs);
}

bool TypeArgSatisfiesUpperBound(
    Type* typeArg, Type* substitutedUb, CHIRBuilder& builder)
{
    // only in this branch, in dev we need not strip refs
    typeArg = typeArg->StripAllRefs();
    substitutedUb = substitutedUb->StripAllRefs();
    if (auto classType = DynamicCast<ClassType>(typeArg)) {
        if (classType->IsEqualOrSubTypeOf(*substitutedUb, builder)) {
            return true;
        }
        for (auto superTy : classType->GetSuperTypesRecusively(builder)) {
            if (superTy->IsEqualOrSubTypeOf(*substitutedUb, builder)) {
                return true;
            }
        }
        return false;
    }
    if (typeArg->IsEqualOrSubTypeOf(*substitutedUb, builder)) {
        return true;
    }
    return false;
}

/// True iff ty is or contains the given generic (by pointer).
/// @param ty Type to check.
/// @param gen Generic to look for.
/// @return True if ty is or contains gen.
bool TypeContainsGeneric(const Type* ty, const GenericType* gen)
{
    if (ty == gen) {
        return true;
    }
    if (auto g = DynamicCast<GenericType>(ty)) {
        return g == gen;
    }
    for (auto arg : ty->GetTypeArgs()) {
        if (arg && TypeContainsGeneric(arg, gen)) {
            return true;
        }
    }
    return false;
}

/// Index in extendedType's type args where extendParam appears (as type arg or inside it).
/// @param extendedType Extended type from the extend.
/// @param extendParam Extend generic parameter to find.
/// @return Index, or extendedType.GetTypeArgs().size() if not found.
size_t FindExtendParamIndexInExtendedType(const ClassType& extendedType, const GenericType* extendParam)
{
    auto args = extendedType.GetTypeArgs();
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] && TypeContainsGeneric(args[i], extendParam)) {
            return i;
        }
    }
    return args.size();
}

/// LHS type for a constraint: from fullSubstMap, or from newParentTy at extend param index when applicable.
/// @param extendedClassTy Extended type from the extend def (or null).
/// @param newParentTy Substituted parent type.
/// @param extendParam The extend generic param whose constraint LHS we want.
/// @param fullSubstMap Eq2/substituted map.
/// @return LHS type, or nullptr if extendParam not in fullSubstMap.
Type* GetConstraintLeftType(const ClassType* extendedClassTy, const ClassType& newParentTy,
    const GenericType* extendParam, const std::unordered_map<const GenericType*, Type*>& fullSubstMap)
{
    auto it = fullSubstMap.find(extendParam);
    if (it == fullSubstMap.end()) {
        return nullptr;
    }
    Type* leftType = it->second;
    if (extendedClassTy && extendedClassTy->GetClassDef() == newParentTy.GetClassDef()) {
        size_t idx = FindExtendParamIndexInExtendedType(*extendedClassTy, extendParam);
        auto parentArgs = newParentTy.GetTypeArgs();
        if (idx < parentArgs.size() && parentArgs[idx]) {
            Type* typeAtPosition = parentArgs[idx];
            leftType = typeAtPosition;
        }
    }
    return leftType;
}

/// True iff class's generic bounds (after classSubstMap) imply leftType <: substitutedUb.
/// @param classDef Class whose generic params have bounds.
/// @param classSubstMap Class generics -> new types.
/// @param leftType LHS of constraint (generic).
/// @param substitutedUb RHS (upper bound) after substitution.
/// @return True if bounds imply the constraint.
bool ClassBoundsImplyConstraint(const ClassDef& classDef,
    const std::unordered_map<const GenericType*, Type*>& classSubstMap, Type* leftType,
    Type* substitutedUb, CHIRBuilder& builder)
{
    if (!DynamicCast<GenericType>(leftType)) {
        return false;
    }
    for (auto classParam : classDef.GetGenericTypeParams()) {
        auto classIt = classSubstMap.find(classParam);
        if (classIt == classSubstMap.end() || classIt->second != leftType) {
            continue;
        }
        auto classBounds = classParam->GetUpperBounds();
        if (classBounds.empty()) {
            break;
        }
        for (auto cb : classBounds) {
            Type* subClassBound = ReplaceRawGenericArgType(*cb, classSubstMap, builder);
            if (!subClassBound || !subClassBound->IsEqualOrSubTypeOf(*substitutedUb, builder)) {
                return false;
            }
        }
        return true;
    }
    return false;
}

/// Apply one constraint: add bound to generic or record extra pair.
/// @param leftType LHS of constraint.
/// @param substitutedUb RHS (upper bound) after substitution.
/// @param classDef Class whose generic params have bounds.
/// @param classSubstMap Class generics -> new types.
/// @return nullopt if constraint never satisfied; empty pair when bound added to generic;
///     (leftType, substitutedUb) when LHS is not a single generic (extra pair to add).
std::optional<std::pair<Type*, Type*>> ApplyOneConstraint(Type* leftType, Type* substitutedUb,
    const ClassDef& classDef, const std::unordered_map<const GenericType*, Type*>& classSubstMap,
    CHIRBuilder& builder)
{
    if (DynamicCast<GenericType>(leftType)) {
        auto leftGenTy = StaticCast<GenericType>(leftType);
        if (ClassBoundsImplyConstraint(classDef, classSubstMap, leftType, substitutedUb, builder)) {
            return std::make_pair(nullptr, nullptr);
        }
        auto existing = leftGenTy->GetUpperBounds();
        std::vector<Type*> newBounds(existing.begin(), existing.end());
        newBounds.push_back(substitutedUb);
        leftGenTy->SetUpperBounds(newBounds);
        return std::make_pair(nullptr, nullptr);
    }
    if (!TypeArgSatisfiesUpperBound(leftType, substitutedUb, builder)) {
        return std::nullopt;
    }
    return std::make_pair(leftType, substitutedUb);
}

/// Check that type args satisfy the def's generic upper bounds.
/// @param def Class or extend whose generic params have bounds.
/// @param typeTy Type to check (e.g. newParentTy or newClassTy).
/// @return True if constraints hold.
bool CheckTypeConstraints(
    const ClassDef& def, const ClassType& typeTy, CHIRBuilder& builder)
{
    auto genParams = def.GetGenericTypeParams();
    auto typeArgs = typeTy.GetTypeArgs();
    if (genParams.size() != typeArgs.size()) {
        return true;
    }
    for (size_t i = 0; i < genParams.size(); ++i) {
        auto genParam = genParams[i];
        auto typeArg = typeArgs[i];
        auto upperBounds = genParam->GetUpperBounds();
        if (upperBounds.empty()) {
            continue;
        }
        std::unordered_map<const GenericType*, Type*> substMap;
        substMap[genParam] = typeArg;
        for (auto ub : upperBounds) {
            Type* substitutedUb = ReplaceRawGenericArgType(*ub, substMap, builder);
            if (!TypeArgSatisfiesUpperBound(typeArg, substitutedUb, builder)) {
                return false;
            }
        }
    }
    return true;
}

/// Result of ApplyExtendConstraints.
struct ApplyExtendConstraintsResult {
    bool success;                                      ///< True if all constraints applied.
    std::vector<std::pair<Type*, Type*>> extraConstraintPairs;  ///< Extra LHS<:RHS pairs to add to extend.
};

/// Apply extend's generic constraints; add bounds or extra pairs as needed.
/// @param extendDef Source extend.
/// @param fullSubstMap Eq2/substituted map.
/// @param classSubstMap Class generics -> new types.
/// @param classDef Class we are generating extend for.
/// @param parentDef Parent class def (for constraint check), or null.
/// @param newClassTy Substituted class type (modified with new bounds if needed).
/// @param newParentTy Substituted parent type.
/// @return {false, {}} if any constraint never satisfied; else {true, extraConstraintPairs}.
ApplyExtendConstraintsResult ApplyExtendConstraints(
    const ExtendDef& extendDef,
    const std::unordered_map<const GenericType*, Type*>& fullSubstMap,
    const std::unordered_map<const GenericType*, Type*>& classSubstMap,
    const ClassDef& classDef, const ClassDef* parentDef,
    ClassType& newClassTy, const ClassType& newParentTy,
    CHIRBuilder& builder)
{
    if (parentDef) {
        if (!CheckTypeConstraints(*parentDef, newParentTy, builder)) {
            return {false, {}};
        }
    }
    if (!CheckTypeConstraints(classDef, newClassTy, builder)) {
        return {false, {}};
    }

    std::vector<std::pair<Type*, Type*>> extraConstraintPairs;
    const Type* extendedTypeRaw = extendDef.GetExtendedType();
    const ClassType* extendedClassTy =
        extendedTypeRaw ? DynamicCast<ClassType>(extendedTypeRaw) : nullptr;

    for (auto extendParam : extendDef.GetGenericTypeParams()) {
        auto upperBounds = extendParam->GetUpperBounds();
        if (upperBounds.empty()) {
            continue;
        }
        Type* leftType = GetConstraintLeftType(extendedClassTy, newParentTy, extendParam, fullSubstMap);
        if (!leftType) {
            continue;
        }
        for (auto ub : upperBounds) {
            Type* substitutedUb = ReplaceRawGenericArgType(*ub, fullSubstMap, builder);
            if (!substitutedUb) {
                continue;
            }
            auto opt = ApplyOneConstraint(leftType, substitutedUb, classDef, classSubstMap, builder);
            if (!opt) {
                return {false, {}};
            }
            if (opt->first) {
                extraConstraintPairs.push_back(*opt);
            }
        }
    }
    return {true, std::move(extraConstraintPairs)};
}

/// Return true if the class already has an extend that extends the same
/// type and implements the interface, so we do not add a duplicate.
/// Only considers extends on classDef (e.g. B), not other defs (e.g. A).
/// @param classDef Class we are generating the indirect extend for.
/// @param newClassTy Extended type of the candidate.
/// @param interfaceTy Interface to implement.
/// @return True if classDef already has an extend for this type and interface.
bool ExtendAlreadyExistsForTypeAndInterface(const ClassDef& classDef, const ClassType& newClassTy,
    const ClassType& interfaceTy, CHIRBuilder& builder)
{
    for (auto extendDef : classDef.GetExtends()) {
        Type* extType = extendDef->GetExtendedType();
        if (!extType || !newClassTy.IsEqualOrInstantiatedTypeOf(*extType, builder)) {
            continue;
        }
        for (auto implTy : extendDef->GetImplementedInterfaceTys()) {
            if (!implTy) {
                continue;
            }
            if (interfaceTy.IsEqualOrInstantiatedTypeOf(*implTy, builder) ||
                implTy->IsEqualOrInstantiatedTypeOf(interfaceTy, builder)) {
                return true;
            }
        }
    }
    return false;
}

/// Step 4: create the new extend declaration.
/// @param classDef Class we extend from.
/// @param newClassTy Substituted class type.
/// @param newInterfaceTys Interfaces the new extend implements.
/// @param extraConstraintPairs Optional where-clause pairs (can be null).
/// @return The created ExtendDef, or nullptr if an extend already exists for the same type and interface.
ExtendDef* CreateExtendDecl(
    CHIRBuilder& builder,
    const ClassDef& classDef, ClassType& classTy,
    const std::vector<ClassType*>& interfTys,
    std::vector<std::pair<Type*, Type*>>&& extraConstraintPairs)
{
    if (!interfTys.empty()) {
        if (ExtendAlreadyExistsForTypeAndInterface(classDef, classTy, *interfTys[0], builder)) {
            return nullptr;
        }
    }
    auto newGenTysList = CollectGenericTypesFromTypes(&classTy, interfTys);
    static int extendDeclCounter = 0;
    ClassDef* interfaceDef = interfTys.empty() ? nullptr : interfTys[0]->GetClassDef();
    std::string mangledName = BuildExtendId(classDef.GetPackageName(), classDef, *interfaceDef, ++extendDeclCounter);
    auto newExtend = builder.CreateExtend(
        DebugLocation(), mangledName, classDef.GetPackageName(), false, newGenTysList);

    newExtend->EnableAttr(Attribute::COMPILER_ADD);
    newExtend->SetExtendedType(classTy);
    for (auto interfaceTy : interfTys) {
        newExtend->AddImplementedInterfaceTy(*interfaceTy);
    }
    if (!extraConstraintPairs.empty()) {
        newExtend->SetExtraConstraintPairs(std::move(extraConstraintPairs));
    }
    return newExtend;
}

/// Interfaces that the parent's extend declares (and their parent interfaces), each with its substituted type.
/// BFS from extend's interface list so that direct interfaces appear first, then their parent interfaces, then
/// grandparents, etc., giving a consistent level-order result.
/// @param extendInfo Extend info (interface list from the extend).
/// @param newInterfaceTys Substituted types for extendInfo's interface types; same order as extendInfo.interfTys.
/// @return Pairs (interface ClassDef*, substituted ClassType*).
std::vector<std::pair<ClassDef*, ClassType*>> BuildExtendedInterfacesWithSubstitutedTypes(
    const ExtendInfo& extendInfo, const std::vector<ClassType*>& newInterfaceTys, CHIRBuilder& builder)
{
    std::set<ClassDef*> seen;
    std::vector<std::pair<ClassDef*, ClassType*>> result;
    std::deque<std::pair<ClassDef*, ClassType*>> worklist;
    for (size_t i = 0; i < extendInfo.interfTys.size() && i < newInterfaceTys.size(); ++i) {
        ClassDef* interfDef = extendInfo.interfTys[i]->GetClassDef();
        if (!interfDef || !interfDef->IsInterface()) {
            continue;
        }
        if (seen.insert(interfDef).second) {
            result.emplace_back(interfDef, newInterfaceTys[i]);
            worklist.emplace_back(interfDef, newInterfaceTys[i]);
        }
    }
    while (!worklist.empty()) {
        auto [curDef, curNewTy] = worklist.front();
        worklist.pop_front();
        auto replaceTable = GetInstMapFromCurDefToCurType(*curNewTy);
        for (auto pTy : curDef->GetImplementedInterfaceTys()) {
            if (!pTy || !pTy->GetCustomTypeDef()) {
                continue;
            }
            ClassDef* pDef = StaticCast<ClassDef>(pTy->GetCustomTypeDef());
            if (!pDef->IsInterface() || !seen.insert(pDef).second) {
                continue;
            }
            Type* newPTy = ReplaceRawGenericArgType(*pTy, replaceTable, builder);
            if (newPTy) {
                auto newPClassTy = StaticCast<ClassType>(newPTy);
                result.emplace_back(pDef, newPClassTy);
                worklist.emplace_back(pDef, newPClassTy);
            }
        }
    }
    return result;
}

/// Result of PrepareIndirectExtendForClass (Step 1–3 + constraints).
struct PreparedIndirectExtend {
    std::vector<ClassType*> newInterfaceTys;           ///< Substituted interface types.
    ClassType* extendedTyForExtend;                     ///< Extended type for the new extend decl.
    std::vector<std::pair<Type*, Type*>> extraConstraintPairs;  ///< Extra where-clause pairs.
};

/// Step 1–3 + constraints: type matching, substituted types, extended type, constraints.
/// @param classDef Class we add indirect extend for.
/// @param extendInfo Parent's extend.
/// @param baseClassTy Generic parent type (any generic parent is considered).
/// @param classTy Class type (for substitution).
/// @return Prepared result, or nullopt on failure.
std::optional<PreparedIndirectExtend> PrepareIndirectExtendForClass(
    CHIRBuilder& builder, const ClassDef& classDef, const ExtendInfo& extendInfo, const ClassType& baseClassTy,
    ClassType* classTy)
{
    auto typeArgPairs = BuildEq2ToEq1TypeArgMapping(baseClassTy, *extendInfo.clTy);
    if (typeArgPairs.empty()) {
        return std::nullopt;
    }
    TypeMatchingImpl matchingImpl(builder, classDef.GetPackageName(), classDef);
    if (!matchingImpl.ProcessTypeMatching(typeArgPairs)) {
        return std::nullopt;
    }
    auto substOpt = matchingImpl.CreateSubstitutedTypes(classTy, const_cast<ClassType*>(&baseClassTy),
        extendInfo.interfTys);
    if (!substOpt) {
        return std::nullopt;
    }
    ClassType* extendedTyForExtend =
        BuildAdjustedExtendedType(typeArgPairs, *substOpt->newClassTy, *substOpt->newParentTy, classDef, builder);
    if (!extendedTyForExtend) {
        extendedTyForExtend = substOpt->newClassTy;
    }
    const ClassDef* parentDef = baseClassTy.GetClassDef();
    auto constraintsResult = ApplyExtendConstraints(*extendInfo.extendDef, substOpt->fullSubstMap,
        substOpt->classSubstMap, classDef, parentDef, *substOpt->newClassTy, *substOpt->newParentTy, builder);
    if (!constraintsResult.success) {
        return std::nullopt;
    }
    return PreparedIndirectExtend{
        std::move(substOpt->newInterfaceTys), extendedTyForExtend,
        std::move(constraintsResult.extraConstraintPairs)};
}

/// For each (interface, substituted type) from the parent's extend, create an extend decl (old logic).
/// class X with generic parent Z that has extend Z <: Y gets extend X <: Y with substituted types.
/// @param extendedInterfacesWithSubstitutedTypes From BuildExtendedInterfacesWithSubstitutedTypes.
/// @param classDef The class definition (non-const so we can AddExtend).
/// @param extendedTyForExtend Type used as the extended type for new extend decls.
/// @param extraConstraintPairs Additional type constraints to apply.
/// @return Newly created ExtendDefs.
std::vector<ExtendDef*> CreateExtendDeclsForInterfaces(
    const std::vector<std::pair<ClassDef*, ClassType*>>& extendedInterfacesWithSubstitutedTypes,
    ClassDef& classDef, ClassType& extendedTyForExtend,
    std::vector<std::pair<Type*, Type*>>&& extraConstraintPairs, CHIRBuilder& builder)
{
    std::vector<ExtendDef*> newDefs;
    for (const auto& [interfDef, substitutedInterfTy] : extendedInterfacesWithSubstitutedTypes) {
        ExtendDef* e = CreateExtendDecl(builder, classDef, extendedTyForExtend,
            {substitutedInterfTy}, std::move(extraConstraintPairs));
        if (e) {
            // Attach to the subclass (classDef) so GetFuncIndexInVTable can match via def's
            // extends: replaceTable is built from extend's extended type (A<SSS<R1>,SSS<R2>>)
            // and this type's args (e.g. B<SSS<Int32>,SSS<Int64>>), giving R1->Int32, R2->Int64.
            // Attaching to the extended type (A) would key the vtable as I<B1,SSS<B2>>, which
            // substitutes to I<SSS<Int32>,SSS<Int64>> and does not match I<Int32,SSS<Int64>>.
            classDef.AddExtend(*e);
            newDefs.push_back(e);
        }
    }
    return newDefs;
}

/// For one class and one parent extend: prepare and create all triggered indirect extend decls.
/// @param classDef Class to add indirect extend for (non-const so we can add extends to it).
/// @param extendInfo Parent's extend declaration info.
/// @param baseClassTy Generic parent type (any generic parent is considered).
/// @return List of newly created ExtendDefs.
std::vector<ExtendDef*> AddIndirectExtendForClass(
    CHIRBuilder& builder, ClassDef& classDef, const ExtendInfo& extendInfo, const ClassType& baseClassTy)
{
    auto classTy = StaticCast<ClassType>(classDef.GetType());
    if (!classTy) {
        return {};
    }
    auto extendedTy = extendInfo.clTy;
    if (!extendedTy || baseClassTy.GetClassDef() != extendedTy->GetClassDef()) {
        return {};
    }
    const ClassDef* zDef = baseClassTy.GetClassDef();
    if (!zDef || zDef->GetGenericTypeParams().empty()) {
        return {};
    }
    auto prep = PrepareIndirectExtendForClass(builder, classDef, extendInfo, baseClassTy, classTy);
    if (!prep) {
        return {};
    }
    std::vector<std::pair<ClassDef*, ClassType*>> extendedInterfacesWithSubstitutedTypes =
        BuildExtendedInterfacesWithSubstitutedTypes(extendInfo, prep->newInterfaceTys, builder);
    return CreateExtendDeclsForInterfaces(extendedInterfacesWithSubstitutedTypes, classDef,
        *prep->extendedTyForExtend, std::move(prep->extraConstraintPairs), builder);
}

/// Map from the class/interface that is extended (the "parent" in extend X) to all extend decls that extend it.
using ParentExtendMap = std::unordered_map<ClassDef*, std::vector<ExtendInfo>>;

/// Builds a map: for each interface/abstract class X, the list of ExtendInfo for every extend in pkg that extends X.
/// @return Map from extended ClassDef* (VIRTUAL/ABSTRACT only) to vector of ExtendInfo.
ParentExtendMap BuildParentExtendMap(const Package& pkg)
{
    ParentExtendMap parentExtendMap;
    for (auto extendDef : pkg.GetExtends()) {
        auto extendedType = extendDef->GetExtendedType();
        if (!extendedType || !extendedType->IsCustomType()) {
            continue;
        }
        auto customType = StaticCast<CustomType>(extendedType);
        auto extendedDef = DynamicCast<ClassDef>(customType->GetCustomTypeDef());
        if (!extendedDef ||
            (!extendedDef->TestAttr(Attribute::VIRTUAL) && !extendedDef->TestAttr(Attribute::ABSTRACT))) {
            continue;
        }
        std::vector<GenericType*> extendParams = extendDef->GetGenericTypeParams();
        std::vector<ClassType*> interfTys = extendDef->GetImplementedInterfaceTys();
        parentExtendMap[extendedDef].push_back(
            {std::move(extendParams), StaticCast<ClassType>(extendedType), std::move(interfTys), extendDef});
    }
    return parentExtendMap;
}

/// For each class, add indirect extend decls for all parent extends that apply.
/// @param parentExtendMap Map from extended class/interface to its extend infos (from BuildParentExtendMap).
/// @return All newly created ExtendDefs.
std::vector<ExtendDef*> ProcessClassesForIndirectExtend(
    const ParentExtendMap& parentExtendMap, Package& pkg, CHIRBuilder& builder)
{
    std::vector<ExtendDef*> allNewDefs;
    for (auto classDef : pkg.GetClasses()) {
        auto classTy = StaticCast<ClassType>(classDef->GetType());
        if (!classTy) {
            continue;
        }
        for (auto baseClassTy : classTy->GetSuperTypesRecusively(builder)) {
            auto parentDef = baseClassTy->GetClassDef();
            if (!parentDef) {
                continue;
            }
            auto it = parentExtendMap.find(parentDef);
            if (it == parentExtendMap.end()) {
                continue;
            }
            for (auto& extendInfo : it->second) {
                auto defs = AddIndirectExtendForClass(builder, *classDef, extendInfo, *baseClassTy);
                for (auto d : defs) {
                    allNewDefs.push_back(d);
                }
            }
        }
    }
    return allNewDefs;
}
} // namespace

/// Build parent-extend map and create all indirect extend decls for the package.
/// @return All newly created ExtendDefs.
std::vector<ExtendDef*> AddIndirectExtend(Package& pkg, CHIRBuilder& builder)
{
    ParentExtendMap parentExtendMap = BuildParentExtendMap(pkg);
    return ProcessClassesForIndirectExtend(parentExtendMap, pkg, builder);
}

} // namespace Cangjie::CHIR
