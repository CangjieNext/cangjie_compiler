// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * Computes source-level unreachable CFG regions before DCE removes them.
 */

#include "cangjie/CHIR/Analysis/UnreachableBlockAnalysis.h"
#include "cangjie/CHIR/IR/Expression/Expression.h"
#include "cangjie/CHIR/Utils/Visitor/Visitor.h"
#include "cangjie/Utils/CheckUtils.h"

#include <algorithm>
#include <iterator>
#include <queue>
#include <unordered_set>

namespace Cangjie::CHIR {
namespace {

/// Return whether `location` is contained in the non-empty, file-local `range`.
bool Contains(const DebugLocation& range, const DebugLocation& location)
{
    if (range.GetFileID() != location.GetFileID()) {
        return false;
    }
    auto begin = range.GetBeginPos();
    auto end = range.GetEndPos();
    auto position = location.GetBeginPos();
    if (begin.IsZero() || end.IsZero() || position.IsZero()) {
        return false;
    }
    auto afterBegin = position.line > begin.line ||
        (position.line == begin.line && position.column >= begin.column);
    auto beforeEnd = position.line < end.line ||
        (position.line == end.line && position.column <= end.column);
    return afterBegin && beforeEnd;
}

/// Return whether any expression in `region` covers `location`.
bool Contains(const UnreachableBlockRegion& region, const DebugLocation& location)
{
    for (auto block : region.blocks) {
        for (auto expression : block->GetExpressions()) {
            if (Contains(expression->GetDebugLocation(), location)) {
                return true;
            }
        }
    }
    return false;
}

/*
 * Reachability is intentionally computed from the raw successor edges. In
 * particular, this pass must see the continuation edges that DCE may later
 * remove; helpers that treat Attribute::UNREACHABLE as an optimization hint
 * would hide precisely the regions this analysis reports.
 */
std::unordered_set<const Block*> CollectReachableBlocks(const BlockGroup& body)
{
    std::unordered_set<const Block*> reachable;
    std::queue<const Block*> workList;
    workList.push(body.GetEntryBlock());
    while (!workList.empty()) {
        auto block = workList.front();
        workList.pop();
        if (!reachable.emplace(block).second) {
            continue;
        }
        for (auto successor : block->GetSuccessors()) {
            workList.push(successor);
        }
    }
    return reachable;
}

/// Return the blocks owned by `body` that were not found by the entry traversal.
std::unordered_set<const Block*> CollectUnreachableBlocks(
    const std::vector<Block*>& blocks, const std::unordered_set<const Block*>& reachable)
{
    std::unordered_set<const Block*> unreachableBlocks;
    for (auto block : blocks) {
        if (reachable.find(block) == reachable.end()) {
            unreachableBlocks.emplace(block);
        }
    }
    return unreachableBlocks;
}

/*
 * Grouping follows both predecessor and successor edges. This treats a loop as
 * one unreachable component even when no block in the loop has an incoming edge
 * from another component. The seed is an arbitrary remaining unreachable block;
 * `unreachableBlocks` is consumed as the component is visited, so every
 * unreachable block is assigned exactly once.
 */
std::unordered_set<const Block*> CollectComponent(
    std::unordered_set<const Block*>& unreachableBlocks)
{
    std::queue<const Block*> workList;
    auto root = *unreachableBlocks.begin();
    std::unordered_set<const Block*> component{root};
    workList.push(root);
    unreachableBlocks.erase(root);
    while (!workList.empty()) {
        auto block = workList.front();
        workList.pop();
        for (auto successor : block->GetSuccessors()) {
            if (unreachableBlocks.erase(successor) != 0) {
                component.emplace(successor);
                workList.push(successor);
            }
        }
        for (auto predecessor : block->GetPredecessors()) {
            if (unreachableBlocks.erase(predecessor) != 0) {
                component.emplace(predecessor);
                workList.push(predecessor);
            }
        }
    }
    return component;
}

/*
 * A component with an incoming edge from outside itself has a natural root: a
 * block whose predecessors all lie outside the component. A pure cycle has none
 * and is skipped because it has no unique source entry to report.
 */
const Block* FindRegionRoot(const std::unordered_set<const Block*>& component)
{
    for (auto block : component) {
        const auto& predecessors = block->GetPredecessors();
        auto inComponent = [&component](auto predecessor) {
            return component.count(predecessor) != 0;
        };
        if (std::none_of(predecessors.begin(), predecessors.end(), inComponent)) {
            return block;
        }
    }
    return nullptr;
}

/*
 * Continuation blocks are often synthesized without their own source range. In
 * that case use the nearest preceding terminator, which is the source expression
 * that transferred control into the continuation.
 */
DebugLocation GetRootLocation(const std::vector<Block*>& blocks, const Block& root)
{
    auto rootLocation = root.GetDebugLocation();
    if (!rootLocation.GetBeginPos().IsZero()) {
        return rootLocation;
    }
    auto rootIter = std::find(blocks.begin(), blocks.end(), &root);
    while (rootIter != blocks.begin()) {
        --rootIter;
        if (auto terminator = (*rootIter)->GetTerminator()) {
            rootLocation = terminator->GetDebugLocation();
            if (!rootLocation.GetBeginPos().IsZero()) {
                break;
            }
        }
    }
    return rootLocation;
}

/*
 * Build a single source-level region for `root`. The root's debug location is
 * resolved through GetRootLocation because continuation blocks are often
 * synthesized without a source range of their own.
 */
 UnreachableBlockRegion CreateRegion(const BlockGroup& body, const Block& root)
 {
     auto rootLocation = GetRootLocation(body.GetBlocks(), root);
     return UnreachableBlockRegion{&body, {&root}, rootLocation};
 }
 
 /*
  * Merge `region` into `regions`. Two regions are merged when an existing
  * region's expressions already cover the new root location. Otherwise the new
  * region is appended as-is.
  *
  * `regions` is taken by value so callers can use the return value directly;
  * the container holds pointers and is cheap to move.
  */
 UnreachableBlockRegions MergeRegion(
     UnreachableBlockRegions regions, const UnreachableBlockRegion& region)
 {
     auto merged = std::find_if(regions.begin(), regions.end(),
         [&region](const auto& existing) { return Contains(existing, region.rootLocation); });
     if (merged != regions.end()) {
         merged->blocks.insert(merged->blocks.end(),
             region.blocks.begin(), region.blocks.end());
     } else {
         regions.push_back(region);
     }
     return regions;
 }
 
 /*
  * Analyze a block group in three stages: compute entry reachability, split the
  * complement into CFG components, and emit one source-level region per
  * component. Components are visited until `unreachableBlocks` is drained;
  * CollectComponent consumes the blocks it visits, so each block is assigned to
  * exactly one component.
  */
 UnreachableBlockRegions AnalyzeBody(const BlockGroup& body)
 {
     auto blocks = body.GetBlocks();
     UnreachableBlockRegions result;
     auto unreachableBlocks = CollectUnreachableBlocks(blocks, CollectReachableBlocks(body));
     while (!unreachableBlocks.empty()) {
         auto component = CollectComponent(unreachableBlocks);
         // A pure cycle has no unique entry to report; skip it.
         auto regionRoot = FindRegionRoot(component);
         if (regionRoot == nullptr) {
             continue;
         }
         result = MergeRegion(std::move(result), CreateRegion(body, *regionRoot));
     }
     return result;
 }

} // namespace

/* Visit every function and nested lambda block group. The visitor is used so the
 * same CFG analysis applies to ordinary functions and generated closures. */
UnreachableBlockRegions UnreachableBlockAnalysis::Parse() const
{
    UnreachableBlockRegions result;
    for (auto func : package->GetGlobalFuncsWithBody()) {
        Visitor::Visit(*func, [&result](BlockGroup& body) {
            auto regions = AnalyzeBody(body);
            result.insert(result.end(), std::make_move_iterator(regions.begin()),
                std::make_move_iterator(regions.end()));
            return VisitResult::CONTINUE;
        });
    }
    return result;
}

} // namespace Cangjie::CHIR
