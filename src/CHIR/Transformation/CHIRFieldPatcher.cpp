// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/Transformation/CHIRFieldPatcher.h"

namespace Cangjie::CHIR {

CHIRFieldPatcher::CHIRFieldPatcher(CHIRBuilder& builder) : builder(builder) {}

void CHIRFieldPatcher::PatchCommonFieldsForMirror(ClassDef* classDef, Attribute mirrorAttr)
{
    if (!classDef) {
        return;
    }

    auto vars = classDef->GetDirectInstanceVars();
    for (auto& var : vars) {
        if (IsFFIAddedField(var)) {
            continue;
        }

        Function* getter = FindGetterForField(classDef, var, mirrorAttr);
        if (getter) {
            PatchFieldToUnit(var);
        }
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
    return var.TestAttr(Attribute::COMPILER_ADD);
}

void CHIRFieldPatcher::PatchFieldToUnit(MemberVarInfo& var)
{
    var.type = builder.GetUnitTy();
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

        Function* getter = FindGetterForField(classDef, var, Attribute::JAVA_IMPL);
        if (getter) {
            PatchFieldToUnit(var);
        }
    }
    classDef->SetDirectInstanceVars(vars);
}

} // namespace Cangjie::CHIR