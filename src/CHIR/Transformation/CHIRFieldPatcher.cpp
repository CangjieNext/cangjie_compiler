// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/Transformation/CHIRFieldPatcher.h"
#include "cangjie/CHIR/Utils/JavaMirrorFieldHelper.h"
#include "cangjie/Utils/CheckUtils.h"

namespace Cangjie::CHIR {

CHIRFieldPatcher::CHIRFieldPatcher(CHIRBuilder& builder) : builder(builder) {}

void CHIRFieldPatcher::PatchCommonFieldsForMirror(ClassDef* classDef, Attribute mirrorAttr)
{
    if (!classDef) {
        return;
    }
    (void)mirrorAttr;

    auto vars = classDef->GetDirectInstanceVars();
    for (auto& var : vars) {
        if (IsFFIAddedField(var)) {
            continue;
        }

        if (FindInstanceGetterByName(*classDef, var.name)) {
            // Step 2: the field's accesses are redirected to its getter/setter, so its
            // storage is unused; collapse it to Unit. INITIALIZED is kept so VarInitCheck
            // still treats the field as initialized.
            MarkFieldInitialized(var);
            PatchFieldToUnit(var);
            continue;
        }
        CJC_ASSERT_WITH_MSG(!IsPatchableMirrorField(var),
            "CJMP mirror direct field has no matching getter");
    }
    classDef->SetDirectInstanceVars(vars);
}

Function* CHIRFieldPatcher::FindGetterForField(ClassDef* classDef, const MemberVarInfo& field, Attribute mirrorAttr)
{
    for (auto* method : classDef->GetMethods()) {
        if (method->GetFuncKind() != FuncKind::GETTER) {
            continue;
        }
        if (!method->TestAttr(mirrorAttr)) {
            continue;
        }
        auto retType = method->GetReturnType();
        if (retType && *retType == *field.type) {
            return method;
        }
    }
    return nullptr;
}

bool CHIRFieldPatcher::IsFFIAddedField(const MemberVarInfo& var)
{
    return !IsPatchableMirrorField(var);
}

void CHIRFieldPatcher::PatchFieldToUnit(MemberVarInfo& var)
{
    var.type = builder.GetUnitTy();
}

void CHIRFieldPatcher::MarkFieldInitialized(MemberVarInfo& var)
{
    var.attributeInfo.SetAttr(Attribute::INITIALIZED, true);
}

void CHIRFieldPatcher::PatchCommonFieldsForJavaImpl(ClassDef* classDef)
{
    if (!classDef) {
        return;
    }

    auto vars = classDef->GetDirectInstanceVars();
    for (auto& var : vars) {
        if (IsFFIAddedField(var)) {
            continue;
        }

        if (FindInstanceGetterByName(*classDef, var.name)) {
            MarkFieldInitialized(var);
        }
    }
    classDef->SetDirectInstanceVars(vars);
}

} // namespace Cangjie::CHIR