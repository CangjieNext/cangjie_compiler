// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements the OverrideFunctionResolver class for resolving and managing overridden member functions.
 */

#include "OverrideFunctionResolver.h"

#include "Promotion.h"
#include "TypeCheckUtil.h"
#include "cangjie/Sema/CommonTypeAlias.h"
#include "cangjie/Utils/Utils.h"

using namespace Cangjie;
using namespace AST;

// Initialize static cache
std::unordered_map<std::pair<Ptr<AST::Ty>, std::string>, MemberFuncsWithInstTys, HashPair>
    OverrideFunctionResolver::instTy2Members;

MemberFuncsWithInstTys OverrideFunctionResolver::GetInstMemberFuncWithInstTy(
    AST::Ty& instBaseTy, const std::string& identifier)
{
    CJC_NULLPTR_CHECK(typeManager);
    auto key = std::make_pair(&instBaseTy, identifier);
    if (auto found = instTy2Members.find(key); found != instTy2Members.end()) {
        return found->second;
    }
    MemberFuncsWithInstTys funcs;
    auto baseDecl = AST::Ty::GetDeclPtrOfTy<AST::InheritableDecl>(&instBaseTy);
    std::set<Ptr<AST::ExtendDecl>> extendDecls;
    if (baseDecl) {
        MemberFuncsWithInstTys superInterfaceFuncs;
        GetInstMemberFromSuper(instBaseTy, baseDecl, superInterfaceFuncs, identifier, true);
        GetInstMemberFromSuper(instBaseTy, baseDecl, funcs, identifier, false);
        MergeIntoFuncs(funcs, superInterfaceFuncs);
        CollectDeclMemberFunc(*baseDecl, instBaseTy, funcs, identifier);
        extendDecls = typeManager->GetDeclExtends(*baseDecl);
    } else {
        extendDecls = typeManager->GetAllExtendsByTy(instBaseTy);
    }

    MemberFuncsWithInstTys allExtendFuncs;
    for (auto& ed : extendDecls) {
        if (!ed || ed->TestAttr(AST::Attribute::GENERIC_INSTANTIATED)) {
            continue;
        }
        MemberFuncsWithInstTys extendFuncs;
        GetInstMemberFromSuper(instBaseTy, ed, extendFuncs, identifier, true);
        // Select the subclass version in the case of multiple default implementations.
        MergeExtendSuperMember(instBaseTy, allExtendFuncs, extendFuncs);
        // Process extend's own member functions in the same loop
        CollectDeclMemberFunc(*ed, instBaseTy, allExtendFuncs, identifier);
    }
    MergeIntoFuncs(funcs, allExtendFuncs);
    instTy2Members.emplace(key, funcs);
    return funcs;
}

void OverrideFunctionResolver::CollectDeclMemberFunc(
    AST::Decl& decl, AST::Ty& instBaseTy, MemberFuncsWithInstTys& funcs, const std::string& identifier)
{
    CJC_NULLPTR_CHECK(typeManager);
    auto mapping = TypeCheckUtil::GenerateTypeMappingByTy(decl.ty, &instBaseTy);
    for (auto& member : decl.GetMemberDecls()) {
        TypeCheckUtil::WorkForMembers(*member, [this, &funcs, &mapping, &identifier](auto& m) {
            if (m.identifier != identifier || m.astKind != AST::ASTKind::FUNC_DECL ||
                m.TestAttr(AST::Attribute::ABSTRACT)) {
                return;
            }
            auto memberFunc = StaticCast<AST::FuncDecl>(&m);
            auto instMemberTy = typeManager->GetInstantiatedTy(memberFunc->ty, mapping);

            for (auto superFuncIt = funcs.begin(); superFuncIt != funcs.end();) {
                for (auto superFuncInstTyIt = superFuncIt->second.begin();
                    superFuncInstTyIt != superFuncIt->second.end();) {
                    if (IsImplementationFunc(*memberFunc, *superFuncIt->first, instMemberTy, *superFuncInstTyIt)) {
                        superFuncInstTyIt = superFuncIt->second.erase(superFuncInstTyIt);
                    } else {
                        ++superFuncInstTyIt;
                    }
                }
                if (superFuncIt->second.empty()) {
                    superFuncIt = funcs.erase(superFuncIt);
                } else {
                    ++superFuncIt;
                }
            }

            auto funcIt = funcs.find(memberFunc);
            if (funcIt != funcs.end()) {
                funcIt->second.emplace(instMemberTy);
            } else {
                funcs.emplace(std::make_pair(memberFunc, std::unordered_set<Ptr<AST::Ty>>{instMemberTy}));
            }
        });
    }
}

void OverrideFunctionResolver::GetInstMemberFromSuper(AST::Ty& instBaseTy, Ptr<AST::InheritableDecl> baseDecl,
    MemberFuncsWithInstTys& funcs, const std::string& identifier, bool isCheckingInterface)
{
    CJC_NULLPTR_CHECK(typeManager);
    for (auto& type : baseDecl->inheritedTypes) {
        bool notSkipSuper = isCheckingInterface ? type->ty->IsInterface() : type->ty->IsClass();
        if (!notSkipSuper) {
            continue;
        }
        auto superFuncs = GetInstMemberFuncWithInstTy(*type->ty, identifier);
        MergeIntoFuncs(funcs, superFuncs);
    }
    auto mappingBasePtrDecl2InstTy = TypeCheckUtil::GenerateTypeMappingByTy(baseDecl->ty, &instBaseTy);
    for (auto& superFunc : funcs) {
        std::unordered_set<Ptr<AST::Ty>> newTySet;
        for (auto& superFuncInstTy : superFunc.second) {
            newTySet.emplace(typeManager->GetInstantiatedTy(superFuncInstTy, mappingBasePtrDecl2InstTy));
        }
        superFunc.second = std::move(newTySet);
    }
}

bool OverrideFunctionResolver::HasImplementationWithSubTypeCheck(AST::Ty& instBaseTy,
    const MemberFuncsWithInstTys& funcs, Ptr<AST::Ty> newFuncInstTy,
    const std::set<Ptr<AST::Ty>>& newFuncOuterDeclInstTys)
{
    CJC_NULLPTR_CHECK(typeManager);

    for (const auto& func : funcs) {
        auto funcOuterDeclInstTys = Promotion(*typeManager).Promote(instBaseTy, *func.first->outerDecl->ty);

        // Check if the new function instance type is already in the current function's instance types
        if (!Utils::In(newFuncInstTy, func.second)) {
            continue;
        }

        // Check if any leaf type is a subtype of any root type
        for (auto root : newFuncOuterDeclInstTys) {
            auto found = std::find_if(funcOuterDeclInstTys.begin(), funcOuterDeclInstTys.end(),
                [this, &root](auto leaf) { return typeManager->IsSubtype(leaf, root); });
            if (found != funcOuterDeclInstTys.end()) {
                return true;
            }
        }
    }
    return false;
}

void OverrideFunctionResolver::MergeExtendSuperMember(
    AST::Ty& instBaseTy, MemberFuncsWithInstTys& funcs, MemberFuncsWithInstTys& newFuncs)
{
    CJC_NULLPTR_CHECK(typeManager);
    for (auto& newFunc : newFuncs) {
        auto newFuncOuterDeclInstTys = Promotion(*typeManager).Promote(instBaseTy, *newFunc.first->outerDecl->ty);
        for (auto newFuncInstTy : newFunc.second) {
            if (HasImplementationWithSubTypeCheck(instBaseTy, funcs, newFuncInstTy, newFuncOuterDeclInstTys)) {
                continue;
            }
            auto foundSameFuncDecl = funcs.find(newFunc.first);
            if (foundSameFuncDecl != funcs.end()) {
                foundSameFuncDecl->second.emplace(newFuncInstTy);
            } else {
                funcs.emplace(newFunc);
            }
        }
    }
}

void OverrideFunctionResolver::MergeIntoFuncs(MemberFuncsWithInstTys& funcs, MemberFuncsWithInstTys& newFuncs)
{
    for (auto& newFunc : newFuncs) {
        for (auto newFuncInstTy : newFunc.second) {
            bool hasImpl = false;
            for (auto& func : funcs) {
                if (Utils::In(newFuncInstTy, func.second)) {
                    hasImpl = true;
                    break;
                }
            }
            if (hasImpl) {
                continue;
            }
            auto foundSameFuncDecl = funcs.find(newFunc.first);
            if (foundSameFuncDecl != funcs.end()) {
                foundSameFuncDecl->second.emplace(newFuncInstTy);
            } else {
                funcs.emplace(newFunc);
            }
        }
    }
}

bool OverrideFunctionResolver::IsImplementationFunc(
    const FuncDecl& srcFunc, const FuncDecl& superFunc, const Ptr<Ty> srcInstFuncTy, const Ptr<Ty> superInstFuncTy)
{
    CJC_ASSERT(srcFunc.outerDecl && superFunc.outerDecl);
    // If function's static or generic status not equal, the will not have relation of implementation.
    bool noRelation = srcFunc.TestAttr(Attribute::STATIC) != superFunc.TestAttr(Attribute::STATIC) ||
        srcFunc.TestAttr(Attribute::GENERIC) != superFunc.TestAttr(Attribute::GENERIC) ||
        srcFunc.identifier != superFunc.identifier;
    if (noRelation) {
        return false;
    }
    if (srcFunc.TestAttr(Attribute::GENERIC) && superFunc.TestAttr(Attribute::GENERIC)) {
        TypeSubst mappingBetweenFuncs = typeManager->GenerateGenericMappingFromGeneric(srcFunc, superFunc);
        auto substSrcInstFuncTy = typeManager->GetInstantiatedTy(srcInstFuncTy, mappingBetweenFuncs);
        return typeManager->IsSubtype(substSrcInstFuncTy, superInstFuncTy);
    }
    return typeManager->IsSubtype(srcInstFuncTy, superInstFuncTy);
}

void OverrideFunctionResolver::ClearCache()
{
    instTy2Members.clear();
}
