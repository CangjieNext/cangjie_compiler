// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_CHIR_FIELD_REFERENCE_PATCHER_H
#define CANGJIE_CHIR_FIELD_REFERENCE_PATCHER_H

#include "cangjie/CHIR/IR/CHIRBuilder.h"
#include "cangjie/CHIR/IR/Package.h"
#include "cangjie/CHIR/IR/AttributeInfo.h"
#include "cangjie/CHIR/Utils/JavaMirrorFieldHelper.h"

namespace Cangjie::CHIR {

class CHIRFieldReferencePatcher {
public:
    CHIRFieldReferencePatcher(Package& pkg, CHIRBuilder& builder, Attribute mirrorAttr);
    void Run();

private:
    Package& pkg;
    CHIRBuilder& builder;
    Attribute mirrorAttr;

    void PatchFunction(Function& func);
    void PatchGlobalFunction(Function& func);

    /** Rewrites a name-based member access (`Load(GetElementByName)`) to a getter call, or null. */
    Expression* TryPatchLoadByName(Load& load, ClassDef& parentDef);

    /** Rewrites a name-based member store (`StoreElementByName`) to a setter call, or null. */
    Expression* TryPatchStoreElementByName(StoreElementByName& store, ClassDef& parentDef);

    /** Rewrites a value-type member access (`FieldByName`) to a getter call, or null. */
    Expression* TryPatchFieldByName(FieldByName& fieldExpr, ClassDef& parentDef);

    /** Dispatches a single expression in a global function to the matching patch helper; null if unpatched. */
    Expression* TryPatchGlobalExpr(Expression& e);

    Expression* TryPatchLoad(Load& load, ClassDef& parentDef);
    Expression* TryPatchStoreElementRef(StoreElementRef& store, ClassDef& parentDef);
    Expression* TryPatchField(Field& fieldExpr, ClassDef& parentDef);
    Expression* TryPatchLoadGlobalVar(Load& load);
    Expression* TryPatchStoreGlobalVar(Store& store);
    Expression* TryPatchFieldGlobal(Field& fieldExpr);
    Expression* TryPatchLoadGlobal(Load& load);
    ClassDef* GetMirrorClassDefFromType(Type* ty);
    ClassDef* GetMirrorClassDefFromGlobalVar(GlobalVar* gv);
    Expression* CreateGetterCall(const DebugLocation& loc, Value* baseValue, Function* getter, Block* parentBlock);
    Expression* CreateSetterCall(
        const DebugLocation& loc, Value* baseValue, Value* valueToStore, Function* setter, Block* parentBlock);
    Expression* CreateStaticGetterCall(const DebugLocation& loc, Function* getter, Block* parentBlock);
    Expression* CreateStaticSetterCall(
        const DebugLocation& loc, Value* valueToStore, Function* setter, Block* parentBlock);
};

} // namespace Cangjie::CHIR
#endif