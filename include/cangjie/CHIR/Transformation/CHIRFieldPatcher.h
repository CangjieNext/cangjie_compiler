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

/**
 * Helper that rewrites the CHIR instance-field representation of CJMP FFI
 * classes whose field access is redirected through JNI/ObjC getter/setter
 * calls. For fields that have a matching mirror/impl getter, the field's
 * storage type is collapsed to Unit so the CHIR field no longer carries a
 * stale type; the real type is carried by the getter/setter Apply calls that
 * CHIRFieldReferencePatcher patches separately. Compiler-added fields
 * (javaref, hasInited) are left untouched.
 */
class CHIRFieldPatcher {
public:
    /** Constructs a patcher bound to @p builder, used for creating the Unit type. */
    explicit CHIRFieldPatcher(CHIRBuilder& builder);

    /**
     * For each non-compiler-added instance field of @p classDef carrying
     * @p mirrorAttr (@JavaMirror/@ObjCMirror), collapses the field's type to
     * Unit when a matching mirror getter exists, then writes the updated field
     * list back onto the ClassDef. Null @p classDef is a no-op.
     */
    void PatchCommonFieldsForMirror(ClassDef* classDef, Attribute mirrorAttr);

    /**
     * Same field-to-Unit collapse as PatchCommonFieldsForMirror, but for
     * @JavaImpl classes, matching the getter on the JAVA_IMPL attribute. Null
     * @p classDef is a no-op.
     */
    void PatchCommonFieldsForJavaImpl(ClassDef* classDef);

private:
    /** Returns the getter carrying @p mirrorAttr whose return type equals @p field's type, or null. */
    Function* FindGetterForField(ClassDef* classDef, const MemberVarInfo& field, Attribute mirrorAttr);

    /** Returns true for compiler-added fields (javaref/hasInited) that must be left untouched. */
    bool IsFFIAddedField(const MemberVarInfo& var);

    /** Collapses @p var's storage type to Unit. */
    void PatchFieldToUnit(MemberVarInfo& var);

    CHIRBuilder& builder;
};

} // namespace Cangjie::CHIR

#endif // CANGJIE_CHIR_TRANSFORMATION_CHIR_FIELD_PATCHER_H
