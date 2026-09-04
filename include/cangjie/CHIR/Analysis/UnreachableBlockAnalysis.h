// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.

#ifndef CANGJIE_CHIR_ANALYSIS_UNREACHABLEBLOCKANALYSIS_H
#define CANGJIE_CHIR_ANALYSIS_UNREACHABLEBLOCKANALYSIS_H

#include "cangjie/CHIR/IR/Package.h"

#include <vector>

namespace Cangjie::CHIR {

/**
 * @brief A CFG region that cannot be reached from its block group entry.
 */
struct UnreachableBlockRegion {
    /// Block group containing the region.
    const BlockGroup* owner;
    /// Blocks that belong to the unreachable region.
    std::vector<const Block*> blocks;
    /// Source location used as the root of the diagnostic.
    DebugLocation rootLocation;
};
using UnreachableBlockRegions = std::vector<UnreachableBlockRegion>;

class UnreachableBlockAnalysis {
public:
    explicit UnreachableBlockAnalysis(const Package* package) : package(package)
    {
    }

    /**
     * @brief Compute unreachable CFG regions from each function entry.
     */
    UnreachableBlockRegions Parse() const;

private:
    const Package* package;
};

} // namespace Cangjie::CHIR

#endif
