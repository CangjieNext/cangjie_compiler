// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_CHIR_TRANSFORMATION_PATCH_CJMP_IMPL_BODIES_H
#define CANGJIE_CHIR_TRANSFORMATION_PATCH_CJMP_IMPL_BODIES_H

#include "cangjie/CHIR/IR/CHIRBuilder.h"
#include "cangjie/CHIR/IR/Package.h"

namespace Cangjie::CHIR {

/// Patches field references and field initializations in @JavaImpl constructors
/// and methods that originate from the CJMP common part.
///
/// In the CJMP model, common-part constructor and method bodies are deserialized
/// from RAW CHIR. After upstream's reference-wrapper pipeline relocates user
/// fields to a registry companion object, these deserialized bodies still
/// reference fields directly (via GetElementRef/GetElementByName). This class
/// rewrites those references to use getter/setter calls on the companion object,
/// and inserts field-initialization calls for non-MUT fields that have an
/// initializerFunc.
class PatchCJMPImplBodies {
public:
    PatchCJMPImplBodies(Package& pkg, CHIRBuilder& builder);

    /// Patches field load/store expressions in @JavaImpl + FROM_COMMON_PART
    /// constructors and deserialized/common methods to use getter/setter
    /// calls on the companion object.
    void Run();

    /// Inserts field-initialization code for non-MUT fields with
    /// \c initializerFunc in @JavaImpl + FROM_COMMON_PART constructors.
    /// Called after all declarations are translated (step 5 in AST2CHIR).
    void RunCtorFieldInitOnly();

private:
    /// Result of searching a constructor body for the store that writes
    /// the companion object reference into the \c $reg field.
    struct RegStoreInfo {
        Value* cregRef = nullptr;
        Block* targetBlock = nullptr;
    };

    /// Visits a function body and patches field load/store expressions
    /// to getter/setter calls on the companion object.
    /// \param func The constructor or method to patch.
    /// \param implClassDef The @JavaImpl class def containing the fields.
    void PatchFieldReferences(Function& func, ClassDef& implClassDef);

    /// Attempts to replace a Load backed by GetElementRef (concrete index)
    /// with a getter Apply call.
    /// \return The replacement expression, or nullptr if the load does not
    ///         reference a user field with a JAVA_IMPL getter.
    Expression* TryPatchLoad(Load& load, ClassDef& implClassDef);

    /// Attempts to replace a Load backed by GetElementByName (symbolic name)
    /// with a getter Apply call.
    /// \return The replacement expression, or nullptr if the load does not
    ///         reference a user field with a JAVA_IMPL getter.
    Expression* TryPatchLoadByName(Load& load, ClassDef& implClassDef);

    /// Patches a StoreElementByName to use a setter Apply call on the
    /// companion object. If no JAVA_IMPL setter exists, replaces the store
    /// with a Unit literal (field is effectively read-only).
    void TryPatchStoreElementByName(StoreElementByName& storeByName, ClassDef& implClassDef);

    /// Attempts to replace a StoreElementRef (concrete index) with a setter
    /// Apply call. If no JAVA_IMPL setter exists, replaces the store with a
    /// Unit literal.
    /// \return The replacement expression, or nullptr if replaced with Unit
    ///         or if the store does not reference a user field.
    Expression* TryPatchStoreElementRef(StoreElementRef& store, ClassDef& implClassDef);

    /// Orchestrates field-initialization insertion for a single @JavaImpl
    /// constructor from the common part. Finds the \c $reg companion field,
    /// locates where the constructor stores the companion reference, then
    /// inserts initializerFunc calls that store results into the companion.
    /// \param ctor The constructor to patch.
    /// \param implClassDef The @JavaImpl class def.
    void PatchConstructorFieldInit(Function& ctor, ClassDef& implClassDef);

    /// Finds the index of the \c $reg companion field in the class's instance
    /// variables. Validates that the field type is a class reference.
    /// \param allFields All instance variables of the class def.
    /// \return The field index, or \c allFields.size() if not found or invalid.
    size_t FindRegFieldIndex(const std::vector<MemberVarInfo>& allFields) const;

    /// Searches a constructor body for the store that writes the companion
    /// object reference into the \c $reg field.
    /// \param ctor The constructor to search.
    /// \param regIdx The index of the \c $reg field.
    /// \return The stored companion value and its target block, or
    ///         \c {nullptr, nullptr} if not found.
    RegStoreInfo FindRegStore(Function& ctor, size_t regIdx) const;

    /// Creates initializerFunc calls and stores for eligible fields into the
    /// companion object. Processes non-COMPILER_ADD, non-MUT fields that have
    /// an \c initializerFunc.
    /// \param allFields All instance variables of the class def.
    /// \param cregRef The companion object reference value.
    /// \param targetBlock The block where init code should be inserted.
    /// \param thisParam The constructor's \c this parameter.
    /// \param loc Debug location for created expressions.
    void InsertFieldInitializations(const std::vector<MemberVarInfo>& allFields, Value& cregRef,
                                    Block& targetBlock, Value& thisParam, const DebugLocation& loc);

    Package& pkg;
    CHIRBuilder& builder;
};

} // namespace Cangjie::CHIR

#endif
