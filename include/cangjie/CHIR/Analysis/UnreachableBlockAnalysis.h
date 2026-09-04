// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.

#ifndef CANGJIE_CHIR_ANALYSIS_UNREACHABLEBLOCKANALYSIS_H
#define CANGJIE_CHIR_ANALYSIS_UNREACHABLEBLOCKANALYSIS_H

#include "cangjie/CHIR/IR/Package.h"

#include <vector>

namespace Cangjie::CHIR {

struct UnreachableBlockRegion {
    const BlockGroup* owner;
    std::vector<const Block*> blocks;
    DebugLocation rootLocation;
};
using UnreachableBlockRegions = std::vector<UnreachableBlockRegion>;

class UnreachableBlockAnalysis {
public:
    // Compute unreachable CFG regions from each function entry.
    static UnreachableBlockRegions Parse(const Package& package);
};

} // namespace Cangjie::CHIR

#endif
