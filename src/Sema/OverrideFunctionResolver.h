#ifndef CANGJIE_SEMA_OVERRIDE_FUNCTION_RESOLVER_H
#define CANGJIE_SEMA_OVERRIDE_FUNCTION_RESOLVER_H

#include "cangjie/AST/Decl.h"
#include "cangjie/AST/Ty.h"
#include "cangjie/Sema/TypeManager.h"
#include "cangjie/Sema/TypePromotion.h"
#include "cangjie/Sema/GenericInstantiation/GenericInstantiationManagerImpl.h"

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <set>

namespace cangjie {
namespace sema {

/**
 * A utility class to resolve and manage overridden member functions for a given type.
 * This class provides functionality to:
 * 1. Collect member functions from various sources (super interfaces, super classes, type itself, extend declarations)
 * 2. Handle function overriding and implementation relationships
 * 3. Manage function override sets and instantiation types
 * 4. Cache results for efficient lookup
 */
class OverrideFunctionResolver {
public:
    /**
     * Constructor.
     * @param typeManager The type manager to use.
     * @param promotion The type promotion utility to use.
     */
    OverrideFunctionResolver(TypeManager& typeManager, TypePromotion& promotion)
        : typeManager(&typeManager), promotion(&promotion) {}
    
    /**
     * Get instantiated member functions with their instantiation types for a given instantiated type.
     * This function collects member functions from:
     * 1. Super interfaces of the base type
     * 2. Super classes of the base type
     * 3. The base type itself
     * 4. All extend declarations of the base type
     * 5. Super interfaces of all extend declarations
     * 6. Extend declarations themselves
     *
     * When merging member functions from extend declarations, if functions have the same type,
     * the existing function is preserved.
     *
     * @param instBaseTy The instantiated base type to get member functions from
     * @param identifier The name of the member functions to retrieve
     * @return The collection of retrieved member functions with their instantiation types
     */
    MemberFuncsWithInstTys GetInstMemberFuncWithInstTy(AST::Ty& instBaseTy, const std::string& identifier);
    
    /**
     * Clear the global cache.
     */
    static void ClearCache();
    
private:
    /**
     * Collect member functions from a declaration.
     */
    void CollectDeclMemberFunc(
        AST::Decl& decl, AST::Ty& instBaseTy, MemberFuncsWithInstTys& funcs, const std::string& identifier);
    
    /**
     * Get instantiated members from super types (interfaces or classes).
     */
    void GetInstMemberFromSuper(
        AST::Ty& instBaseTy, Ptr<AST::InheritableDecl> baseDecl, 
        MemberFuncsWithInstTys& funcs, const std::string& identifier, bool isCheckingInterface);
    
    /**
     * Check if a new function has an implementation in the existing functions with subtype check.
     */
    bool HasImplementationWithSubTypeCheck(
        AST::Ty& instBaseTy, const MemberFuncsWithInstTys& funcs, 
        Ptr<AST::Ty> newFuncInstTy, const std::set<Ptr<AST::Ty>>& newFuncOuterDeclInstTys);
    
    /**
     * Merge member functions from extend declarations, selecting the subclass version in case of conflicts.
     */
    void MergeExtendSuperMember(
        AST::Ty& instBaseTy, MemberFuncsWithInstTys& funcs, MemberFuncsWithInstTys& newFuncs);
    
    /**
     * Merge new functions into existing functions, avoiding duplicates.
     */
    void MergeIntoFuncs(MemberFuncsWithInstTys& funcs, MemberFuncsWithInstTys& newFuncs);
    
    /**
     * Check if two functions have an implementation/override relation.
     */
    bool IsImplementationFunc(
        AST::Ty& ty, const AST::FuncDecl& interfaceFunc, const AST::FuncDecl& fd);
    
    // Dependencies
    TypeManager* typeManager = nullptr;
    TypePromotion* promotion = nullptr;
    
    // Global cache to store results for faster lookup
    static std::unordered_map<std::pair<Ptr<AST::Ty>, std::string>, MemberFuncsWithInstTys, HashPair> instTy2Members;
};

} // namespace sema
} // namespace cangjie

#endif // CANGJIE_SEMA_OVERRIDE_FUNCTION_RESOLVER_H
