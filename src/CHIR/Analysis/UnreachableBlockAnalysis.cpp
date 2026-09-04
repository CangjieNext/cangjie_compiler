// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.

#include "cangjie/CHIR/Analysis/UnreachableBlockAnalysis.h"
#include "cangjie/CHIR/IR/Expression/Expression.h"
#include "cangjie/CHIR/Utils/Visitor/Visitor.h"

#include <algorithm>
#include <queue>
#include <unordered_set>

namespace Cangjie::CHIR {

static bool Contains(const DebugLocation& range, const DebugLocation& location)
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

static bool Contains(const UnreachableBlockRegion& region, const DebugLocation& location)
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

static void AnalyzeBody(const BlockGroup& body, UnreachableBlockRegions& result)
{
    auto blocks = body.GetBlocks();
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

    std::unordered_set<const Block*> remaining;
    for (auto block : blocks) {
        if (reachable.find(block) == reachable.end()) {
            remaining.emplace(block);
        }
    }
    while (!remaining.empty()) {
        auto root = *std::find_if(blocks.begin(), blocks.end(), [&remaining](auto block) {
            return remaining.find(block) != remaining.end();
        });
        remaining.erase(root);
        std::queue<const Block*> component;
        std::unordered_set<const Block*> componentBlocks{root};
        component.push(root);
        while (!component.empty()) {
            auto block = component.front();
            component.pop();
            for (auto successor : block->GetSuccessors()) {
                if (remaining.erase(successor) != 0) {
                    componentBlocks.emplace(successor);
                    component.push(successor);
                }
            }
            for (auto predecessor : block->GetPredecessors()) {
                if (remaining.erase(predecessor) != 0) {
                    componentBlocks.emplace(predecessor);
                    component.push(predecessor);
                }
            }
        }
        std::vector<const Block*> regionBlocks;
        const Block* regionRoot = nullptr;
        for (auto block : blocks) {
            if (componentBlocks.find(block) == componentBlocks.end()) {
                continue;
            }
            regionBlocks.emplace_back(block);
            bool hasComponentPredecessor = false;
            for (auto predecessor : block->GetPredecessors()) {
                if (componentBlocks.find(predecessor) != componentBlocks.end()) {
                    hasComponentPredecessor = true;
                    break;
                }
            }
            if (!hasComponentPredecessor && regionRoot == nullptr) {
                regionRoot = block;
            }
        }
        if (regionRoot == nullptr) {
            continue;
        }
        regionBlocks = {regionRoot};
        auto rootLocation = regionRoot->GetDebugLocation();
        if (rootLocation.GetBeginPos().IsZero()) {
            auto rootIter = std::find(blocks.begin(), blocks.end(), regionRoot);
            while (rootIter != blocks.begin()) {
                --rootIter;
                auto previousBlock = *rootIter;
                if (auto terminator = previousBlock->GetTerminator()) {
                    rootLocation = terminator->GetDebugLocation();
                    if (!rootLocation.GetBeginPos().IsZero()) {
                        break;
                    }
                }
            }
        }
        auto merged = std::find_if(result.begin(), result.end(), [&body, &rootLocation](const auto& region) {
            auto regionBegin = region.rootLocation.GetBeginPos();
            auto rootBegin = rootLocation.GetBeginPos();
            auto sameFile = region.rootLocation.GetFileID() == rootLocation.GetFileID();
            auto afterRegion = rootBegin.line > regionBegin.line ||
                (rootBegin.line == regionBegin.line && rootBegin.column >= regionBegin.column);
            return region.owner == &body && sameFile && afterRegion && Contains(region, rootLocation);
        });
        if (merged != result.end()) {
            merged->blocks.insert(merged->blocks.end(), regionBlocks.begin(), regionBlocks.end());
        } else {
            result.emplace_back(UnreachableBlockRegion{&body, std::move(regionBlocks), rootLocation});
        }
    }
}

UnreachableBlockRegions UnreachableBlockAnalysis::Parse(const Package& package)
{
    UnreachableBlockRegions result;
    for (auto func : package.GetGlobalFuncsWithBody()) {
        Visitor::Visit(*func, [&result](BlockGroup& body) {
            AnalyzeBody(body, result);
            return VisitResult::CONTINUE;
        });
    }
    return result;
}

} // namespace Cangjie::CHIR
