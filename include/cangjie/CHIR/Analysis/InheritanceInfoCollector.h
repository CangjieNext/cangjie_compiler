// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
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
#include "cangjie/Option/Option.h"

namespace Cangjie::CHIR {

class InheritanceInfoCollector {
public:
    struct InheritanceInfo {
        ClassType* parentType;
        Type* subType;
    };

    using SubTypeMap = std::unordered_map<ClassDef*, std::vector<InheritanceInfo>>;
    using DefsMap = std::unordered_map<const Type*, std::vector<CustomTypeDef*>>;

    InheritanceInfoCollector(const Package* package, const GlobalOptions& opts)
        : package(package), opts(opts)
    {
    }

    /// Fill subtype map and type-to-def map used by de-virtualization.
    void Collect(SubTypeMap& subtypeMap, DefsMap& defsMap) const;

    /// Whether @p def is internal to the current package (cannot be extended outside).
    bool CheckCustomTypeInternal(const CustomTypeDef& def) const;

private:
    const Package* package;
    const GlobalOptions opts;
};

} // namespace Cangjie::CHIR

#endif
