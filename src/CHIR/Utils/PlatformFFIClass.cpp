// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/Utils/PlatformFFIClass.h"

#include <string>
#include <string_view>

#include "cangjie/CHIR/IR/Type/ClassDef.h"

namespace Cangjie::CHIR {

/**
 * Itanium-mangled source-name encoding of the implicit Cangjie `Object` root
 * class: `6` is the length prefix, `Object` is the name, and `E` closes the
 * nested-name component. Matched as a substring of `ClassDef::GetIdentifier()`
 * to recognize classes whose direct superclass is `Object`.
 */
constexpr std::string_view kCangjieObjectMangledName = "6ObjectE";

bool IsJavaMirrorClass(const ClassDef& classDef)
{
    return classDef.TestAttr(Attribute::JAVA_MIRROR);
}

bool IsObjCMirrorClass(const ClassDef& classDef)
{
    return classDef.TestAttr(Attribute::OBJ_C_MIRROR);
}

bool IsPlatformMirrorClass(const ClassDef& classDef)
{
    return IsJavaMirrorClass(classDef) || IsObjCMirrorClass(classDef);
}

bool HasObjectAsSuperClass(const ClassDef& classDef)
{
    auto superClass = classDef.GetSuperClassDef();
    if (superClass == nullptr) {
        return true;  // classDef is Object itself (no superclass)
    }
    auto superId = superClass->GetIdentifier();
    return superId.find(kCangjieObjectMangledName) != std::string::npos;  // inherits from Object
}

} // namespace Cangjie::CHIR
