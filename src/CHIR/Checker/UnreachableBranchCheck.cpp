// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/Checker/UnreachableBranchCheck.h"

#include <map>
#include <unordered_set>

using namespace Cangjie::CHIR;

UnreachableBranchCheck::UnreachableBranchCheck(
    ConstAnalysisWrapper* constAnalysisWrapper, DiagnosticEngine& diag, const std::string& packageName)
    : diag(diag), analysisWrapper(constAnalysisWrapper), currentPackageName(packageName)
{
}

namespace {
std::string GetKeyWordBySourceExpr(SourceExpr sourceExpr)
{
    if (sourceExpr == SourceExpr::IF_EXPR) {
        return "if";
    } else if (sourceExpr == SourceExpr::WHILE_EXPR) {
        return "while";
    } else if (sourceExpr == SourceExpr::FOR_IN_EXPR) {
        return "for";
    }
    return "";
}

Branch* GetElseIfBranch(const Branch& branch, Block& block)
{
    auto nested = Cangjie::DynamicCast<Branch*>(block.GetTerminator());
    // Direct else-if entries share their source position with the nested Branch.
    // An ordinary `else { if ... }` body has a different source range.
    return branch.GetSourceExpr() == SourceExpr::IF_EXPR && &block == branch.GetFalseBlock() && nested &&
        nested->GetSourceExpr() == SourceExpr::IF_EXPR && nested->GetDebugLocation() == block.GetDebugLocation()
        ? nested : nullptr;
}

/*
 * The GetNextMatchCase overloads return {boundary terminator, next source-case entry}. A null terminator with a
 * non-null block means traversal must continue inside the current case; two nulls mean this path has no case boundary.
 */

// Interprets the false edge of a pattern/guard Branch according to its source-case metadata.
std::pair<Expression*, Block*> GetNextMatchCase(Branch& branch)
{
    auto nextCase = branch.GetFalseBlock();
    if (branch.GetSourceExpr() == SourceExpr::MATCH_CASE) {
        return {&branch, nextCase};
    }
    return branch.GetSourceExpr() == SourceExpr::MATCH_EXPR
        ? std::make_pair(nullptr, nextCase) : std::pair<Expression*, Block*>{};
}

// Interprets the default edge of a pattern MultiBranch, including boundaries inside an enum constructor table.
std::pair<Expression*, Block*> GetNextMatchCase(MultiBranch& branch)
{
    auto nextCase = branch.GetDefaultBlock();
    if (branch.GetSourceExpr() == SourceExpr::MATCH_CASE) {
        return {&branch, nextCase};
    }
    return branch.GetSourceExpr() == SourceExpr::MATCH_EXPR
        ? std::make_pair(nullptr, nextCase) : std::pair<Expression*, Block*>{};
}

// Walks pattern blocks and GoTo bridges until a Branch/MultiBranch overload finds the next source-case boundary.
std::pair<Expression*, Block*> GetNextMatchCase(Block& caseBlock)
{
    /*
     * Follow only pattern-evaluation CFG. GoTo nodes bridge one pattern corridor; MATCH_EXPR continues within a case;
     * MATCH_CASE exposes the terminator and entry of the next source case. Stop at case bodies and nested user control
     * flow so diagnostics never recurse into unrelated source expressions.
     */
    auto block = &caseBlock;
    std::set<Block*> visited;
    while (visited.emplace(block).second && block->TestAttr(Attribute::MATCH_PATTERN)) {
        auto terminator = block->GetTerminator();
        auto goTo = Cangjie::DynamicCast<GoTo>(terminator);
        if (goTo && goTo->TestAttr(Attribute::MATCH_PATTERN)) {
            block = goTo->GetDestination();
            continue;
        }
        if (goTo) {
            return {};
        }
        std::pair<Expression*, Block*> next;
        if (auto branch = Cangjie::DynamicCast<Branch>(terminator)) {
            next = GetNextMatchCase(*branch);
        } else if (auto multiBranch = Cangjie::DynamicCast<MultiBranch>(terminator)) {
            next = GetNextMatchCase(*multiBranch);
        } else {
            return {};
        }
        if (next.first || !next.second) {
            return next;
        }
        block = next.second;
    }
    return {};
}

// Suppress an excluded match-success edge only when another edge from the same source case can select its target.
template <typename TConstDomain>
bool CanSkipMatchSuccessWarning(Results<TConstDomain>& result, Branch& branch, Block& target,
    std::map<std::pair<Block*, Block*>, bool>& successCache)
{
    if (&target != branch.GetFalseBlock()) {
        return false;
    }
    auto success = branch.GetTrueBlock();
    if (result.GetEntryState(*success).IsBottom()) {
        return false;
    }
    auto nextCase = GetNextMatchCase(*branch.GetParentBlock()).second;
    if (!nextCase) {
        return false;
    }

    // OR operands share a success target and a source-case failure boundary. Check that group only once.
    auto key = std::make_pair(nextCase, success);
    if (auto it = successCache.find(key); it != successCache.end()) {
        return it->second;
    }

    bool reachable = false;
    for (auto predecessor : success->GetPredecessors()) {
        auto incoming = Cangjie::DynamicCast<Branch*>(predecessor->GetTerminator());
        if (!incoming || incoming == &branch || incoming->GetTrueBlock() != success) {
            continue;
        }
        if (result.GetEntryState(*predecessor).IsBottom()) {
            continue;
        }
        auto selected = result.GetTerminatorResult(*predecessor);
        if (selected.has_value() && selected.value() != success) {
            continue;
        }
        if (GetNextMatchCase(*predecessor).second != nextCase) {
            continue;
        }
        reachable = true;
        break;
    }
    successCache.emplace(key, reachable);
    return reachable;
}

// Groups pattern fragments by source case; each pair retains {fragment, originating normal successor}.
std::vector<std::vector<std::pair<Block*, Block*>>> CollectEnumMatchFragments(
    const std::vector<Block*>& roots)
{
    std::map<size_t, std::vector<std::pair<Block*, Block*>>> fragments;
    for (auto root : roots) {
        if (!root->TestAttr(Attribute::MATCH_PATTERN)) {
            continue;
        }

        // Expand a pattern-routing table into entries; the routing table is not itself a fragment.
        std::vector<Block*> entries{root};
        if (auto table = Cangjie::DynamicCast<MultiBranch*>(root->GetTerminator());
            table && table->TestAttr(Attribute::MATCH_PATTERN)) {
            entries = table->GetSuccessors();
        }

        // Entries can share a failure-chain suffix; collect it once per root, preserving other roots' origins.
        std::unordered_set<Block*> visited;
        for (auto block : entries) {
            while (block->TestAttr(Attribute::MATCH_PATTERN)) {
                if (!visited.emplace(block).second) {
                    break;
                }
                // Group fragments by source case while retaining the table successor they originate from.
                if (auto caseId = block->Get<MatchCaseId>()) {
                    fragments[caseId.value()].emplace_back(block, root);
                }
                auto terminator = block->GetTerminator();
                // Pattern bridges can lack a case ID; an unmarked GoTo ends pattern traversal.
                if (auto goTo = Cangjie::DynamicCast<GoTo*>(terminator);
                    goTo && goTo->TestAttr(Attribute::MATCH_PATTERN)) {
                    block = goTo->GetDestination();
                    continue;
                }
                // Only follow failures into enum fragments, including alternatives of the same source case.
                Block* next = nullptr;
                if (auto branch = Cangjie::DynamicCast<Branch*>(terminator)) {
                    next = GetNextMatchCase(*branch).second;
                } else if (auto table = Cangjie::DynamicCast<MultiBranch*>(terminator);
                    table && table->TestAttr(Attribute::MATCH_PATTERN)) {
                    next = GetNextMatchCase(*table).second;
                }
                if (!next || !next->Get<MatchCaseId>().has_value()) {
                    break;
                }
                block = next;
            }
        }
    }
    std::vector<std::vector<std::pair<Block*, Block*>>> groups;
    groups.reserve(fragments.size());
    for (auto& [caseId, blocks] : fragments) {
        groups.emplace_back(std::move(blocks));
    }
    return groups;
}

} // namespace
void UnreachableBranchCheck::RunOnPackage(const Package& package, size_t threadNum)
{
    if (threadNum == 1) {
        for (auto func : package.GetGlobalFuncsWithBody()) {
            /* The following code should not report warning.
            interface I {
                func test() : Bool {
                match(this) {
                    case v: ?Int64 => false
                    case v: Int64 => true
                    case _ => false
                }}}
            */
            if (func->Get<SkipCheck>() == SkipKind::SKIP_DCE_WARNING) {
                continue;
            }
            RunOnFunc(func);
        }
    } else {
        Utils::TaskQueue taskQueue(threadNum);
        // Check in generic decl is not currently supported, as constant analysis does not yet support.
        for (auto func : package.GetGlobalFuncsWithBody()) {
            if (func->Get<SkipCheck>() == SkipKind::SKIP_DCE_WARNING) {
                continue;
            }
            taskQueue.AddTask<void>([this, func]() { return RunOnFunc(func); });
        }
        taskQueue.RunAndWaitForAllTasksCompleted();
    }
}

template <typename TConstDomain>
void UnreachableBranchCheck::PrintMatchSuccessors(
    Results<TConstDomain>& result, Block& block, std::set<Block*>& hasProcessed)
{
    /*
     * A constant pattern or guard can make several later cases unreachable. Walk its source-case failure chain and
     * report each case once; hasProcessed also prevents duplicates after CFG paths converge.
     */
    auto current = &block;
    while (!current->Get<MatchCaseId>().has_value()) {
        auto [terminator, nextCase] = GetNextMatchCase(*current);
        if (!terminator || !nextCase || hasProcessed.find(nextCase) != hasProcessed.end() ||
            !result.GetEntryState(*nextCase).IsBottom()) {
            return;
        }
        PrintWarning(*terminator, *nextCase, hasProcessed);
        current = nextCase;
    }
}

template <typename TConstDomain>
bool UnreachableBranchCheck::TryVisitEnumMatchTable(
    Results<TConstDomain>& result, MultiBranch& table, Block& target, std::set<Block*>& hasProcessed)
{
    if (table.TestAttr(Attribute::MATCH_PATTERN)) {
        return false;
    }
    auto fragments = CollectEnumMatchFragments(table.GetNormalBlocks());
    if (fragments.empty()) {
        return false;
    }
    // The default can also be reached by payload failure, so it has no exclusive normal-successor origin.
    fragments.push_back({{table.GetDefaultBlock(), nullptr}});

    for (auto& blocks : fragments) {
        std::vector<Block*> unreachable;
        for (auto [block, root] : blocks) {
            if ((root && root != &target) || result.GetEntryState(*block).IsBottom()) {
                unreachable.emplace_back(block);
            }
        }
        if (unreachable.size() == blocks.size()) {
            auto [first, last] = std::minmax_element(
                unreachable.begin(), unreachable.end(), [](const Block* lhs, const Block* rhs) {
                    auto lhsPos = lhs->GetDebugLocation().GetBeginPos();
                    auto rhsPos = rhs->GetDebugLocation().GetBeginPos();
                    return lhsPos.line < rhsPos.line || (lhsPos.line == rhsPos.line && lhsPos.column < rhsPos.column);
                });
            auto location = (*first)->GetDebugLocation();
            location.SetEndPos((*last)->GetDebugLocation().GetEndPos());
            (*first)->template Set<DebugLocationInfoForWarning>(location);
            PrintWarning(table, **first, hasProcessed);
            hasProcessed.insert(unreachable.begin(), unreachable.end());
        } else {
            for (auto block : unreachable) {
                PrintWarning(table, *block, hasProcessed);
            }
        }
    }
    return true;
}

template <typename TConstDomain>
void UnreachableBranchCheck::VisitTerminator(
    Results<TConstDomain>& result, Expression& terminator, Block& target, std::set<Block*>& hasProcessed)
{
    Block* failure = nullptr;
    if (auto branch = Cangjie::DynamicCast<Branch*>(&terminator)) {
        failure = GetNextMatchCase(*branch).second;
    } else if (auto table = Cangjie::DynamicCast<MultiBranch*>(&terminator)) {
        if (TryVisitEnumMatchTable(result, *table, target, hasProcessed)) {
            return;
        }
        // The second result is the match's default successor, or nullptr for a non-match table.
        failure = GetNextMatchCase(*table).second;
    } else {
        CJC_ABORT();
    }
    for (auto successor : terminator.GetSuccessors()) {
        if (successor == &target) {
            continue;
        }
        // Pattern and guard paths can share a successor; an excluded edge alone does not make that case unreachable.
        if (successor == failure && !result.GetEntryState(*successor).IsBottom()) {
            continue;
        }
        PrintWarning(terminator, *successor, hasProcessed);
        if (successor == failure) {
            PrintMatchSuccessors(result, *successor, hasProcessed);
        }
    }
}

void UnreachableBranchCheck::PrintWarning(
    const Expression& node, Block& block, std::set<Block*>& hasProcessed)
{
    // Do-while expr do not need to check unreachable branch.
    if (block.Get<SkipCheck>() == SkipKind::SKIP_DCE_WARNING || node.Get<SkipCheck>() == SkipKind::SKIP_DCE_WARNING) {
        return;
    }
    auto [res, range] = GetDebugPos(block);
    if (!res) {
        return;
    }
    if (IsCrossPackage(range.begin, currentPackageName, diag)) {
        return;
    }
    auto branch = Cangjie::DynamicCast<const Branch*>(&node);
    auto sourceExpr = branch ? branch->GetSourceExpr() : StaticCast<const MultiBranch*>(&node)->GetSourceExpr();
    if (branch) {
        if (auto nested = GetElseIfBranch(*branch, block)) {
            PrintWarning(*nested, *nested->GetTrueBlock(), hasProcessed);
            PrintWarning(*nested, *nested->GetFalseBlock(), hasProcessed);
            return;
        }
    }
    if (!hasProcessed.emplace(&block).second) {
        return;
    }
    if (branch && (sourceExpr == SourceExpr::QUEST || sourceExpr == SourceExpr::BINARY)) {
        (void)diag.DiagnoseRefactor(DiagKindRefactor::chir_dce_unreachable_expression, range);
    } else if (sourceExpr == SourceExpr::MATCH_EXPR || sourceExpr == SourceExpr::MATCH_CASE ||
        (!branch && sourceExpr == SourceExpr::OTHER)) {
        (void)diag.DiagnoseRefactor(DiagKindRefactor::chir_unreachable_pattern, range);
    } else {
        auto keyWord = GetKeyWordBySourceExpr(sourceExpr);
        auto builder =
            diag.DiagnoseRefactor(DiagKindRefactor::chir_dce_unreachable_block_in_expression, range, keyWord);
        builder.AddMainHintArguments(keyWord);
    }
}

template <typename TConstDomain>
void UnreachableBranchCheck::VisitFunc(Results<TConstDomain>& result)
{
    std::map<std::pair<Block*, Block*>, bool> matchSuccessCache;
    std::set<Block*> hasProcessed;
    const auto actionBeforeVisitExpr = [](const TConstDomain&, Expression*, size_t) {};
    const auto actionAfterVisitExpr = [](const TConstDomain&, Expression*, size_t) {};
    const auto actionOnTerminator = [this, &result, &hasProcessed, &matchSuccessCache](
        const TConstDomain&, Expression* terminator, std::optional<Block*> target) {
        if (!target.has_value()) {
            return;
        }
        if (auto branch = Cangjie::DynamicCast<Branch*>(terminator);
            branch && CanSkipMatchSuccessWarning(result, *branch, *target.value(), matchSuccessCache)) {
            return;
        }
        if (terminator->GetExprKind() == ExprKind::BRANCH || terminator->GetExprKind() == ExprKind::MULTIBRANCH) {
            VisitTerminator(result, *terminator, *target.value(), hasProcessed);
        }
    };
    result.VisitWith(actionBeforeVisitExpr, actionAfterVisitExpr, actionOnTerminator);
}

void UnreachableBranchCheck::RunOnFunc(const Ptr<Function> func)
{
    // we should check the generic func, not the instantiated func.
    if (func->TestAttr(Attribute::GENERIC_INSTANTIATED)) {
        return;
    }
    bool isCommonFunctionWithoutBody = func->TestAttr(Attribute::SKIP_ANALYSIS);
    if (isCommonFunctionWithoutBody) {
        return; // Nothing to visit
    }
    if (auto result = analysisWrapper->CheckFuncResult(*func); result) {
        VisitFunc<ConstDomain>(*result);
    } else if (auto resultPool = analysisWrapper->CheckFuncActiveResult(*func); resultPool) {
        VisitFunc<ConstPoolDomain>(*resultPool);
    }
}
