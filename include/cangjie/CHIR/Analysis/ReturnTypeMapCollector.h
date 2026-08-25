// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * Collect functions whose runtime return type is more concrete than the declared one.
 */

#ifndef CANGJIE_CHIR_ANALYSIS_RETURN_TYPE_MAP_COLLECTOR_H
#define CANGJIE_CHIR_ANALYSIS_RETURN_TYPE_MAP_COLLECTOR_H

#include <unordered_map>

#include "cangjie/CHIR/IR/Package.h"
#include "cangjie/CHIR/IR/Type/Type.h"
#include "cangjie/CHIR/IR/Value/Value.h"

namespace Cangjie::CHIR {

class ReturnTypeMapCollector {
public:
    using ReturnTypeMap = std::unordered_map<Function*, Type*>;

    explicit ReturnTypeMapCollector(const Package* package) : package(package)
    {
    }

    /// Fill @p returnTypeMap with funcs that have a more concrete runtime return type.
    void Collect(ReturnTypeMap& returnTypeMap) const;

private:
    const Package* package;
};

} // namespace Cangjie::CHIR

#endif
