// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_CHIR_ANALYSIS_UNREACHABLEBLOCKANALYSIS_H
#define CANGJIE_CHIR_ANALYSIS_UNREACHABLEBLOCKANALYSIS_H

#include "cangjie/Basic/DiagnosticEngine.h"
#include "cangjie/CHIR/IR/Package.h"

#include <string>
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

/**
 * Computes unreachable CFG regions and emits source diagnostics.
 *
 * Each function, including nested block groups, is analyzed independently.
 * `RunOnPackage` can therefore process functions concurrently without locking
 * region storage. DiagnosticEngine may be called from multiple threads.
 */
class UnreachableBlockAnalysis {
public:
    /// The diagnostic engine and package are borrowed for the duration of RunOnPackage().
    explicit UnreachableBlockAnalysis(DiagnosticEngine& diag, const Package& package);

    /**
     * Analyze every function with a body and report unreachable source regions.
     *
     * The analysis observes the current CFG and does not remove blocks. Warnings
     * are emitted before DCE changes the CFG.
     */
    void RunOnPackage(size_t threadsNum);

private:
    void RunOnFunc(Function& func);

    DiagnosticEngine& diag;
    const Package& package;
};

} // namespace Cangjie::CHIR

#endif
