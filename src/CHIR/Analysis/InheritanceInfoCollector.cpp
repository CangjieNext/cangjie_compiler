// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/Analysis/InheritanceInfoCollector.h"

#include "cangjie/CHIR/IR/Type/ExtendDef.h"
#include "cangjie/CHIR/Utils/Utils.h"
#include "cangjie/Modules/ModulesUtils.h"
#include "cangjie/Utils/Casting.h"

namespace Cangjie::CHIR {

bool InheritanceInfoCollector::CheckCustomTypeInternal(const CustomTypeDef& def) const
{
    auto relation = Modules::GetPackageRelation(def.GetPackageName(), package->GetName());
    if (def.TestAttr(Attribute::PUBLIC) || def.TestAttr(Attribute::PROTECTED)) {
        // When building an executable, external packages cannot add subtypes.
        if (opts.CompileExecutable() && relation == Modules::PackageRelation::SAME_PACKAGE) {
            return true;
        }
        return false;
    }
    if (def.TestAttr(Attribute::PRIVATE)) {
        return true;
    }
    if (def.TestAttr(Attribute::INTERNAL)) {
        if (relation != Modules::PackageRelation::CHILD && relation != Modules::PackageRelation::SAME_PACKAGE) {
            return true;
        }
        if (opts.noSubPkg) {
            return true;
        }
        return false;
    }
    return false;
}

void InheritanceInfoCollector::Collect(SubTypeMap& subtypeMap, DefsMap& defsMap) const
{
    for (const auto customTypeDef : package->GetAllCustomTypeDef()) {
        if (customTypeDef->TestAttr(Attribute::SKIP_ANALYSIS)) {
            continue;
        }
        if (customTypeDef->IsClassLike() && IsClosureConversionEnvClass(*StaticCast<const ClassDef*>(customTypeDef))) {
            continue;
        }

        Type* thisType = customTypeDef->GetType();
        if (customTypeDef->IsExtend()) {
            thisType = StaticCast<ExtendDef*>(customTypeDef)->GetExtendedType();
            if (auto customTy = DynamicCast<CustomType*>(thisType)) {
                thisType = customTy->GetCustomTypeDef()->GetType();
            }
        }

        defsMap[thisType].emplace_back(customTypeDef);
        for (auto parentTy : customTypeDef->GetSuperTypesInCurDef()) {
            auto parentDef = parentTy->GetClassDef();
            if (IsCoreObject(*parentDef)) {
                continue;
            }
            if (CheckCustomTypeInternal(*parentDef)) {
                subtypeMap[parentDef].emplace_back(InheritanceInfo{parentTy, thisType});
            }
        }
    }
}

} // namespace Cangjie::CHIR
