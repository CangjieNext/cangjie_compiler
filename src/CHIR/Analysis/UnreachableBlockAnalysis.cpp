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

#include <algorithm>
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
    std::unordered_set<const Block*> remaining;
    for (auto block : blocks) {
        if (reachable.find(block) == reachable.end()) {
            remaining.emplace(block);
        }
    }
    return remaining;
}

/*
 * Grouping follows both predecessor and successor edges. This treats a loop as
 * one unreachable component even when no block in the loop has an incoming edge
 * from another component. `remaining` is consumed as the component is visited,
 * so every unreachable block is assigned exactly once.
 */
std::unordered_set<const Block*> CollectComponent(
    std::unordered_set<const Block*>& remaining, const Block* root)
{
    std::queue<const Block*> workList;
    std::unordered_set<const Block*> component{root};
    workList.push(root);
    remaining.erase(root);
    while (!workList.empty()) {
        auto block = workList.front();
        workList.pop();
        for (auto successor : block->GetSuccessors()) {
            if (remaining.erase(successor) != 0) {
                component.emplace(successor);
                workList.push(successor);
            }
        }
        for (auto predecessor : block->GetPredecessors()) {
            if (remaining.erase(predecessor) != 0) {
                component.emplace(predecessor);
                workList.push(predecessor);
            }
        }
    }
    return component;
}

/*
 * The block-group order gives diagnostics a stable representative. A component
 * with an incoming edge from outside itself has a natural root; a pure cycle has
 * none and is skipped because it has no unique source entry to report.
 */
const Block* FindRegionRoot(
    const std::vector<Block*>& blocks, const std::unordered_set<const Block*>& component)
{
    for (auto block : blocks) {
        if (component.find(block) == component.end()) {
            continue;
        }
        auto predecessors = block->GetPredecessors();
        auto hasComponentPredecessor = std::any_of(predecessors.begin(), predecessors.end(),
            [&component](auto predecessor) { return component.find(predecessor) != component.end(); });
        if (!hasComponentPredecessor) {
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
 * Several CFG components may be generated by one source expression (for example
 * after lowering a continuation). Merge such roots only when they belong to the
 * same block group and the existing region contains the new root location.
 */
void AddRegion(const BlockGroup& body, const Block& root, UnreachableBlockRegions& result)
{
    auto rootLocation = GetRootLocation(body.GetBlocks(), root);
    auto merged = std::find_if(result.begin(), result.end(), [&body, &rootLocation](const auto& region) {
        auto regionBegin = region.rootLocation.GetBeginPos();
        auto rootBegin = rootLocation.GetBeginPos();
        auto sameFile = region.rootLocation.GetFileID() == rootLocation.GetFileID();
        auto afterRegion = rootBegin.line > regionBegin.line ||
            (rootBegin.line == regionBegin.line && rootBegin.column >= regionBegin.column);
        return region.owner == &body && sameFile && afterRegion && Contains(region, rootLocation);
    });
    if (merged != result.end()) {
        merged->blocks.emplace_back(&root);
    } else {
        result.emplace_back(UnreachableBlockRegion{&body, {&root}, rootLocation});
    }
}

/*
 * Analyze a block group in three stages: compute entry reachability, split the
 * complement into CFG components, and add one source-level root for each
 * component. The result stores roots only; the full component is not needed by
 * the checker and is deliberately kept local to this pass.
 */
void AnalyzeBody(const BlockGroup& body, UnreachableBlockRegions& result)
{
    auto blocks = body.GetBlocks();
    auto remaining = CollectUnreachableBlocks(blocks, CollectReachableBlocks(body));
    while (!remaining.empty()) {
        auto root = *std::find_if(blocks.begin(), blocks.end(), [&remaining](auto block) {
            return remaining.find(block) != remaining.end();
        });
        auto component = CollectComponent(remaining, root);
        auto regionRoot = FindRegionRoot(blocks, component);
        if (regionRoot == nullptr) {
            continue;
        }
        AddRegion(body, *regionRoot, result);
    }
}

} // namespace

/* Visit every function and nested lambda block group. The visitor is used so the
 * same CFG analysis applies to ordinary functions and generated closures. */
UnreachableBlockRegions UnreachableBlockAnalysis::Parse() const
{
    UnreachableBlockRegions result;
    for (auto func : package->GetGlobalFuncsWithBody()) {
        Visitor::Visit(*func, [&result](BlockGroup& body) {
            AnalyzeBody(body, result);
            return VisitResult::CONTINUE;
        });
    }
    return result;
}

} // namespace Cangjie::CHIR
