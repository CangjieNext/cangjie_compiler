// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * Orchestrate de-virtualization info collection into DevirtualizationInfo.
 */

#include "cangjie/CHIR/Analysis/DevirtualizationInfo.h"

namespace Cangjie::CHIR {

void DevirtualizationInfo::Collect()
{
    ReturnTypeMapCollector{package}.Collect(returnTypeMap);
    InheritanceInfoCollector{package}.Collect(subtypeMap, defsMap);
    ConstMemberVarCollector{package, constMemberTypeMap}.CollectConstMemberVarType();
}

} // namespace Cangjie::CHIR
