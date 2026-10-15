// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/IR/Type/StructDef.h"

#include "cangjie/CHIR/IR/Type/ExtendDef.h"
#include "cangjie/CHIR/Utils/CHIRCasting.h"
#include "cangjie/CHIR/Utils/Utils.h"

using namespace Cangjie::CHIR;

void StructDef::SetCStruct(bool value)
{
    isC = value;
}

bool StructDef::IsCStruct() const
{
    return isC;
}

void StructDef::SetType(CustomType& ty)
{
    CJC_ASSERT(ty.GetTypeKind() == Type::TypeKind::TYPE_STRUCT);
    type = &ty;
}

StructType* StructDef::GetType() const
{
    return StaticCast<StructType>(type);
}

std::string StructDef::AddExtraComment() const
{
    return isC ? "isCStruct" : "";
}

bool StructDef::IsCopyable() const
{
    // @C structs are implicitly Copyable (local-mode spec).
    if (IsCStruct()) {
        return true;
    }
    auto isCopyableIface = [](ClassDef* def) {
        return IsExpectedCustomType(*def->GetType(), CORE_PACKAGE_NAME, std::string(COPY_NAME));
    };
    for (auto def : GetImplementedInterfaceDefs()) {
        if (isCopyableIface(def)) {
            return true;
        }
    }
    for (auto extend : GetExtends()) {
        for (auto def : extend->GetImplementedInterfaceDefs()) {
            if (isCopyableIface(def)) {
                return true;
            }
        }
    }
    return false;
}