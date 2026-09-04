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
        const Block* representative = root;
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
        for (auto block : blocks) {
            if (componentBlocks.find(block) != componentBlocks.end() && !block->GetExpressions().empty()) {
                representative = block;
                break;
            }
        }
        auto rootLocation = representative->GetDebugLocation();
        if (rootLocation.GetBeginPos().IsZero() && !representative->GetExpressions().empty()) {
            rootLocation = representative->GetExpressions().front()->GetDebugLocation();
        }
        result.emplace_back(UnreachableBlockRegion{representative, rootLocation});
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
