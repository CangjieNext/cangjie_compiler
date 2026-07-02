// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_CHIR_TRANSFORMATION_CHIR_FIELD_PATCHER_H
#define CANGJIE_CHIR_TRANSFORMATION_CHIR_FIELD_PATCHER_H

#include "cangjie/CHIR/IR/Type/CustomTypeDef.h"
#include "cangjie/CHIR/IR/AttributeInfo.h"
#include "cangjie/CHIR/IR/CHIRBuilder.h"

namespace Cangjie::CHIR {

class CHIRFieldPatcher {
public:
    explicit CHIRFieldPatcher(CHIRBuilder& builder);

    void PatchCommonFieldsForMirror(ClassDef* classDef, Attribute mirrorAttr);
    void PatchCommonFieldsForJavaImpl(ClassDef* classDef);

private:
    Function* FindGetterForField(ClassDef* classDef, const MemberVarInfo& field, Attribute mirrorAttr);
    bool IsFFIAddedField(const MemberVarInfo& var);
    void PatchFieldToUnit(MemberVarInfo& var);

    CHIRBuilder& builder;
};

} // namespace Cangjie::CHIR

#endif // CANGJIE_CHIR_TRANSFORMATION_CHIR_FIELD_PATCHER_H