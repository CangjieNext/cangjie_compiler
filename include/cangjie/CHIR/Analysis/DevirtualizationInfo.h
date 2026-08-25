// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * Result data collected for the de-virtualization pass.
 */

#ifndef CANGJIE_CHIR_ANALYSIS_DEVIRTUALIZATION_INFO_H
#define CANGJIE_CHIR_ANALYSIS_DEVIRTUALIZATION_INFO_H

#include <unordered_map>

#include "cangjie/CHIR/Analysis/ConstMemberVarCollector.h"
#include "cangjie/CHIR/Analysis/InheritanceInfoCollector.h"
#include "cangjie/CHIR/Analysis/ReturnTypeMapCollector.h"
#include "cangjie/CHIR/IR/Package.h"
#include "cangjie/CHIR/IR/Type/ClassDef.h"
#include "cangjie/CHIR/IR/Type/Type.h"
#include "cangjie/CHIR/IR/Value/Value.h"

namespace Cangjie::CHIR {

/**
 * @brief Collected facts used by de-virtualization (return types, inheritance, const members).
 *
 * Collection is done by ReturnTypeMapCollector, InheritanceInfoCollector and ConstMemberVarCollector.
 * Closed-world policy lives in Devirtualization (IsSubtypeSetComplete), not here.
 */
struct DevirtualizationInfo {
    using InheritanceInfo = InheritanceInfoCollector::InheritanceInfo;
    using SubTypeMap = InheritanceInfoCollector::SubTypeMap;
    using DefsMap = InheritanceInfoCollector::DefsMap;
    using ReturnTypeMap = ReturnTypeMapCollector::ReturnTypeMap;
    using ConstMemberMapType = ConstMemberVarCollector::ConstMemberMapType;

    DevirtualizationInfo() = delete;

    explicit DevirtualizationInfo(const Package* package) : package(package)
    {
    }

    /// Run the three collectors and fill the result fields below.
    void Collect();

    ReturnTypeMap returnTypeMap;
    SubTypeMap subtypeMap;
    ConstMemberMapType constMemberTypeMap;
    DefsMap defsMap;

private:
    const Package* package;
};

} // namespace Cangjie::CHIR

#endif
