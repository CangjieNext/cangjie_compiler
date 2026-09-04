// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_CHIR_ANALYSIS_UNREACHABLEBLOCKANALYSIS_H
#define CANGJIE_CHIR_ANALYSIS_UNREACHABLEBLOCKANALYSIS_H

#include "cangjie/CHIR/IR/Package.h"

#include <vector>

namespace Cangjie::CHIR {

/**
 * A source-level unreachable region collected from one BlockGroup.
 *
 * The analysis groups CFG components by their source location. `blocks` therefore
 * contains the representative root blocks used for diagnostics, rather than every
 * block in the component. All pointers are non-owning and remain valid while the
 * package is being checked.
 */
struct UnreachableBlockRegion {
    /// BlockGroup that owns the representative blocks.
    const BlockGroup* owner;
    /// Representative roots belonging to the same source-level region.
    std::vector<const Block*> blocks;
    /// Source location of the expression that made the region unreachable.
    DebugLocation rootLocation;
};
using UnreachableBlockRegions = std::vector<UnreachableBlockRegion>;

class UnreachableBlockAnalysis {
public:
    /// The package is borrowed; it is only traversed by Parse().
    explicit UnreachableBlockAnalysis(const Package* package) : package(package)
    {
    }

    /**
     * Compute unreachable CFG regions from each function and nested block group.
     *
     * The analysis observes the current CFG and does not remove blocks or attach
     * diagnostic state to the package. The returned regions are consumed before
     * DCE changes the CFG.
     */
    UnreachableBlockRegions Parse() const;

private:
    const Package* package;
};

} // namespace Cangjie::CHIR

#endif
