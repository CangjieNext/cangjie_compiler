// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/Checker/UnreachableBranchCheck.h"

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
std::pair<Expression*, Block*> GetNextMatchCase(
    Branch& branch, const std::set<Block*>& constructorGroups)
{
    auto nextCase = branch.GetFalseBlock();
    if (branch.GetSourceExpr() == SourceExpr::MATCH_CASE) {
        if (!constructorGroups.empty() &&
            (!nextCase->TestAttr(Attribute::MATCH_PATTERN) || !nextCase->Get<MatchCaseId>().has_value() ||
                constructorGroups.find(nextCase) != constructorGroups.end())) {
            return {};
        }
        return {&branch, nextCase};
    }
    return branch.GetSourceExpr() == SourceExpr::MATCH_EXPR
        ? std::make_pair(nullptr, nextCase) : std::pair<Expression*, Block*>{};
}

// Interprets the default edge of a pattern MultiBranch, including boundaries inside an enum constructor table.
std::pair<Expression*, Block*> GetNextMatchCase(
    MultiBranch& branch, const std::set<Block*>& constructorGroups)
{
    auto nextCase = branch.GetDefaultBlock();
    if (branch.GetSourceExpr() == SourceExpr::MATCH_CASE) {
        if (!constructorGroups.empty() &&
            (!nextCase->TestAttr(Attribute::MATCH_PATTERN) || !nextCase->Get<MatchCaseId>().has_value() ||
                constructorGroups.find(nextCase) != constructorGroups.end())) {
            return {};
        }
        return {&branch, nextCase};
    }
    return branch.GetSourceExpr() == SourceExpr::MATCH_EXPR
        ? std::make_pair(nullptr, nextCase) : std::pair<Expression*, Block*>{};
}

// Walks pattern blocks and GoTo bridges until a Branch/MultiBranch overload finds the next source-case boundary.
std::pair<Expression*, Block*> GetNextMatchCase(
    Block& caseBlock, const std::set<Block*>& constructorGroups = {})
{
    /*
     * Follow only pattern-evaluation CFG. GoTo nodes bridge one pattern corridor; MATCH_EXPR continues within a case;
     * MATCH_CASE exposes the terminator and entry of the next source case. Stop at case bodies, nested user control
     * flow, or another constructor-group root so diagnostics never recurse into unrelated source expressions.
     */
    auto block = &caseBlock;
    bool isFirstBlock = true;
    std::set<Block*> visited;
    while (visited.emplace(block).second && block->TestAttr(Attribute::MATCH_PATTERN)) {
        if (!isFirstBlock && constructorGroups.find(block) != constructorGroups.end()) {
            return {};
        }
        isFirstBlock = false;
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
            next = GetNextMatchCase(*branch, constructorGroups);
        } else if (auto multiBranch = Cangjie::DynamicCast<MultiBranch>(terminator)) {
            next = GetNextMatchCase(*multiBranch, constructorGroups);
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

void UnreachableBranchCheck::PrintMatchTableSuccessors(Block& block, std::set<Block*>& hasProcessed,
    std::set<size_t>& reportedCaseIds, const std::set<Block*>& constructorGroups)
{
    /*
     * A payload table may contain several fragments of one source case. Follow its default-edge chain inside the
     * current constructor group and use MatchCaseId to report that source case once.
     */
    auto current = &block;
    std::set<Block*> visited;
    while (visited.emplace(current).second) {
        auto [terminator, nextCase] = GetNextMatchCase(*current, constructorGroups);
        if (!terminator || !nextCase) {
            return;
        }
        auto caseId = nextCase->Get<MatchCaseId>();
        CJC_ASSERT(caseId.has_value());
        if (reportedCaseIds.emplace(caseId.value()).second) {
            PrintWarning(*terminator, *nextCase, hasProcessed);
        }
        current = nextCase;
    }
}

void UnreachableBranchCheck::PrintMatchTableCases(MultiBranch& table, std::set<Block*>& hasProcessed,
    std::set<size_t>& reportedCaseIds, const std::set<Block*>& constructorGroups)
{
    /*
     * Report keyed payload entries, then inspect each entry's failure chain and the table default edge. The default
     * may enter a later source case even when that case has no keyed entry in this table.
     */
    for (auto successor : table.GetNormalBlocks()) {
        CJC_ASSERT(successor->TestAttr(Attribute::MATCH_PATTERN) && successor->Get<MatchCaseId>().has_value());
        auto caseId = successor->Get<MatchCaseId>();
        CJC_ASSERT(caseId.has_value());
        if (reportedCaseIds.emplace(caseId.value()).second) {
            PrintWarning(table, *successor, hasProcessed);
        }
        PrintMatchTableSuccessors(*successor, hasProcessed, reportedCaseIds, constructorGroups);
    }
    PrintMatchTableSuccessors(*table.GetParentBlock(), hasProcessed, reportedCaseIds, constructorGroups);
}

void UnreachableBranchCheck::VisitBranch(Branch& branch, Block& target)
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
        // A marked guard follows a refutable pattern. Its false edge is not a source-case reachability decision;
        // pattern failure independently reaches the same next case. Its true edge remains reportable when rejected.
        if (branch.TestAttr(Attribute::MATCH_PATTERN) && successor == branch.GetFalseBlock()) {
            continue;
        }
        PrintWarning(branch, *successor, hasProcessed);
        if (successor == branch.GetFalseBlock() &&
            (branch.GetSourceExpr() == SourceExpr::MATCH_EXPR || branch.GetSourceExpr() == SourceExpr::MATCH_CASE)) {
            PrintMatchSuccessors(*successor, hasProcessed);
        }
    }
}

void UnreachableBranchCheck::VisitMultiBranch(MultiBranch& branch, Block& target)
{
    /*
     * A top-level optimized enum match has constructor-group successors marked MATCH_PATTERN. Group its table entries
     * by MatchCaseId because constructor order differs from source-case order. Other MultiBranch nodes use the normal
     * successor path and SourceExpr only selects the diagnostic kind.
     */
    CJC_ASSERT(!branch.GetParentBlock()->Get<MatchCaseId>().has_value());
    std::set<Block*> hasProcessed;
    std::set<size_t> reportedCaseIds;
    std::set<Block*> constructorGroups;
    for (auto successor : branch.GetNormalBlocks()) {
        if (successor->TestAttr(Attribute::MATCH_PATTERN)) {
            constructorGroups.emplace(successor);
        }
    }
    const bool table = !branch.GetParentBlock()->TestAttr(Attribute::MATCH_PATTERN) &&
        !constructorGroups.empty() && branch.GetSourceExpr() == SourceExpr::MATCH_EXPR;
    for (auto successor : branch.GetSuccessors()) {
        if (successor == &target) {
            continue;
        }
        if (table && successor->TestAttr(Attribute::MATCH_PATTERN)) {
            auto caseId = successor->Get<MatchCaseId>();
            CJC_ASSERT(caseId.has_value());
            if (reportedCaseIds.emplace(caseId.value()).second) {
                PrintWarning(branch, *successor, hasProcessed);
            }
            if (auto nestedTable = DynamicCast<MultiBranch>(successor->GetTerminator())) {
                PrintMatchTableCases(*nestedTable, hasProcessed, reportedCaseIds, constructorGroups);
            } else {
                PrintMatchTableSuccessors(*successor, hasProcessed, reportedCaseIds, constructorGroups);
            }
        } else {
            PrintWarning(branch, *successor, hasProcessed);
            if (branch.GetSourceExpr() == SourceExpr::MATCH_CASE && successor == branch.GetDefaultBlock()) {
                PrintMatchSuccessors(*successor, hasProcessed);
            }
        }
    }
    if (table && target.TestAttr(Attribute::MATCH_PATTERN) && target.Get<MatchCaseId>().has_value()) {
        auto targetCaseId = target.Get<MatchCaseId>();
        CJC_ASSERT(targetCaseId.has_value());
        reportedCaseIds.emplace(targetCaseId.value());
        if (target.GetTerminator()->GetExprKind() == ExprKind::MULTIBRANCH) {
            PrintMatchTableSuccessors(target, hasProcessed, reportedCaseIds, constructorGroups);
        }
    }
}

void UnreachableBranchCheck::PrintWarning(
    const Expression& node, Block& block, std::set<Block*>& hasProcessed)
{
    if (hasProcessed.find(&block) != hasProcessed.end()) {
        return;
    }
    hasProcessed.emplace(&block);
    // Do-while expr do not need to check unreachable branch.
    if (block.Get<SkipCheck>() == SkipKind::SKIP_DCE_WARNING || node.Get<SkipCheck>() == SkipKind::SKIP_DCE_WARNING) {
        return;
    }
    auto [res, range] = ToRangeIfNotZero(block.GetDebugLocation());
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

    const auto actionOnTerminator = [this](
            const TConstDomain&, Expression* terminator, std::optional<Block*> targetSucc) {
        switch (terminator->GetExprKind()) {
            case ExprKind::BRANCH: {
                if (targetSucc.has_value()) {
                    auto branchNode = StaticCast<Branch*>(terminator);
                    VisitBranch(*branchNode, *targetSucc.value());
                }
                break;
            }
            case ExprKind::MULTIBRANCH: {
                if (targetSucc.has_value()) {
                    VisitMultiBranch(*StaticCast<MultiBranch*>(terminator), *targetSucc.value());
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
