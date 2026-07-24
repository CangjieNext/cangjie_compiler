// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_CHIR_ANALYSIS_UNREACHABLE_BLOCK_REGION_H
#define CANGJIE_CHIR_ANALYSIS_UNREACHABLE_BLOCK_REGION_H

#include "cangjie/CHIR/IR/Expression/Terminator.h"
#include "cangjie/CHIR/IR/UnreachableBlockInfo.h"
#include "cangjie/CHIR/IR/Value/Value.h"

#include <algorithm>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace Cangjie::CHIR {

using UnreachableBlockRegionMap = std::unordered_map<Block*, Block*>;

inline Block* GetUnreachableBlockRoot(Block& block, const UnreachableBlockMap& maybeUnreachableBlocks)
{
    Block* root = &block;
    while (true) {
        auto it = maybeUnreachableBlocks.find(root);
        CJC_ASSERT(it != maybeUnreachableBlocks.end());
        if (it->second.sourceRoot != nullptr) {
            return it->second.sourceRoot;
        }
        if (it->second.startsNewRoot) {
            return root;
        }

        auto sourceBlock = it->second.terminator->GetParentBlock();
        if (sourceBlock == nullptr || maybeUnreachableBlocks.find(sourceBlock) == maybeUnreachableBlocks.end()) {
            return root;
        }
        root = sourceBlock;
    }
}

inline UnreachableBlockRegionMap GetUnreachableBlockRegions(
    const BlockGroup& body, const UnreachableBlockMap& maybeUnreachableBlocks)
{
    auto blocks = body.GetAllBlocks();
    std::unordered_set<Block*> blocksInBody(blocks.begin(), blocks.end());
    std::vector<Block*> continuations;
    UnreachableBlockRegionMap roots;
    for (auto block : blocks) {
        auto it = maybeUnreachableBlocks.find(block);
        if (it == maybeUnreachableBlocks.end()) {
            continue;
        }
        continuations.emplace_back(block);
        roots.emplace(block, GetUnreachableBlockRoot(*block, maybeUnreachableBlocks));
    }

    const auto buildRegions = [&blocksInBody, &continuations, &roots]() {
        UnreachableBlockRegionMap regions;
        std::queue<Block*> workList;
        for (auto block : continuations) {
            regions.emplace(block, roots.at(block));
            workList.push(block);
        }
        while (!workList.empty()) {
            auto block = workList.front();
            workList.pop();
            auto root = regions.at(block);
            for (auto successor : block->GetSuccessors()) {
                if (blocksInBody.find(successor) == blocksInBody.end() || regions.find(successor) != regions.end()) {
                    continue;
                }
                auto predecessors = successor->GetPredecessors();
                bool isRootOwned =
                    std::all_of(predecessors.begin(), predecessors.end(), [&regions, root](Block* predecessor) {
                        auto it = regions.find(predecessor);
                        return it != regions.end() && it->second == root;
                    });
                if (isRootOwned) {
                    regions.emplace(successor, root);
                    workList.push(successor);
                }
            }
        }
        return regions;
    };

    // A nested Nothing continuation has no CFG edge from its terminating block. Let it inherit the owner of that
    // block, then rebuild until all continuation roots reflect their enclosing source-level region.
    for (size_t iteration = 0; iteration <= continuations.size(); ++iteration) {
        auto regions = buildRegions();
        bool changed = false;
        for (auto block : continuations) {
            auto it = maybeUnreachableBlocks.find(block);
            if (it->second.startsNewRoot) {
                continue;
            }
            auto sourceBlock = it->second.terminator->GetParentBlock();
            auto sourceRegion = regions.find(sourceBlock);
            if (sourceRegion != regions.end() && roots.at(block) != sourceRegion->second) {
                roots[block] = sourceRegion->second;
                changed = true;
            }
        }
        if (!changed) {
            return regions;
        }
    }
    return buildRegions();
}
} // namespace Cangjie::CHIR

#endif
