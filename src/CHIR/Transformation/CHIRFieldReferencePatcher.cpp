// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/Transformation/CHIRFieldReferencePatcher.h"
#include "cangjie/CHIR/Utils/CHIRCasting.h"
#include "cangjie/CHIR/Utils/Visitor/Visitor.h"
#include "cangjie/CHIR/IR/Expression/Expression.h"
#include "cangjie/CHIR/IR/Type/Type.h"
#include "cangjie/Utils/CheckUtils.h"

namespace Cangjie::CHIR {

CHIRFieldReferencePatcher::CHIRFieldReferencePatcher(Package& pkg, CHIRBuilder& builder, Attribute mirrorAttr)
    : pkg(pkg), builder(builder), mirrorAttr(mirrorAttr) {}

void CHIRFieldReferencePatcher::Run()
{
    for (auto func : pkg.GetGlobalFuncsWithBody()) {
        if (func->TestAttr(Attribute::DESERIALIZED) || func->TestAttr(Attribute::COMMON)) {
            auto parentDef = func->GetOuterDeclaredOrExtendedDef();
            if (parentDef && parentDef->TestAttr(mirrorAttr)) {
                PatchFunction(*func);
            } else {
                PatchGlobalFunction(*func);
            }
        }
    }
}

void CHIRFieldReferencePatcher::PatchFunction(Function& func)
{
    auto parentDef = func.GetOuterDeclaredOrExtendedDef();
    if (!parentDef) return;

    if (!parentDef->TestAttr(mirrorAttr)) return;

    auto classDef = DynamicCast<ClassDef*>(parentDef);
    if (!classDef) return;

    auto preVisit = [this, classDef](Expression& e) {
        if (auto load = DynamicCast<Load*>(&e)) {
            auto newExpr = TryPatchLoad(*load, *classDef);
            if (!newExpr) {
                newExpr = TryPatchLoadByName(*load, *classDef);
            }
            if (newExpr) {
                load->ReplaceWith(*newExpr);
            }
        } else if (auto storeByName = DynamicCast<StoreElementByName*>(&e)) {
            auto newExpr = TryPatchStoreElementByName(*storeByName, *classDef);
            if (newExpr) {
                storeByName->ReplaceWith(*newExpr);
            }
        } else if (auto store = DynamicCast<StoreElementRef*>(&e)) {
            auto newExpr = TryPatchStoreElementRef(*store, *classDef);
            if (newExpr) {
                store->ReplaceWith(*newExpr);
            }
        } else if (auto fieldExpr = DynamicCast<Field*>(&e)) {
            auto newExpr = TryPatchField(*fieldExpr, *classDef);
            if (newExpr) {
                fieldExpr->ReplaceWith(*newExpr);
            }
        } else if (auto fieldByName = DynamicCast<FieldByName*>(&e)) {
            auto newExpr = TryPatchFieldByName(*fieldByName, *classDef);
            if (newExpr) {
                fieldByName->ReplaceWith(*newExpr);
            }
        }
        return VisitResult::CONTINUE;
    };
    Visitor::Visit(func, preVisit);
}

void CHIRFieldReferencePatcher::PatchGlobalFunction(Function& func)
{
    auto preVisit = [this](Expression& e) {
        if (auto load = DynamicCast<Load*>(&e)) {
            auto newExpr = TryPatchLoadGlobalVar(*load);
            if (!newExpr) {
                newExpr = TryPatchLoadGlobal(*load);
            }
            if (newExpr) {
                load->ReplaceWith(*newExpr);
            }
        } else if (auto store = DynamicCast<Store*>(&e)) {
            auto newExpr = TryPatchStoreGlobalVar(*store);
            if (newExpr) {
                store->ReplaceWith(*newExpr);
            }
        } else if (auto fieldExpr = DynamicCast<Field*>(&e)) {
            auto newExpr = TryPatchFieldGlobal(*fieldExpr);
            if (newExpr) {
                fieldExpr->ReplaceWith(*newExpr);
            }
        } else if (auto storeElementRef = DynamicCast<StoreElementRef*>(&e)) {
            auto mirrorClass = GetMirrorClassDefFromType(storeElementRef->GetBase()->GetType());
            if (!mirrorClass) {
                return VisitResult::CONTINUE;
            }
            auto newExpr = TryPatchStoreElementRef(*storeElementRef, *mirrorClass);
            if (newExpr) {
                storeElementRef->ReplaceWith(*newExpr);
            }
        } else if (auto storeByName = DynamicCast<StoreElementByName*>(&e)) {
            auto location = storeByName->GetLocation();
            auto mirrorClass = location ? GetMirrorClassDefFromType(location->GetType()) : nullptr;
            if (!mirrorClass) {
                return VisitResult::CONTINUE;
            }
            auto newExpr = TryPatchStoreElementByName(*storeByName, *mirrorClass);
            if (newExpr) {
                storeByName->ReplaceWith(*newExpr);
            }
        } else if (auto fieldByName = DynamicCast<FieldByName*>(&e)) {
            auto baseValue = fieldByName->GetBase();
            auto mirrorClass = baseValue ? GetMirrorClassDefFromType(baseValue->GetType()) : nullptr;
            if (!mirrorClass) {
                return VisitResult::CONTINUE;
            }
            auto newExpr = TryPatchFieldByName(*fieldByName, *mirrorClass);
            if (newExpr) {
                fieldByName->ReplaceWith(*newExpr);
            }
        }
        return VisitResult::CONTINUE;
    };
    Visitor::Visit(func, preVisit);
}

ClassDef* CHIRFieldReferencePatcher::GetMirrorClassDefFromType(Type* ty)
{
    if (!ty) return nullptr;
    auto derefType = ty->StripAllRefs();
    auto classType = DynamicCast<ClassType*>(derefType);
    if (!classType) return nullptr;
    auto classDef = classType->GetClassDef();
    if (!classDef || !classDef->TestAttr(mirrorAttr)) return nullptr;
    return classDef;
}

ClassDef* CHIRFieldReferencePatcher::GetMirrorClassDefFromGlobalVar(GlobalVar* gv)
{
    if (!gv) return nullptr;
    auto parentTypeDef = gv->GetParentCustomTypeDef();
    if (!parentTypeDef) return nullptr;
    auto parentClassDef = DynamicCast<ClassDef*>(parentTypeDef);
    if (!parentClassDef) return nullptr;

    auto identifier = parentClassDef->GetIdentifier();
    auto specificClassDef = pkg.TryGetClassDef(identifier);
    if (specificClassDef && specificClassDef->TestAttr(mirrorAttr)) {
        return specificClassDef;
    }

    if (parentClassDef->TestAttr(mirrorAttr)) {
        return parentClassDef;
    }

    return nullptr;
}

Expression* CHIRFieldReferencePatcher::TryPatchLoadGlobalVar(Load& load)
{
    auto location = load.GetLocation();
    auto globalVar = DynamicCast<GlobalVar*>(location);
    if (!globalVar) return nullptr;

    auto mirrorClassDef = GetMirrorClassDefFromGlobalVar(globalVar);
    if (!mirrorClassDef) return nullptr;

    auto getter = FindStaticGetterForGlobalVar(*mirrorClassDef, *globalVar);
    CJC_ASSERT_WITH_MSG(getter != nullptr, "CJMP mirror static field has no matching getter");
    if (!getter) return nullptr;

    auto loc = load.GetDebugLocation();
    return CreateStaticGetterCall(loc, getter, load.GetParentBlock());
}

Expression* CHIRFieldReferencePatcher::TryPatchStoreGlobalVar(Store& store)
{
    auto location = store.GetLocation();
    auto globalVar = DynamicCast<GlobalVar*>(location);
    if (!globalVar) return nullptr;

    auto mirrorClassDef = GetMirrorClassDefFromGlobalVar(globalVar);
    if (!mirrorClassDef) return nullptr;

    auto setter = FindStaticSetterForGlobalVar(*mirrorClassDef, *globalVar);
    CJC_ASSERT_WITH_MSG(setter != nullptr, "CJMP mirror static field has no matching setter");
    if (!setter) return nullptr;

    auto loc = store.GetDebugLocation();
    return CreateStaticSetterCall(loc, store.GetValue(), setter, store.GetParentBlock());
}

Expression* CHIRFieldReferencePatcher::TryPatchFieldGlobal(Field& fieldExpr)
{
    auto baseValue = fieldExpr.GetBase();
    auto mirrorClassDef = GetMirrorClassDefFromType(baseValue->GetType());
    if (!mirrorClassDef) return nullptr;

    auto path = fieldExpr.GetPath();
    if (path.empty()) return nullptr;
    auto fieldIdx = path[0];

    auto allFields = mirrorClassDef->GetAllInstanceVars();
    if (fieldIdx >= allFields.size()) return nullptr;
    auto& field = allFields[fieldIdx];

    if (!IsJavaMirrorField(field, mirrorAttr)) return nullptr;

    auto getter = FindGetterForField(*mirrorClassDef, field, mirrorAttr);
    if (!getter) return nullptr;

    auto loc = fieldExpr.GetDebugLocation();
    return CreateGetterCall(loc, baseValue, getter, fieldExpr.GetParentBlock());
}

Expression* CHIRFieldReferencePatcher::TryPatchLoadGlobal(Load& load)
{
    auto location = load.GetLocation();
    auto localVar = DynamicCast<LocalVar*>(location);
    if (!localVar) return nullptr;

    auto localVarExpr = localVar->GetExpr();
    if (!localVarExpr) return nullptr;

    if (localVarExpr->GetExprKind() == ExprKind::GET_ELEMENT_REF) {
        auto getRef = StaticCast<GetElementRef*>(localVarExpr);
        auto baseValue = getRef->GetBase();
        auto mirrorClassDef = GetMirrorClassDefFromType(baseValue->GetType());
        if (!mirrorClassDef) return nullptr;

        auto path = getRef->GetPath();
        if (path.empty()) return nullptr;
        auto fieldIdx = path[0];

        auto allFields = mirrorClassDef->GetAllInstanceVars();
        if (fieldIdx >= allFields.size()) return nullptr;
        auto& field = allFields[fieldIdx];

        if (!IsJavaMirrorField(field, mirrorAttr)) return nullptr;

        auto getter = FindGetterForField(*mirrorClassDef, field, mirrorAttr);
        if (!getter) {
            getter = FindInstanceGetterByName(*mirrorClassDef, field.name);
            if (!getter) return nullptr;
        }

        auto loc = load.GetDebugLocation();
        return CreateGetterCall(loc, baseValue, getter, load.GetParentBlock());
    }

    if (localVarExpr->GetExprKind() == ExprKind::GET_ELEMENT_BY_NAME) {
        auto getByName = StaticCast<GetElementByName*>(localVarExpr);
        auto baseValue = getByName->GetLocation();
        auto mirrorClassDef = GetMirrorClassDefFromType(baseValue->GetType());
        if (!mirrorClassDef) return nullptr;

        auto names = getByName->GetNames();
        if (names.size() != 1) return nullptr;

        auto getter = FindInstanceGetterByName(*mirrorClassDef, names[0]);
        CJC_ASSERT_WITH_MSG(getter != nullptr || !IsDirectPatchableMirrorField(*mirrorClassDef, names[0]),
            "CJMP mirror direct field has no matching getter");
        if (!getter) return nullptr;

        auto loc = load.GetDebugLocation();
        return CreateGetterCall(loc, baseValue, getter, load.GetParentBlock());
    }

    return nullptr;
}

Expression* CHIRFieldReferencePatcher::TryPatchLoad(Load& load, ClassDef& parentDef)
{
    auto location = load.GetLocation();
    auto localVar = DynamicCast<LocalVar*>(location);
    if (!localVar) return nullptr;

    auto getRef = DynamicCast<GetElementRef*>(localVar->GetExpr());
    if (!getRef) return nullptr;

    auto path = getRef->GetPath();
    if (path.empty()) return nullptr;
    auto fieldIdx = path[0];

    auto allFields = parentDef.GetAllInstanceVars();
    if (fieldIdx >= allFields.size()) return nullptr;
    auto& field = allFields[fieldIdx];

    if (!IsJavaMirrorField(field, mirrorAttr)) return nullptr;

    auto getter = FindGetterForField(parentDef, field, mirrorAttr);
    if (!getter) return nullptr;

    auto baseValue = getRef->GetBase();
    auto loc = load.GetDebugLocation();
    return CreateGetterCall(loc, baseValue, getter, load.GetParentBlock());
}

Expression* CHIRFieldReferencePatcher::TryPatchLoadByName(Load& load, ClassDef& parentDef)
{
    auto location = load.GetLocation();
    auto localVar = DynamicCast<LocalVar*>(location);
    if (!localVar) return nullptr;

    auto getByName = DynamicCast<GetElementByName*>(localVar->GetExpr());
    if (!getByName) return nullptr;

    auto names = getByName->GetNames();
    if (names.size() != 1) return nullptr;

    auto getter = FindInstanceGetterByName(parentDef, names[0]);
    CJC_ASSERT_WITH_MSG(getter != nullptr || !IsDirectPatchableMirrorField(parentDef, names[0]),
        "CJMP mirror direct field has no matching getter");
    if (!getter) return nullptr;

    auto baseValue = getByName->GetLocation();
    auto loc = load.GetDebugLocation();
    return CreateGetterCall(loc, baseValue, getter, load.GetParentBlock());
}

Expression* CHIRFieldReferencePatcher::TryPatchStoreElementByName(StoreElementByName& store, ClassDef& parentDef)
{
    auto names = store.GetNames();
    if (names.size() != 1) return nullptr;

    auto setter = FindInstanceSetterByName(parentDef, names[0]);
    CJC_ASSERT_WITH_MSG(setter != nullptr || !IsDirectPatchableMirrorField(parentDef, names[0]),
        "CJMP mirror direct field has no matching setter");
    if (!setter) return nullptr;

    auto baseValue = store.GetLocation();
    auto valueToStore = store.GetValue();
    auto loc = store.GetDebugLocation();
    return CreateSetterCall(loc, baseValue, valueToStore, setter, store.GetParentBlock());
}

Expression* CHIRFieldReferencePatcher::TryPatchFieldByName(FieldByName& fieldExpr, ClassDef& parentDef)
{
    auto names = fieldExpr.GetNames();
    if (names.size() != 1) return nullptr;

    auto getter = FindInstanceGetterByName(parentDef, names[0]);
    CJC_ASSERT_WITH_MSG(getter != nullptr || !IsDirectPatchableMirrorField(parentDef, names[0]),
        "CJMP mirror direct field has no matching getter");
    if (!getter) return nullptr;

    auto baseValue = fieldExpr.GetBase();
    auto loc = fieldExpr.GetDebugLocation();
    return CreateGetterCall(loc, baseValue, getter, fieldExpr.GetParentBlock());
}

Expression* CHIRFieldReferencePatcher::TryPatchStoreElementRef(StoreElementRef& store, ClassDef& parentDef)
{
    auto baseValue = store.GetBase();
    auto valueToStore = store.GetValue();
    auto path = store.GetPath();

    if (path.empty()) return nullptr;
    auto fieldIdx = path[0];

    auto allFields = parentDef.GetAllInstanceVars();
    if (fieldIdx >= allFields.size()) return nullptr;
    auto& field = allFields[fieldIdx];

    if (!IsJavaMirrorField(field, mirrorAttr)) return nullptr;

    auto setter = FindSetterForField(parentDef, field, mirrorAttr);
    if (!setter) return nullptr;

    auto loc = store.GetDebugLocation();
    return CreateSetterCall(loc, baseValue, valueToStore, setter, store.GetParentBlock());
}

Expression* CHIRFieldReferencePatcher::TryPatchField(Field& fieldExpr, ClassDef& parentDef)
{
    auto baseValue = fieldExpr.GetBase();
    auto path = fieldExpr.GetPath();

    if (path.empty()) return nullptr;
    auto fieldIdx = path[0];

    auto allFields = parentDef.GetAllInstanceVars();
    if (fieldIdx >= allFields.size()) return nullptr;
    auto& field = allFields[fieldIdx];

    if (!IsJavaMirrorField(field, mirrorAttr)) return nullptr;

    auto getter = FindGetterForField(parentDef, field, mirrorAttr);
    if (!getter) return nullptr;

    auto loc = fieldExpr.GetDebugLocation();
    return CreateGetterCall(loc, baseValue, getter, fieldExpr.GetParentBlock());
}

Expression* CHIRFieldReferencePatcher::CreateGetterCall(
    const DebugLocation& loc, Value* baseValue, Function* getter, Block* parentBlock)
{
    auto returnType = getter->GetReturnType();

    std::vector<Value*> args = {baseValue};

    auto funcType = getter->GetFuncType();
    Type* thisType = nullptr;
    if (funcType && !funcType->GetParamTypes().empty()) {
        thisType = funcType->GetParamTypes()[0];
    }

    FuncCallContext ctx{.args = args, .thisType = thisType};

    return builder.CreateExpression<Apply>(loc, returnType, getter, ctx, parentBlock);
}

Expression* CHIRFieldReferencePatcher::CreateSetterCall(
    const DebugLocation& loc, Value* baseValue, Value* valueToStore, Function* setter, Block* parentBlock)
{
    std::vector<Value*> args = {baseValue, valueToStore};

    auto funcType = setter->GetFuncType();
    Type* thisType = nullptr;
    if (funcType && !funcType->GetParamTypes().empty()) {
        thisType = funcType->GetParamTypes()[0];
    }

    FuncCallContext ctx{.args = args, .thisType = thisType};

    return builder.CreateExpression<Apply>(loc, builder.GetUnitTy(), setter, ctx, parentBlock);
}

Expression* CHIRFieldReferencePatcher::CreateStaticGetterCall(
    const DebugLocation& loc, Function* getter, Block* parentBlock)
{
    auto returnType = getter->GetReturnType();

    auto calleeParentDef = getter->GetParentCustomTypeDef();
    Type* thisType = calleeParentDef ? calleeParentDef->GetType() : nullptr;

    FuncCallContext ctx{.args = {}, .thisType = thisType};

    return builder.CreateExpression<Apply>(loc, returnType, getter, ctx, parentBlock);
}

Expression* CHIRFieldReferencePatcher::CreateStaticSetterCall(
    const DebugLocation& loc, Value* valueToStore, Function* setter, Block* parentBlock)
{
    std::vector<Value*> args = {valueToStore};

    auto calleeParentDef = setter->GetParentCustomTypeDef();
    Type* thisType = calleeParentDef ? calleeParentDef->GetType() : nullptr;

    FuncCallContext ctx{.args = args, .thisType = thisType};

    return builder.CreateExpression<Apply>(loc, builder.GetUnitTy(), setter, ctx, parentBlock);
}

} // namespace Cangjie::CHIR