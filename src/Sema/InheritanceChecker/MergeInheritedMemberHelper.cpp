// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements how to merge inherited parent member to child member.
 */
#include "StructInheritanceChecker.h"

#include "Promotion.h"
#include "TypeCheckUtil.h"

using namespace Cangjie;
using namespace AST;
using namespace TypeCheckUtil;

namespace {
void UpdateInconsistentTypes(
    MemberSignature& member, const MemberSignature& parent, const std::vector<ModalTy>& inconsistentTypes)
{
    member.inconsistentTypes.insert(parent.inconsistentTypes.begin(), parent.inconsistentTypes.end());
    member.inconsistentTypes.insert(inconsistentTypes.begin(), inconsistentTypes.end());
}

std::vector<std::unordered_set<DataTy>> UpdateUpperBoundsSet(
    TypeManager& tyMgr, const std::vector<std::unordered_set<DataTy>>& upperBounds, const TypeSubst& typeMapping)
{
    if (typeMapping.empty() ||
        std::all_of(upperBounds.begin(), upperBounds.end(), [](auto it) { return it.empty(); })) {
        // If typeMapping is empty or 'upperBounds' is empty or
        // All elements in 'upperBounds' are empty, quick quit.
        return upperBounds;
    }
    std::vector<std::unordered_set<DataTy>> allUpperBounds;
    for (const auto& uppers : std::as_const(upperBounds)) {
        std::unordered_set<DataTy> newUppers;
        for (auto it : uppers) {
            newUppers.emplace(tyMgr.GetInstantiatedTy(it, typeMapping));
        }
        allUpperBounds.emplace_back(newUppers);
    }
    return allUpperBounds;
}

// Caller guarantees 'src' and 'other' are both generic function.
void MergeUpperBounds(TypeManager& tyMgr, MemberSignature& src, const MemberSignature& other, bool shouldMerge)
{
    bool ignored = !shouldMerge || src.decl == other.decl || src.upperBounds.empty() ||
        src.upperBounds.size() != other.upperBounds.size();
    if (ignored) {
        return;
    }
    TypeSubst typeMapping = tyMgr.GenerateGenericMappingFromGeneric(
        *RawStaticCast<FuncDecl*>(other.decl), *RawStaticCast<FuncDecl*>(src.decl));
    for (size_t i = 0; i < other.upperBounds.size(); ++i) {
        for (auto upper : other.upperBounds[i]) {
            src.upperBounds[i].emplace(tyMgr.GetInstantiatedTy(upper, typeMapping));
        }
    }
}

void SetPossibleInconsistentType(
    const MemberSignature& target, const MemberSignature& src, std::vector<ModalTy>& inconsistentTypes)
{
    auto getTy = [](const MemberSignature& member) -> ModalTy {
        if (auto ty = DynamicCast<FuncTy>(member.ty)) {
            return ty->retTy;
        }
        return member.ty;
    };
    inconsistentTypes.emplace_back(getTy(target));
    inconsistentTypes.emplace_back(getTy(src));
}
} // namespace

/**
 * Compute whether 'parent' and 'child' contains inconsistent return type. If so, update 'inconsistentTypes'
 * Update 'updated' result's type if 'parent' and 'child' are both from inherited interfaces.
 * and return type of 'parent' is subtype of return type of 'child'.
 * @p status means {parent and child have same signature, parent and child are both from inherited interfaces}.
 */
bool StructInheritanceChecker::ComputeInconsistentTypes(const MemberSignature& child, const MemberSignature& parent,
    MemberSignature& updated, const std::pair<bool, bool>& status, std::vector<ModalTy>& inconsistentTypes) const
{
    auto parentTy = DynamicCast<FuncTy>(parent.ty);
    auto childTy = DynamicCast<FuncTy>(child.ty);
    if (!Ty::IsTyCorrect(parentTy) || !Ty::IsTyCorrect(childTy)) {
        return true;
    }
    bool hasConsistentReturnTy = true;
    if (status.first) {
        // If child's ret type is subtype of parent, and parent is abstract or both not abstract, return type is valid.
        bool isValidOverride = parent.decl->TestAttr(Attribute::ABSTRACT) || !child.decl->TestAttr(Attribute::ABSTRACT);
        hasConsistentReturnTy = isValidOverride && AreReturnTypesCompatible(*parentTy, *childTy);
        // If two decls are both interface members that are inherited from current type
        // and parent's return type is subtype of child, using parentTy.
        // The overridden return type is consistent.
        bool bothInterfaceMember = status.second && child.decl->outerDecl->astKind == ASTKind::INTERFACE_DECL &&
            parent.decl->outerDecl->astKind == ASTKind::INTERFACE_DECL;
        bool needUpdateTy = bothInterfaceMember && AreReturnTypesCompatible(*childTy, *parentTy);
        hasConsistentReturnTy = hasConsistentReturnTy || needUpdateTy;
        if (needUpdateTy) {
            updated.ty = parentTy;
        }
    }

    if (!hasConsistentReturnTy) {
        SetPossibleInconsistentType(parent, child, inconsistentTypes);
    }
    return hasConsistentReturnTy;
}

void StructInheritanceChecker::ComputeInconsistentPropTypes(const MemberSignature& child, const MemberSignature& parent,
    std::vector<ModalTy>& inconsistentTypes) const
{
    if (parent.thisMode != child.thisMode) {
        return;
    }
    auto parentTy = parent.ty;
    auto childTy = child.ty;
    if (!Ty::IsTyCorrect(parentTy) || !Ty::IsTyCorrect(childTy)) {
        return;
    }
    if (!typeManager.IsTyEqual(parentTy, childTy)) {
        inconsistentTypes.emplace_back(parent.ty, parent.thisMode);
        inconsistentTypes.emplace_back(child.ty, child.thisMode);
    }
}

void StructInheritanceChecker::UpdateOverriddenFuncDeclCache(Ptr<Decl> child, Ptr<Decl> parent)
{
    if (!child || !parent) {
        return;
    }
    // Record when the overriding member belongs to the type currently under check.
    bool isMemberOfCheckingType = !checkingDecls.empty() && child->outerDecl == checkingDecls.back();
    // Also record interface-to-interface overrides (e.g. Equatable.!= overrides NotEqual.!=)
    // when the child interface itself is being checked as an inherited base: in that case
    // checkingDecls.back() is still the child interface, so the branch above already covers
    // it. Keep an explicit interface fallback for robustness when checkingDecls is empty.
    bool isInterfaceOverride = child->outerDecl && parent->outerDecl &&
        child->outerDecl->astKind == ASTKind::INTERFACE_DECL &&
        parent->outerDecl->astKind == ASTKind::INTERFACE_DECL;
    if (isMemberOfCheckingType || (checkingDecls.empty() && isInterfaceOverride)) {
        typeManager.UpdateTopOverriddenFuncDeclMap(child, parent);
    }
}

/**
 * Update @p child value into the @p members
 * NOTE: Do not report error here.
 * @param inheritedInterfaces Indicates parent and child are members of interfaces that inherited by same type.
 *                            This will also be true if parent and child's parent types are from generic upper bounds.
 */
MemberSignature StructInheritanceChecker::UpdateInheritedMemberIfNeeded(
    MemberMap& inheritedMembers, const MemberSignature& child, bool inheritedInterfaces)
{
    MemberSignature updated = child;
    bool needImplement = child.decl->outerDecl->astKind == ASTKind::INTERFACE_DECL || inheritedInterfaces;
    auto foundMembers = inheritedMembers.equal_range(child.decl->identifier);
    std::vector<ModalTy> inconsistentTypes;
    for (auto it = foundMembers.first; it != foundMembers.second; ++it) {
        auto parent = it->second;
        if (parent.decl->astKind != child.decl->astKind) {
            continue;
        }
        bool shouldUpdate = true;
        // Compare after promoting onto the most derived type so generic diamond paths are not false conflicts.
        bool inconsistentType = needImplement && (parent.thisMode != child.thisMode ||
            !typeManager.IsTyEqual(PromoteMemberTyToCheckingDecl(parent), PromoteMemberTyToCheckingDecl(child)));
        if (parent.decl->IsFunc()) {
            if (child.decl->TestAttr(Attribute::GENERIC) && parent.decl->TestAttr(Attribute::GENERIC)) {
                TypeSubst typeMapping = typeManager.GenerateGenericMappingFromGeneric(
                    *RawStaticCast<FuncDecl*>(parent.decl), *RawStaticCast<FuncDecl*>(child.decl));
                parent.ty = typeManager.GetInstantiatedTy(parent.ty, typeMapping);
            }
            auto parentFuncTy = DynamicCast<FuncTy>(parent.ty);
            auto childFuncTy = DynamicCast<FuncTy>(child.ty);
            bool sameStatus = parent.decl->TestAttr(Attribute::STATIC) == child.decl->TestAttr(Attribute::STATIC) &&
                child.decl->TestAttr(Attribute::GENERIC) == parent.decl->TestAttr(Attribute::GENERIC);
            shouldUpdate = sameStatus && parentFuncTy && childFuncTy &&
                typeManager.IsFuncParameterTypesIdentical(*parentFuncTy, *childFuncTy) &&
                parent.thisMode == child.thisMode;
            auto consistent = ComputeInconsistentTypes(
                child, parent, updated, {shouldUpdate, inheritedInterfaces}, inconsistentTypes);
            inconsistentType = inconsistentType && !consistent;
        } else if (parent.decl->astKind == ASTKind::PROP_DECL) {
            ComputeInconsistentPropTypes(child, parent, inconsistentTypes);
            shouldUpdate = parent.ty == child.ty && parent.thisMode == child.thisMode;
            if (!inconsistentTypes.empty()) {
                UpdateInconsistentTypes(it->second, parent, inconsistentTypes);
            }
        } else if (inconsistentType) {
            SetPossibleInconsistentType(parent, child, inconsistentTypes);
        }

        CheckNativeFFI(parent, child);

        if (shouldUpdate) {
            if (parent.decl->TestAttr(Attribute::ABSTRACT) || !child.decl->TestAttr(Attribute::ABSTRACT)) {
                UpdateOverriddenFuncDeclCache(child.decl, parent.decl);
                it->second = updated; // Override stored value to child.
            }
            it->second.replaceOther = true;
            if (needImplement && (!parent.inconsistentTypes.empty() || inconsistentType)) {
                UpdateInconsistentTypes(it->second, parent, inconsistentTypes);
            }
            MergeUpperBounds(typeManager, it->second, parent, inheritedInterfaces);
            MergeUpperBounds(typeManager, it->second, child, inheritedInterfaces);
            if (!parent.extendDecl || !child.extendDecl) {
                it->second.extendDecl = nullptr;
            }
            it->second.isInheritedInterface = child.isInheritedInterface && inheritedInterfaces; // Update status.
            // Dual default members: require an explicit impl only when they are distinct defaults that still
            // share a signature after promotion onto the most derived type. The same default inherited via
            // multiple paths (diamond: C <: B & A where B <: A) is not a conflict. Mismatched promoted types
            // are handled by normal overload / inconsistent-type checking above.
            bool isRelationExtend = true;
            if (!checkingDecls.empty() && checkingDecls[0]->astKind == ASTKind::EXTEND_DECL && child.extendDecl) {
                auto curExtend = StaticCast<ExtendDecl*>(checkingDecls[0].get());
                isRelationExtend = typeManager.IsExtendInheritRelation(*curExtend, *child.extendDecl).first;
            }
            bool dualDefaultContext = it->second.isInheritedInterface || !isRelationExtend;
            it->second.shouldBeImplemented = NeedReimplementMergedDefaults(parent, child, dualDefaultContext);
            return it->second;
        }
    }
    inheritedMembers.emplace(child.decl->identifier, updated);
    return updated;
}

DataTy StructInheritanceChecker::PromoteMemberTyToCheckingDecl(const MemberSignature& member) const
{
    if (!Ty::IsTyCorrect(member.ty) || !member.decl || !member.decl->outerDecl) {
        return member.ty;
    }
    auto outerTy = member.decl->outerDecl->DataTy();
    if (!Ty::IsTyCorrect(outerTy)) {
        return member.ty;
    }
    // Inheritance-path interface type keeps diamond instantiations (A<Int> vs A<String>) distinct.
    DataTy pathTy = Ty::IsTyCorrect(member.structTy) ? member.structTy : outerTy;
    auto pathSubst = GenerateTypeMappingByTy(outerTy, pathTy);
    DataTy pathInstTy =
        pathSubst.empty() ? member.decl->DataTy() : typeManager.GetInstantiatedTy(member.decl->DataTy(), pathSubst);
    if (checkingDecls.empty() || !Ty::IsTyCorrect(checkingDecls.back()->DataTy())) {
        return Ty::IsTyCorrect(pathInstTy) ? pathInstTy : member.ty;
    }
    auto derivedTy = checkingDecls.back()->DataTy();
    auto mts = Promotion(typeManager).GetPromoteTypeMapping(derivedTy, pathTy);
    if (mts.empty()) {
        return Ty::IsTyCorrect(pathInstTy) ? pathInstTy : member.ty;
    }
    auto instTys = typeManager.GetInstantiatedTys(pathInstTy, mts);
    if (instTys.size() == 1) {
        return *instTys.begin();
    }
    // Ambiguous promotion set: keep the path-resolved type already carried by the member.
    return member.ty;
}

bool StructInheritanceChecker::NeedReimplementMergedDefaults(
    const MemberSignature& parent, const MemberSignature& child, bool dualDefaultContext) const
{
    if (!dualDefaultContext || !parent.decl || !child.decl) {
        return false;
    }
    if (!parent.decl->TestAttr(Attribute::DEFAULT) || !child.decl->TestAttr(Attribute::DEFAULT)) {
        return false;
    }
    // Same underlying default (diamond inheritance): never require a re-implementation here.
    // Distinct defaults with the same promoted signature still must be implemented explicitly.
    if (parent.decl == child.decl || parent.thisMode != child.thisMode) {
        return false;
    }
    return typeManager.IsTyEqual(PromoteMemberTyToCheckingDecl(parent), PromoteMemberTyToCheckingDecl(child));
}

/**
 * Merge @p otherMembers to @p members
 * If member of the otherMembers implements member in the 'members', replace the member in 'members'.
 * NOTE: Do not report error here.
 */
void StructInheritanceChecker::MergeInheritedMembers(
    MemberMap& members, const MemberMap& otherMembers, DataTy structTy, bool inheritedInterfaces)
{
    MultiTypeSubst mts;
    typeManager.GenerateGenericMapping(mts, *structTy);
    auto typeMapping = MultiTypeSubstToTypeSubst(mts);
    for (auto& member : otherMembers) {
        auto memberSig = member.second;
        if (inheritedInterfaces) {
            memberSig.isInheritedInterface = true;
        }
        memberSig.ty = typeManager.GetInstantiatedTy(memberSig.ty, typeMapping);
        memberSig.structTy = typeManager.GetInstantiatedTy(memberSig.structTy, typeMapping);
        memberSig.upperBounds = UpdateUpperBoundsSet(typeManager, memberSig.upperBounds, typeMapping);
        UpdateInheritedMemberIfNeeded(members, memberSig, inheritedInterfaces);
    }
}
