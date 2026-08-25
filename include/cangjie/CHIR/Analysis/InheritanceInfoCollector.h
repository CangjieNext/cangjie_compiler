// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * Collect parent-child inheritance relationships for de-virtualization.
 */

#ifndef CANGJIE_CHIR_ANALYSIS_INHERITANCE_INFO_COLLECTOR_H
#define CANGJIE_CHIR_ANALYSIS_INHERITANCE_INFO_COLLECTOR_H

#include <unordered_map>
#include <vector>

#include "cangjie/CHIR/IR/Package.h"
#include "cangjie/CHIR/IR/Type/ClassDef.h"
#include "cangjie/CHIR/IR/Type/Type.h"

namespace Cangjie::CHIR {

class InheritanceInfoCollector {
public:
    struct InheritanceInfo {
        ClassType* parentType;
        Type* subType;
    };

    using SubTypeMap = std::unordered_map<ClassDef*, std::vector<InheritanceInfo>>;
    using DefsMap = std::unordered_map<const Type*, std::vector<CustomTypeDef*>>;

    explicit InheritanceInfoCollector(const Package* package) : package(package)
    {
    }

    /// Record all parent→child edges visible in this package (no closed-world filter).
    void Collect(SubTypeMap& subtypeMap, DefsMap& defsMap) const;

private:
    const Package* package;
};

} // namespace Cangjie::CHIR

#endif
