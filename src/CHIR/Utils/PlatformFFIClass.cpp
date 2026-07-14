// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/Utils/PlatformFFIClass.h"

#include "cangjie/CHIR/IR/Type/ClassDef.h"
#include "cangjie/CHIR/Utils/Utils.h"

namespace Cangjie::CHIR {

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
    return IsCoreObject(*superClass);
}

} // namespace Cangjie::CHIR
