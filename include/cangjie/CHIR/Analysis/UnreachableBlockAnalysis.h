// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.

#ifndef CANGJIE_CHIR_ANALYSIS_UNREACHABLEBLOCKANALYSIS_H
#define CANGJIE_CHIR_ANALYSIS_UNREACHABLEBLOCKANALYSIS_H

#include "cangjie/CHIR/IR/Package.h"

#include <unordered_map>

namespace Cangjie::CHIR {

using UnreachableBlockMap = std::unordered_map<const Block*, DebugLocation>;

class UnreachableBlockAnalysis {
public:
    // Collect continuation blocks after AST2CHIR has built the CFG.
    static UnreachableBlockMap Parse(const Package& package);
};

} // namespace Cangjie::CHIR

#endif
