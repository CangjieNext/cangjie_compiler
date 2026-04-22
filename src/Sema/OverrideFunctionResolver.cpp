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

#include "cangjie/Sema/OverrideFunctionResolver.h"
#include "cangjie/AST/InheritableDecl.h"
#include "cangjie/AST/ExtendDecl.h"
#include "cangjie/AST/ClassDecl.h"
#include "cangjie/AST/InterfaceDecl.h"
#include "cangjie/Sema/TypeCheckUtil.h"
#include "cangjie/Sema/TypeSubst.h"
#include "cangjie/Utils/Utils.h"

namespace cangjie {
namespace sema {

// Initialize static cache
std::unordered_map<std::pair<Ptr<AST::Ty>, std::string>, MemberFuncsWithInstTys, HashPair> OverrideFunctionResolver::instTy2Members;

MemberFuncsWithInstTys OverrideFunctionResolver::GetInstMemberFuncWithInstTy(AST::Ty& instBaseTy, const std::string& identifier) {
    CJC_ASSERT(typeManager && "OverrideFunctionResolver not properly initialized!");
    
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
    AST::Decl& decl, AST::Ty& instBaseTy, MemberFuncsWithInstTys& funcs, const std::string& identifier) {
    CJC_ASSERT(typeManager && "OverrideFunctionResolver not properly initialized!");
    
    auto mapping = GenerateTypeMappingByTy(decl.ty, &instBaseTy);
    for (auto& member : decl.GetMemberDecls()) {
        WorkForMembers(*member, [this, &funcs, &mapping, &identifier](auto& m) {
            if (m.identifier != identifier || m.astKind != AST::ASTKind::FUNC_DECL || m.TestAttr(AST::Attribute::ABSTRACT)) {
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

void OverrideFunctionResolver::GetInstMemberFromSuper(
    AST::Ty& instBaseTy, Ptr<AST::InheritableDecl> baseDecl, 
    MemberFuncsWithInstTys& funcs, const std::string& identifier, bool isCheckingInterface) {
    CJC_ASSERT(typeManager && "OverrideFunctionResolver not properly initialized!");
    
    for (auto& type : baseDecl->inheritedTypes) {
        bool notSkipSuper = isCheckingInterface ? type->ty->IsInterface() : type->ty->IsClass();
        if (!notSkipSuper) {
            continue;
        }
        OverrideFunctionResolver resolver(*typeManager, *promotion);
        auto superFuncs = resolver.GetInstMemberFuncWithInstTy(*type->ty, identifier);
        MergeIntoFuncs(funcs, superFuncs);
    }
    auto mappingBasePtrDecl2InstTy = GenerateTypeMappingByTy(baseDecl->ty, &instBaseTy);
    for (auto& superFunc : funcs) {
        std::unordered_set<Ptr<AST::Ty>> newTySet;
        for (auto& superFuncInstTy : superFunc.second) {
            newTySet.emplace(typeManager->GetInstantiatedTy(superFuncInstTy, mappingBasePtrDecl2InstTy));
        }
        superFunc.second = std::move(newTySet);
    }
}

bool OverrideFunctionResolver::HasImplementationWithSubTypeCheck(
    AST::Ty& instBaseTy, const MemberFuncsWithInstTys& funcs, 
    Ptr<AST::Ty> newFuncInstTy, const std::set<Ptr<AST::Ty>>& newFuncOuterDeclInstTys) {
    CJC_ASSERT(typeManager && promotion && "OverrideFunctionResolver not properly initialized!");
    
    for (const auto& func : funcs) {
        auto funcOuterDeclInstTys = promotion->Promote(instBaseTy, *func.first->outerDecl->ty);

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
    AST::Ty& instBaseTy, MemberFuncsWithInstTys& funcs, MemberFuncsWithInstTys& newFuncs) {
    CJC_ASSERT(typeManager && promotion && "OverrideFunctionResolver not properly initialized!");
    
    for (auto& newFunc : newFuncs) {
        auto newFuncOuterDeclInstTys = promotion->Promote(instBaseTy, *newFunc.first->outerDecl->ty);

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

void OverrideFunctionResolver::MergeIntoFuncs(MemberFuncsWithInstTys& funcs, MemberFuncsWithInstTys& newFuncs) {
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
    AST::Ty& ty, const AST::FuncDecl& interfaceFunc, const AST::FuncDecl& fd) {
    CJC_ASSERT(fd.outerDecl && interfaceFunc.outerDecl);
    CJC_ASSERT(typeManager && promotion && "OverrideFunctionResolver not properly initialized!");
    
    // If function's static or generic status not equal, the will not have relation of implementation.
    bool noRelation = interfaceFunc.TestAttr(AST::Attribute::STATIC) != fd.TestAttr(AST::Attribute::STATIC) ||
        interfaceFunc.TestAttr(AST::Attribute::GENERIC) != fd.TestAttr(AST::Attribute::GENERIC);
    if (noRelation) {
        return false;
    }
    if (interfaceFunc.outerDecl->astKind == fd.outerDecl->astKind) {
        // If both functions are declared in same kind of type decls, directly return if 'fd' is abstract.
        if (fd.TestAttr(AST::Attribute::ABSTRACT)) {
            return false;
        }
    } else {
        // If functions are declared in different kind of type decls, directly return if 'fd' is in interface.
        if (fd.outerDecl->astKind == AST::ASTKind::INTERFACE_DECL) {
            return false;
        }
    }
    auto baseTy = Ptr(&ty);
    auto structDecl = RawStaticCast<AST::InheritableDecl*>(fd.outerDecl);
    // Update base ty to extendDecl's ty when the extended type is current given 'ty'.
    // For primitive and builtin types, the declPtr is always nullptr that always update baseTy to extendDecl ty.
    // For user defined types only update baseTy when declPtr is same.
    bool shouldUpdateImplementTy = true;
    if (auto ed = DynamicCast<AST::ExtendDecl*>(structDecl);
        ed && AST::Ty::GetDeclPtrOfTy(ed->extendedType->ty) == AST::Ty::GetDeclPtrOfTy(baseTy)) {
        baseTy = ed->extendedType->ty;
        shouldUpdateImplementTy = false;
    }
    // 1. Substitute interface func's type to baseTy. NOTE: May update baseTy's value.
    auto interfaceTy = interfaceFunc.outerDecl->ty;
    auto typeMappings = GetTypeMapping(baseTy, *interfaceTy);
    for (auto& type : structDecl->inheritedTypes) {
        // Eg: generate mapping for extend Int16 <: Number<Int32> and interface Number<T>.
        if (type && type->ty && typeManager->IsSubtype(AST::Ty::GetGenericTyOfInsTy(*type->ty), interfaceTy)) {
            typeManager->GenerateGenericMapping(typeMappings, *type->ty);
            break;
        }
    }
    auto iFuncTy = interfaceFunc.ty;
    // 1.5 Mapping function generic from 'interfaceFunc' to 'fd'.
    // Previously checked 'interfaceFunc' and 'fd' have same generic status.
    if (fd.TestAttr(AST::Attribute::GENERIC)) {
        TypeSubst typeMapping = typeManager->GenerateGenericMappingFromGeneric(interfaceFunc, fd);
        iFuncTy = typeManager->GetInstantiatedTy(iFuncTy, typeMapping);
    }
    // For 'interface I<T> { func foo(a: T) {} }; class C <: I<A> & I<B>',
    // function 'foo' may have multiple instantiation type.
    auto interfaceFuncTys = typeManager->GetInstantiatedTys(iFuncTy, typeMappings);

    // 2. Substitute implemented func's type to baseTy.
    auto implementedFuncTy = RawStaticCast<AST::FuncTy*>(fd.ty);
    if (shouldUpdateImplementTy) {
        auto structTy = structDecl->ty;
        if (baseTy && structTy) {
            typeMappings = promotion->GetPromoteTypeMapping(*baseTy, *structTy);
        } else {
            typeMappings = {};
        }
        implementedFuncTy = RawStaticCast<AST::FuncTy*>(typeManager->GetBestInstantiatedTy(fd.ty, typeMappings));
    }

    // 3. compare two func's signature. structure declaration mapping relation if signatures are same.
    // when function have multiple instantiated types, only need one of them passed.
    // Actual type will be decided in rearrange stage.
    return std::any_of(interfaceFuncTys.begin(), interfaceFuncTys.end(), [this, implementedFuncTy](auto ty) {
        return implementedFuncTy && ty &&
            typeManager->IsFuncParameterTypesIdentical(*implementedFuncTy, *RawStaticCast<AST::FuncTy*>(ty));
    });
}

void OverrideFunctionResolver::ClearCache() {
    instTy2Members.clear();
}

} // namespace sema
} // namespace cangjie
