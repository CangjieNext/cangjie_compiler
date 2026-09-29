// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_CHIR_JAVAMIRROR_FIELD_HELPER_H
#define CANGJIE_CHIR_JAVAMIRROR_FIELD_HELPER_H

#include "cangjie/CHIR/IR/Type/CustomTypeDef.h"
#include "cangjie/CHIR/IR/Value/Value.h"
#include "cangjie/CHIR/IR/AttributeInfo.h"

namespace Cangjie::CHIR {

bool IsJavaMirrorField(const MemberVarInfo& field, Attribute mirrorAttr);

/**
 * True for a user-declared instance field whose accesses must be redirected to
 * an FFI accessor: excludes compiler-added fields (javaref/reg/hasInited) and
 * internal `$`-prefixed names. Used to decide whether a missing accessor is an
 * error (assert) and whether a field should be marked INITIALIZED.
 */
bool IsPatchableMirrorField(const MemberVarInfo& field);

/**
 * True when @p def declares a direct instance field named @p name that is
 * eligible for accessor rewriting (see IsPatchableMirrorField). Inherited
 * fields return false.
 */
bool IsDirectPatchableMirrorField(const ClassDef& def, const std::string& name);

Function* FindGetterForField(const ClassDef& def, const MemberVarInfo& field, Attribute mirrorAttr);

Function* FindSetterForField(const ClassDef& def, const MemberVarInfo& field, Attribute mirrorAttr);

Function* FindStaticGetterForGlobalVar(const ClassDef& def, const GlobalVar& globalVar);

Function* FindStaticSetterForGlobalVar(const ClassDef& def, const GlobalVar& globalVar);

Function* FindInstanceGetterByName(const ClassDef& def, const std::string& fieldName);

Function* FindInstanceSetterByName(const ClassDef& def, const std::string& fieldName);

} // namespace Cangjie::CHIR
#endif