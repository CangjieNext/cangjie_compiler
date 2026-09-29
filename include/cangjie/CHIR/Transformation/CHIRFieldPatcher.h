// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
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
 * Helper that marks the CHIR instance fields of CJMP FFI classes whose field
 * access is redirected through JNI/ObjC getter/setter calls. A field with a
 * matching accessor (`$nameget`/`$nameset`) is marked INITIALIZED so
 * VarInitCheck does not report it as uninitialized even though its stores were
 * rewritten to setter calls. Compiler-added fields (javaref/reg/hasInited) are
 * left untouched. For mirrors, the field's storage is additionally collapsed to
 * Unit because all its accesses go through the accessors; for @JavaImpl only the
 * marker is set.
 */
class CHIRFieldPatcher {
public:
    /** Constructs a patcher bound to @p builder. */
    explicit CHIRFieldPatcher(CHIRBuilder& builder);

    /**
     * For each non-compiler-added direct instance field of @p classDef
     * (@JavaMirror/@ObjCMirror) with a matching `$nameget` accessor, marks the
     * field INITIALIZED and collapses its storage type to Unit, then writes the
     * updated field list back onto the ClassDef. Asserts when a genuine direct
     * mirror field has no accessor. Null @p classDef is a no-op.
     */
    void PatchCommonFieldsForMirror(ClassDef* classDef, Attribute mirrorAttr);

    /**
     * Same INITIALIZED marking as PatchCommonFieldsForMirror, but for @JavaImpl
     * wrapper classes. Mark-only: no assert (non-relocated
     * FROM_COMMON_PART/specific fields may legitimately lack an accessor) and no
     * Unit collapse. Null @p classDef is a no-op.
     */
    void PatchCommonFieldsForJavaImpl(ClassDef* classDef);

private:
    /** Returns the getter carrying @p mirrorAttr whose return type equals @p field's type, or null (Step 2). */
    Function* FindGetterForField(ClassDef* classDef, const MemberVarInfo& field, Attribute mirrorAttr);

    /** Returns true for fields that must be left untouched (compiler-added/internal). */
    bool IsFFIAddedField(const MemberVarInfo& var);

    /** Marks @p var INITIALIZED. */
    void MarkFieldInitialized(MemberVarInfo& var);

    /** Collapses @p var's storage type to Unit (its accesses go through accessors). */
    void PatchFieldToUnit(MemberVarInfo& var);

    CHIRBuilder& builder;
};

} // namespace Cangjie::CHIR

#endif // CANGJIE_CHIR_TRANSFORMATION_CHIR_FIELD_PATCHER_H
