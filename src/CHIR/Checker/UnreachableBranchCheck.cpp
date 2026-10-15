// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/Checker/UnreachableBranchCheck.h"

#include <map>

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
     * MATCH_CASE exposes the terminator and entry of the next source case. Stop at case bodies, nested user control
     * flow, or another constructor-group root so diagnostics never recurse into unrelated source expressions.
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

std::map<size_t, std::vector<std::pair<Block*, Block*>>> CollectEnumMatchFragments(
    const std::set<Block*>& constructorBlocks)
{
    std::map<size_t, std::vector<std::pair<Block*, Block*>>> fragments;
    for (auto constructor : constructorBlocks) {
        std::vector<Block*> workList{constructor};
        while (!workList.empty()) {
            auto block = workList.back();
            workList.pop_back();
            if (auto caseId = block->Get<MatchCaseId>()) {
                fragments[caseId.value()].emplace_back(block, constructor);
            }
            for (auto successor : block->GetTerminator()->GetSuccessors()) {
                if (!successor->TestAttr(Attribute::MATCH_PATTERN) ||
                    constructorBlocks.find(successor) != constructorBlocks.end()) {
                    continue;
                }
                workList.emplace_back(successor);
            }
        }
    }
    return fragments;
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

void UnreachableBranchCheck::PrintMatchSuccessors(Block& block, std::set<Block*>& hasProcessed)
{
    /*
     * A constant pattern or guard can make several later cases unreachable. Walk its source-case failure chain and
     * report each case once; hasProcessed also prevents duplicates after CFG paths converge.
     */
    auto current = &block;
    while (true) {
        auto [terminator, nextCase] = GetNextMatchCase(*current);
        if (!terminator || !nextCase || hasProcessed.find(nextCase) != hasProcessed.end()) {
            return;
        }
        PrintWarning(*terminator, *nextCase, hasProcessed);
        current = nextCase;
    }
}

template <typename TConstDomain>
bool UnreachableBranchCheck::TryVisitEnumMatchTable(
    Results<TConstDomain>& result, MultiBranch& table, Block& target)
{
    std::set<Block*> constructorBlocks;
    for (auto block : table.GetNormalBlocks()) {
        if (block->Get<MatchCaseId>().has_value()) {
            constructorBlocks.emplace(block);
        }
    }
    if (constructorBlocks.empty()) {
        return false;
    }
    auto fragments = CollectEnumMatchFragments(constructorBlocks);

    std::set<Block*> hasProcessed;
    for (auto& entry : fragments) {
        auto& blocks = entry.second;
        std::vector<Block*> unreachable;
        for (auto [block, constructor] : blocks) {
            auto isBottom = result.GetEntryState(*block).IsBottom();
            if (constructor != &target || isBottom) {
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
        } else {
            for (auto block : unreachable) {
                PrintWarning(table, *block, hasProcessed);
            }
        }
    }
    auto defaultBlock = table.GetDefaultBlock();
    if (constructorBlocks.find(defaultBlock) == constructorBlocks.end() &&
        result.GetEntryState(*defaultBlock).IsBottom()) {
        PrintWarning(table, *defaultBlock, hasProcessed);
    }
    return true;
}

template <typename TConstDomain>
void UnreachableBranchCheck::VisitBranch(Results<TConstDomain>& result, Branch& branch, Block& target)
{
    /*
     * ConstAnalysis supplies the selected successor. For a match branch, an unselected false edge can also head later
     * source cases, so traverse it only when SourceExpr identifies a pattern/case boundary. Cases already diagnosed
     * by Sema carry SKIP_DCE_WARNING and are filtered by PrintWarning.
     */
    CJC_ASSERT(!branch.GetParentBlock()->Get<MatchCaseId>().has_value());
    std::set<Block*> hasProcessed;
    for (auto successor : branch.GetSuccessors()) {
        if (successor == &target) {
            continue;
        }
        if (!result.GetEntryState(*successor).IsBottom()) {
            continue;
        }
        PrintWarning(branch, *successor, hasProcessed);
        if (successor == branch.GetFalseBlock() &&
            (branch.GetSourceExpr() == SourceExpr::MATCH_EXPR || branch.GetSourceExpr() == SourceExpr::MATCH_CASE)) {
            PrintMatchSuccessors(*successor, hasProcessed);
        }
    }
}

template <typename TConstDomain>
void UnreachableBranchCheck::VisitMultiBranch(Results<TConstDomain>& result, MultiBranch& branch, Block& target)
{
    CJC_ASSERT(!branch.GetParentBlock()->Get<MatchCaseId>().has_value());
    if (TryVisitEnumMatchTable(result, branch, target)) {
        return;
    }
    std::set<Block*> hasProcessed;
    for (auto successor : branch.GetSuccessors()) {
        if (successor == &target) {
            continue;
        }
        if (!result.GetEntryState(*successor).IsBottom()) {
            continue;
        }
        PrintWarning(branch, *successor, hasProcessed);
        if (branch.GetSourceExpr() == SourceExpr::MATCH_CASE && successor == branch.GetDefaultBlock()) {
            PrintMatchSuccessors(*successor, hasProcessed);
        }
    }
}

void UnreachableBranchCheck::PrintWarning(const Expression& node, Block& block,
    std::set<Block*>& hasProcessed)
{
    if (hasProcessed.find(&block) != hasProcessed.end()) {
        return;
    }
    hasProcessed.emplace(&block);
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
    if (node.GetExprKind() == ExprKind::BRANCH) {
        auto branch = StaticCast<const Branch*>(&node);
        auto sourceExpr = branch->GetSourceExpr();
        if (auto nested = GetElseIfBranch(*branch, block)) {
            PrintWarning(*nested, *nested->GetTrueBlock(), hasProcessed);
            PrintWarning(*nested, *nested->GetFalseBlock(), hasProcessed);
            return;
        }
        if (sourceExpr == SourceExpr::QUEST || sourceExpr == SourceExpr::BINARY) {
            (void)diag.DiagnoseRefactor(DiagKindRefactor::chir_dce_unreachable_expression, range);
        } else if (sourceExpr == SourceExpr::MATCH_EXPR || sourceExpr == SourceExpr::MATCH_CASE) {
            (void)diag.DiagnoseRefactor(DiagKindRefactor::chir_unreachable_pattern, range);
        } else {
            auto keyWord = GetKeyWordBySourceExpr(sourceExpr);
            auto builder =
                diag.DiagnoseRefactor(DiagKindRefactor::chir_dce_unreachable_block_in_expression, range, keyWord);
            builder.AddMainHintArguments(keyWord);
        }
    } else {
        auto sourceExpr = StaticCast<const MultiBranch*>(&node)->GetSourceExpr();
        if (sourceExpr == SourceExpr::MATCH_EXPR || sourceExpr == SourceExpr::MATCH_CASE ||
            sourceExpr == SourceExpr::OTHER) {
            (void)diag.DiagnoseRefactor(DiagKindRefactor::chir_unreachable_pattern, range);
        } else {
            auto keyWord = GetKeyWordBySourceExpr(sourceExpr);
            auto builder =
                diag.DiagnoseRefactor(DiagKindRefactor::chir_dce_unreachable_block_in_expression, range, keyWord);
            builder.AddMainHintArguments(keyWord);
        }
    }
}

template <typename TConstDomain>
void UnreachableBranchCheck::VisitFunc(Results<TConstDomain>& result)
{
    const auto actionBeforeVisitExpr = [](const TConstDomain&, Expression*, size_t) {};
    const auto actionAfterVisitExpr = [](const TConstDomain&, Expression*, size_t) {};

    const auto actionOnTerminator = [this, &result](
            const TConstDomain&, Expression* terminator, std::optional<Block*> targetSucc) {
        // Nested enum-table fragments are diagnosed from their top-level table and source-case grouping.
        if (terminator->GetParentBlock()->Get<MatchCaseId>().has_value()) {
            return;
        }
        switch (terminator->GetExprKind()) {
            case ExprKind::BRANCH: {
                if (targetSucc.has_value()) {
                    auto branchNode = StaticCast<Branch*>(terminator);
                    VisitBranch(result, *branchNode, *targetSucc.value());
                }
                break;
            }
            case ExprKind::MULTIBRANCH: {
                if (targetSucc.has_value()) {
                    VisitMultiBranch(result, *StaticCast<MultiBranch*>(terminator), *targetSucc.value());
                }
                break;
            }
            default:
                break;
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
