// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/Transformation/PatchCJMPImplBodies.h"

#include "cangjie/CHIR/IR/Expression/Expression.h"
#include "cangjie/CHIR/IR/Type/ClassDef.h"
#include "cangjie/CHIR/IR/Type/Type.h"
#include "cangjie/CHIR/IR/Value/LiteralValue.h"
#include "cangjie/CHIR/IR/Value/Value.h"
#include "cangjie/CHIR/Utils/CHIRCasting.h"
#include "cangjie/CHIR/Utils/JavaMirrorFieldHelper.h"
#include "cangjie/CHIR/Utils/Visitor/Visitor.h"
#include "cangjie/Utils/CastingTemplate.h"
#include "cangjie/Utils/ConstantsUtils.h"
#include "cangjie/Utils/ProfileRecorder.h"

using namespace Cangjie::CHIR;

PatchCJMPImplBodies::PatchCJMPImplBodies(Package& pkg, CHIRBuilder& builder)
    : pkg(pkg), builder(builder)
{
}

/// \copydoc PatchCJMPImplBodies::Run
void PatchCJMPImplBodies::Run()
{
    Utils::ProfileRecorder recorder("TranslateNominalDecls", "PatchCJMPImplBodies");

    for (auto classDef : pkg.GetAllClassDef()) {
        if (!classDef->TestAttr(Attribute::JAVA_IMPL) || !classDef->TestAttr(Attribute::FROM_COMMON_PART)) {
            continue;
        }

        for (auto method : classDef->GetMethods()) {
            if (!method || !method->IsFuncWithBody()) {
                continue;
            }
            if (method->GetFuncKind() == FuncKind::GETTER || method->GetFuncKind() == FuncKind::SETTER) {
                continue;
            }
            if (method->IsConstructor()) {
                PatchFieldReferences(*method, *classDef);
                continue;
            }
            if (!method->TestAttr(Attribute::DESERIALIZED) && !method->TestAttr(Attribute::COMMON)) {
                continue;
            }
            PatchFieldReferences(*method, *classDef);
        }
    }
}

/// \copydoc PatchCJMPImplBodies::RunCtorFieldInitOnly
void PatchCJMPImplBodies::RunCtorFieldInitOnly()
{
    for (auto classDef : pkg.GetAllClassDef()) {
        if (!classDef->TestAttr(Attribute::JAVA_IMPL) || !classDef->TestAttr(Attribute::FROM_COMMON_PART)) {
            continue;
        }

        for (auto method : classDef->GetMethods()) {
            if (!method || !method->IsFuncWithBody()) {
                continue;
            }
            if (method->GetFuncKind() == FuncKind::GETTER || method->GetFuncKind() == FuncKind::SETTER) {
                continue;
            }
            if (method->IsConstructor()) {
                PatchConstructorFieldInit(*method, *classDef);
            }
        }
    }
}

/// \copydoc PatchCJMPImplBodies::PatchFieldReferences
void PatchCJMPImplBodies::PatchFieldReferences(Function& func, ClassDef& implClassDef)
{
    auto preVisit = [this, &implClassDef](Expression& e) {
        if (auto load = DynamicCast<Load*>(&e)) {
            auto newExpr = TryPatchLoad(*load, implClassDef);
            if (!newExpr) {
                newExpr = TryPatchLoadByName(*load, implClassDef);
            }
            if (newExpr) {
                load->ReplaceWith(*newExpr);
            }
        } else if (auto storeByName = DynamicCast<StoreElementByName*>(&e)) {
            TryPatchStoreElementByName(*storeByName, implClassDef);
        } else if (auto store = DynamicCast<StoreElementRef*>(&e)) {
            auto newExpr = TryPatchStoreElementRef(*store, implClassDef);
            if (newExpr) {
                store->ReplaceWith(*newExpr);
            }
        }
        return VisitResult::CONTINUE;
    };

    Visitor::Visit(func, preVisit);
}

/// \copydoc PatchCJMPImplBodies::TryPatchLoad
Expression* PatchCJMPImplBodies::TryPatchLoad(Load& load, ClassDef& implClassDef)
{
    auto location = load.GetLocation();
    if (!location) return nullptr;

    auto localVar = DynamicCast<LocalVar*>(location);
    if (!localVar) return nullptr;

    auto getRef = DynamicCast<GetElementRef*>(localVar->GetExpr());
    if (!getRef) return nullptr;

    auto path = getRef->GetPath();
    if (path.empty()) return nullptr;
    auto fieldIdx = path[0];

    auto allFields = implClassDef.GetAllInstanceVars();
    if (fieldIdx >= allFields.size()) return nullptr;
    auto& field = allFields[fieldIdx];

    if (field.TestAttr(Attribute::COMPILER_ADD)) return nullptr;

    auto fieldName = field.name;
    if (fieldName.empty()) return nullptr;

    auto getter = FindInstanceGetterByName(implClassDef, fieldName);
    if (!getter || !getter->TestAttr(Attribute::COMPILER_ADD)) return nullptr;

    auto baseValue = getRef->GetLocation();
    FuncCallContext callContext {
        .args = {baseValue},
        .instTypeArgs = {},
        .thisType = baseValue ? baseValue->GetType() : nullptr
    };

    auto loc = load.GetDebugLocation();
    auto resultTy = getter->GetReturnType();
    return builder.CreateExpression<Apply>(
        loc, resultTy, StaticCast<Value*>(getter), callContext, load.GetParentBlock());
}

/// \copydoc PatchCJMPImplBodies::TryPatchLoadByName
Expression* PatchCJMPImplBodies::TryPatchLoadByName(Load& load, ClassDef& implClassDef)
{
    auto location = load.GetLocation();
    if (!location) return nullptr;

    auto localVar = DynamicCast<LocalVar*>(location);
    if (!localVar) return nullptr;

    auto getByName = DynamicCast<GetElementByName*>(localVar->GetExpr());
    if (!getByName) return nullptr;

    auto names = getByName->GetNames();
    if (names.empty()) return nullptr;

    auto getter = FindInstanceGetterByName(implClassDef, names[0]);
    if (!getter || !getter->TestAttr(Attribute::COMPILER_ADD)) return nullptr;

    auto baseValue = getByName->GetLocation();
    FuncCallContext callContext {
        .args = {baseValue},
        .instTypeArgs = {},
        .thisType = baseValue ? baseValue->GetType() : nullptr
    };

    auto loc = load.GetDebugLocation();
    auto resultTy = getter->GetReturnType();
    return builder.CreateExpression<Apply>(
        loc, resultTy, StaticCast<Value*>(getter), callContext, load.GetParentBlock());
}

/// \copydoc PatchCJMPImplBodies::TryPatchStoreElementByName
void PatchCJMPImplBodies::TryPatchStoreElementByName(StoreElementByName& storeByName, ClassDef& implClassDef)
{
    auto names = storeByName.GetNames();
    if (names.empty()) {
        return;
    }

    auto allFields = implClassDef.GetAllInstanceVars();
    bool isCompilerAdd = false;
    for (auto& f : allFields) {
        if (f.name == names[0] && f.TestAttr(Attribute::COMPILER_ADD)) {
            isCompilerAdd = true;
            break;
        }
    }
    if (isCompilerAdd) {
        return;
    }

    auto setter = FindInstanceSetterByName(implClassDef, names[0]);
    auto loc = storeByName.GetDebugLocation();
    if (setter && setter->TestAttr(Attribute::COMPILER_ADD)) {
        auto thisValue = storeByName.GetLocation();
        auto storedValue = storeByName.GetValue();
        if (thisValue && storedValue) {
            FuncCallContext callContext {
                .args = {thisValue, storedValue},
                .instTypeArgs = {},
                .thisType = thisValue->GetType()
            };
            auto resultTy = builder.GetUnitTy();
            auto newExpr = builder.CreateExpression<Apply>(
                loc, resultTy, StaticCast<Value*>(setter), callContext, storeByName.GetParentBlock());
            storeByName.ReplaceWith(*newExpr);
        }
    } else {
        auto unitExpr = builder.CreateConstantExpression<UnitLiteral>(
            loc, builder.GetUnitTy(), storeByName.GetParentBlock());
        storeByName.ReplaceWith(*unitExpr);
    }
}

/// \copydoc PatchCJMPImplBodies::TryPatchStoreElementRef
Expression* PatchCJMPImplBodies::TryPatchStoreElementRef(StoreElementRef& store, ClassDef& implClassDef)
{
    auto baseValue = store.GetLocation();
    auto storedValue = store.GetValue();
    auto path = store.GetPath();

    if (path.empty()) return nullptr;
    auto fieldIdx = path[0];

    auto allFields = implClassDef.GetAllInstanceVars();
    if (fieldIdx >= allFields.size()) return nullptr;
    auto& field = allFields[fieldIdx];

    if (field.TestAttr(Attribute::COMPILER_ADD)) return nullptr;

    auto fieldName = field.name;
    if (fieldName.empty()) return nullptr;

    auto setter = FindInstanceSetterByName(implClassDef, fieldName);
    auto loc = store.GetDebugLocation();
    if (!setter || !setter->TestAttr(Attribute::COMPILER_ADD)) {
        auto unitExpr = builder.CreateConstantExpression<UnitLiteral>(loc, builder.GetUnitTy(), store.GetParentBlock());
        store.ReplaceWith(*unitExpr);
        return nullptr;
    }

    FuncCallContext callContext {
        .args = {baseValue, storedValue},
        .instTypeArgs = {},
        .thisType = baseValue ? baseValue->GetType() : nullptr
    };

    auto resultTy = builder.GetUnitTy();
    return builder.CreateExpression<Apply>(
        loc, resultTy, StaticCast<Value*>(setter), callContext, store.GetParentBlock());
}

/// \copydoc PatchCJMPImplBodies::PatchConstructorFieldInit
void PatchCJMPImplBodies::PatchConstructorFieldInit(Function& ctor, ClassDef& implClassDef)
{
    auto allFields = implClassDef.GetAllInstanceVars();

    size_t regIdx = FindRegFieldIndex(allFields);
    if (regIdx >= allFields.size()) { return; }

    auto store = FindRegStore(ctor, regIdx);
    if (!store.cregRef || !store.targetBlock) { return; }

    auto thisParam = ctor.GetParam(0);
    if (!thisParam) { return; }

    InsertFieldInitializations(allFields, *store.cregRef, *store.targetBlock,
                               *thisParam, ctor.GetDebugLocation());
}

/// \copydoc PatchCJMPImplBodies::FindRegFieldIndex
size_t PatchCJMPImplBodies::FindRegFieldIndex(const std::vector<MemberVarInfo>& allFields) const
{
    for (size_t i = 0; i < allFields.size(); ++i) {
        if (allFields[i].name != Cangjie::Interop::Java::JAVA_IMPL_REGISTRY_COMPANION_REF_FIELD_NAME ||
            !allFields[i].TestAttr(Attribute::COMPILER_ADD)) {
            continue;
        }
        auto fieldType = allFields[i].type;
        if (!fieldType) { return allFields.size(); }
        auto customType = DynamicCast<CustomType*>(fieldType->StripAllRefs());
        if (!customType) { return allFields.size(); }
        if (!DynamicCast<ClassDef*>(customType->GetCustomTypeDef())) { return allFields.size(); }
        return i;
    }
    return allFields.size();
}

/// \copydoc PatchCJMPImplBodies::FindRegStore
PatchCJMPImplBodies::RegStoreInfo PatchCJMPImplBodies::FindRegStore(Function& ctor, size_t regIdx) const
{
    RegStoreInfo result;

    Visitor::Visit(ctor, [&](Expression& e) -> VisitResult {
        if (auto storeRef = DynamicCast<StoreElementRef*>(&e)) {
            auto path = storeRef->GetPath();
            if (!path.empty() && path[0] == regIdx) {
                result.cregRef = storeRef->GetValue();
                result.targetBlock = storeRef->GetParentBlock();
                return VisitResult::STOP;
            }
        } else if (auto storeByName = DynamicCast<StoreElementByName*>(&e)) {
            auto names = storeByName->GetNames();
            if (!names.empty() && names[0] == Cangjie::Interop::Java::JAVA_IMPL_REGISTRY_COMPANION_REF_FIELD_NAME) {
                result.cregRef = storeByName->GetValue();
                result.targetBlock = storeByName->GetParentBlock();
                return VisitResult::STOP;
            }
        }
        return VisitResult::CONTINUE;
    });

    return result;
}

/// \copydoc PatchCJMPImplBodies::InsertFieldInitializations
void PatchCJMPImplBodies::InsertFieldInitializations(const std::vector<MemberVarInfo>& allFields,
    Value& cregRef, Block& targetBlock, Value& thisParam, const DebugLocation& loc)
{
    auto terminator = targetBlock.GetTerminator();
    if (!terminator) { return; }

    for (auto& field : allFields) {
        if (field.TestAttr(Attribute::COMPILER_ADD)) continue;
        if (!field.initializerFunc) continue;
        if (field.TestAttr(Attribute::MUT)) continue;

        auto initApply = builder.CreateExpression<Apply>(
            loc,
            field.initializerFunc->GetReturnType(),
            StaticCast<Value*>(field.initializerFunc),
            FuncCallContext{
                .args = {&thisParam},
                .instTypeArgs = {},
                .thisType = thisParam.GetType()
            },
            &targetBlock);
        initApply->MoveBefore(terminator);

        auto initStore = builder.CreateExpression<StoreElementByName>(
            loc,
            builder.GetUnitTy(),
            initApply->GetResult(),
            &cregRef,
            std::vector<std::string>{field.name},
            &targetBlock);
        initStore->MoveBefore(terminator);
    }
}
