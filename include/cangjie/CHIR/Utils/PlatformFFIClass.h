// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_CHIR_PLATFORM_FFI_CLASS_H
#define CANGJIE_CHIR_PLATFORM_FFI_CLASS_H

#include "cangjie/CHIR/IR/Type/CustomTypeDef.h"
#include "cangjie/CHIR/IR/AttributeInfo.h"

namespace Cangjie::CHIR {

/**
 * Returns true if @p classDef is a @JavaMirror CHIR class (the Java side of a
 * Java FFI mirror/impl pair).
 */
bool IsJavaMirrorClass(const ClassDef& classDef);

/**
 * Returns true if @p classDef is an @ObjCMirror CHIR class (the ObjC side of an
 * ObjC FFI mirror/impl pair).
 */
bool IsObjCMirrorClass(const ClassDef& classDef);

/**
 * Returns true if @p classDef is a platform "mirror" FFI class, i.e. it carries
 * either @JavaMirror or @ObjCMirror. This is the CHIR-level predicate used by
 * CJMP super-class/super-call patching to decide whether a deserialized
 * FROM_COMMON_PART class needs its Object super repointed to the platform
 * parent (JObject / NSObject).
 */
bool IsPlatformMirrorClass(const ClassDef& classDef);

/**
 * Returns true if @p classDef is, or directly inherits from, the implicit
 * Cangjie `Object` root class — i.e. it has no explicit non-Object superclass.
 *
 * A class with no superclass is `Object` itself. Otherwise the immediate
 * superclass is matched against Object's mangled identifier. Used by
 * `PatchCJMPSuperClass` to decide whether the CHIR ClassDef super-class
 * reference must be repointed to the platform parent; classes that already
 * inherit from a real (non-Object) superclass are left untouched.
 */
bool HasObjectAsSuperClass(const ClassDef& classDef);

} // namespace Cangjie::CHIR
#endif // CANGJIE_CHIR_PLATFORM_FFI_CLASS_H
