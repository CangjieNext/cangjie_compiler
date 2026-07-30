// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/Checker/UnreachableBranchCheck.h"

#include <algorithm>

using namespace Cangjie::CHIR;

UnreachableBranchCheck::UnreachableBranchCheck(
    ConstAnalysisWrapper* constAnalysisWrapper, DiagnosticEngine& diag, const std::string& packageName,
    const std::unordered_map<Block*, Expression*>& maybeUnreachableBlocks,
    MatchTraversalInfo& matchTraversalInfo)
    : diag(diag), analysisWrapper(constAnalysisWrapper), currentPackageName(packageName),
      maybeUnreachableBlocks(maybeUnreachableBlocks), matchTraversalInfo(matchTraversalInfo)
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

const std::set<Block*> NO_CONSTRUCTOR_GROUPS;

bool IsEnumMatchTableEntry(const Block& block, const MatchTraversalInfo& matchTraversalInfo)
{
    return matchTraversalInfo.IsPatternBlock(block) && matchTraversalInfo.GetCaseId(block).has_value();
}

bool IsOuterEnumMatchTable(const MultiBranch& table, const MatchTraversalInfo& matchTraversalInfo)
{
    if (table.GetSourceExpr() != SourceExpr::MATCH_EXPR ||
        matchTraversalInfo.IsPatternBlock(*table.GetParentBlock())) {
        return false;
    }
    auto successors = table.GetNormalBlocks();
    return std::any_of(successors.begin(), successors.end(),
        [&matchTraversalInfo](const Block* block) { return IsEnumMatchTableEntry(*block, matchTraversalInfo); });
}

std::pair<Expression*, Block*> GetNextMatchCase(
    Branch& branch, bool withinTable, const std::set<Block*>& constructorGroups,
    const MatchTraversalInfo& matchTraversalInfo)
{
    auto nextCase = branch.GetFalseBlock();
    if (branch.GetSourceExpr() == SourceExpr::MATCH_CASE) {
        if (withinTable &&
            (!IsEnumMatchTableEntry(*nextCase, matchTraversalInfo) ||
                constructorGroups.find(nextCase) != constructorGroups.end())) {
            return {};
        }
        return {&branch, nextCase};
    }
    if (branch.GetSourceExpr() == SourceExpr::MATCH_EXPR) {
        return {nullptr, nextCase};
    }
    return {};
}

std::pair<Expression*, Block*> GetNextMatchCase(
    MultiBranch& branch, bool withinTable, const std::set<Block*>& constructorGroups,
    const MatchTraversalInfo& matchTraversalInfo)
{
    auto nextCase = branch.GetDefaultBlock();
    if (branch.GetSourceExpr() == SourceExpr::MATCH_CASE) {
        if (withinTable &&
            (!IsEnumMatchTableEntry(*nextCase, matchTraversalInfo) ||
                constructorGroups.find(nextCase) != constructorGroups.end())) {
            return {};
        }
        return {&branch, nextCase};
    }
    if (branch.GetSourceExpr() == SourceExpr::MATCH_EXPR) {
        return {nullptr, nextCase};
    }
    return {};
}

std::pair<Expression*, Block*> GetNextMatchCase(Block& caseBlock,
    const std::unordered_map<Block*, Expression*>& maybeUnreachableBlocks,
    const MatchTraversalInfo& matchTraversalInfo,
    const std::set<Block*>& constructorGroups = NO_CONSTRUCTOR_GROUPS)
{
    auto block = &caseBlock;
    bool isFirstBlock = true;
    std::set<Block*> visited;
    while (block != nullptr) {
        if (!visited.emplace(block).second) {
            return {};
        }
        const bool withinTable = !constructorGroups.empty();
        if (maybeUnreachableBlocks.find(block) != maybeUnreachableBlocks.end() ||
            (withinTable && !isFirstBlock && constructorGroups.find(block) != constructorGroups.end()) ||
            !matchTraversalInfo.IsPatternBlock(*block)) {
            return {};
        }
        isFirstBlock = false;
        auto terminator = block->GetTerminator();
        if (auto goTo = Cangjie::DynamicCast<GoTo>(terminator)) {
            if (!matchTraversalInfo.IsPatternGoTo(*goTo)) {
                return {};
            }
            block = goTo->GetDestination();
            continue;
        }
        std::pair<Expression*, Block*> next;
        if (auto branch = Cangjie::DynamicCast<Branch>(terminator)) {
            next = GetNextMatchCase(*branch, withinTable, constructorGroups, matchTraversalInfo);
        } else if (auto multiBranch = Cangjie::DynamicCast<MultiBranch>(terminator)) {
            next = GetNextMatchCase(*multiBranch, withinTable, constructorGroups, matchTraversalInfo);
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

void CollectMatchConstructorGroups(
    const BlockGroup& group, std::set<Block*>& constructorGroups, const MatchTraversalInfo& matchTraversalInfo);

void CollectMatchConstructorGroups(const MultiBranch& multiBranch, std::set<Block*>& constructorGroups,
    const MatchTraversalInfo& matchTraversalInfo)
{
    if (!IsOuterEnumMatchTable(multiBranch, matchTraversalInfo)) {
        return;
    }
    for (auto successor : multiBranch.GetNormalBlocks()) {
        if (IsEnumMatchTableEntry(*successor, matchTraversalInfo)) {
            constructorGroups.emplace(successor);
        }
    }
}

void CollectMatchConstructorGroups(
    const Block& block, std::set<Block*>& constructorGroups, const MatchTraversalInfo& matchTraversalInfo)
{
    if (auto multiBranch = Cangjie::DynamicCast<MultiBranch>(block.GetTerminator())) {
        CollectMatchConstructorGroups(*multiBranch, constructorGroups, matchTraversalInfo);
    }
    for (auto expression : block.GetExpressions()) {
        if (auto lambda = Cangjie::DynamicCast<Lambda>(expression)) {
            CollectMatchConstructorGroups(*lambda->GetBody(), constructorGroups, matchTraversalInfo);
        }
    }
}

void CollectMatchConstructorGroups(
    const BlockGroup& group, std::set<Block*>& constructorGroups, const MatchTraversalInfo& matchTraversalInfo)
{
    for (auto block : group.GetBlocks()) {
        CollectMatchConstructorGroups(*block, constructorGroups, matchTraversalInfo);
    }
}

void CollectMatchCaseIds(Block& block, const std::set<Block*>& constructorGroups, std::set<size_t>& caseIds,
    std::set<Block*>& visited, const MatchTraversalInfo& matchTraversalInfo)
{
    if (!matchTraversalInfo.IsPatternBlock(block) || !visited.emplace(&block).second) {
        return;
    }
    auto caseId = matchTraversalInfo.GetCaseId(block);
    CJC_ASSERT(caseId.has_value());
    caseIds.emplace(caseId.value());
    auto terminator = block.GetTerminator();
    CJC_ASSERT(terminator);
    // Table patterns continue through keyed entries and pattern-failure edges inside one constructor group.
    if (auto multiBranch = Cangjie::DynamicCast<MultiBranch>(terminator)) {
        for (auto successor : multiBranch->GetNormalBlocks()) {
            if (constructorGroups.find(successor) == constructorGroups.end() &&
                IsEnumMatchTableEntry(*successor, matchTraversalInfo)) {
                CollectMatchCaseIds(*successor, constructorGroups, caseIds, visited, matchTraversalInfo);
            }
        }
        auto defaultBlock = multiBranch->GetDefaultBlock();
        if (constructorGroups.find(defaultBlock) == constructorGroups.end() &&
            IsEnumMatchTableEntry(*defaultBlock, matchTraversalInfo)) {
            CollectMatchCaseIds(*defaultBlock, constructorGroups, caseIds, visited, matchTraversalInfo);
        }
    } else if (auto branch = Cangjie::DynamicCast<Branch>(terminator)) {
        auto falseBlock = branch->GetFalseBlock();
        if (constructorGroups.find(falseBlock) == constructorGroups.end() &&
            IsEnumMatchTableEntry(*falseBlock, matchTraversalInfo)) {
            CollectMatchCaseIds(*falseBlock, constructorGroups, caseIds, visited, matchTraversalInfo);
        }
    }
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
    matchTraversalInfo.Clear();
}

void UnreachableBranchCheck::PrintMatchTableCases(MultiBranch& table, std::set<Block*>& hasProcessed,
    std::set<size_t>& reportedCaseIds, const std::set<Block*>& constructorGroups)
{
    for (auto successor : table.GetNormalBlocks()) {
        CJC_ASSERT(IsEnumMatchTableEntry(*successor, matchTraversalInfo));
        auto caseId = matchTraversalInfo.GetCaseId(*successor);
        CJC_ASSERT(caseId.has_value());
        if (reportedCaseIds.emplace(caseId.value()).second) {
            PrintWarning(table, *successor, hasProcessed);
        }
        PrintMatchTableSuccessors(*successor, hasProcessed, reportedCaseIds, constructorGroups);
    }
    // The owner terminator classifies its default edge as an in-case or cross-case failure.
    PrintMatchTableSuccessors(*table.GetParentBlock(), hasProcessed, reportedCaseIds, constructorGroups);
}

void UnreachableBranchCheck::PrintMatchTableSuccessors(Block& block, std::set<Block*>& hasProcessed,
    std::set<size_t>& reportedCaseIds, const std::set<Block*>& constructorGroups)
{
    auto current = &block;
    std::set<Block*> visited;
    while (true) {
        if (!visited.emplace(current).second) {
            return;
        }
        auto [terminator, nextCase] =
            GetNextMatchCase(*current, maybeUnreachableBlocks, matchTraversalInfo, constructorGroups);
        if (!terminator || !nextCase || hasProcessed.find(nextCase) != hasProcessed.end()) {
            return;
        }
        auto caseId = matchTraversalInfo.GetCaseId(*nextCase);
        CJC_ASSERT(caseId.has_value());
        if (reportedCaseIds.emplace(caseId.value()).second) {
            PrintWarning(*terminator, *nextCase, hasProcessed);
        }
        current = nextCase;
    }
}

void UnreachableBranchCheck::PrintMatchSuccessors(Block& block, std::set<Block*>& hasProcessed)
{
    auto current = &block;
    while (true) {
        auto [terminator, nextCase] = GetNextMatchCase(*current, maybeUnreachableBlocks, matchTraversalInfo);
        if (!terminator || !nextCase || hasProcessed.find(nextCase) != hasProcessed.end()) {
            return;
        }
        PrintWarning(*terminator, *nextCase, hasProcessed);
        current = nextCase;
    }
}

void UnreachableBranchCheck::PrintWarning(
    const Expression& node, Block& block, std::set<Block*>& hasProcessed)
{
    if (hasProcessed.find(&block) != hasProcessed.end()) {
        return;
    }
    hasProcessed.emplace(&block);
    CJC_ASSERT(maybeUnreachableBlocks.find(&block) == maybeUnreachableBlocks.end());
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
        auto multiBranch = StaticCast<const MultiBranch*>(&node);
        auto sourceExpr = multiBranch->GetSourceExpr();
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

void UnreachableBranchCheck::VisitBranch(Branch& branch, Block& target)
{
    std::set<Block*> hasProcessed;
    auto sourceExpr = branch.GetSourceExpr();
    const bool isMatchSource = sourceExpr == SourceExpr::MATCH_EXPR || sourceExpr == SourceExpr::MATCH_CASE;
    const bool isTablePattern = matchTraversalInfo.GetCaseId(*branch.GetParentBlock()).has_value();
    // ConstAnalysis does not track enum payload fields, so it cannot select a branch inside a table entry.
    CJC_ASSERT(!isTablePattern);
    for (auto successor : branch.GetSuccessors()) {
        if (successor == &target) {
            continue;
        }
        const bool isFailureEdge = isMatchSource && successor == branch.GetFalseBlock();
        PrintWarning(branch, *successor, hasProcessed);
        if (!isFailureEdge) {
            continue;
        }
        PrintMatchSuccessors(*successor, hasProcessed);
    }
}

void UnreachableBranchCheck::VisitMultiBranchSuccessor(MultiBranch& branch, Block& successor,
    bool isOuterMatchTable, std::set<Block*>& hasProcessed, std::set<size_t>& reportedCaseIds,
    const std::set<Block*>& constructorGroups)
{
    auto sourceExpr = branch.GetSourceExpr();
    const bool isTablePattern = matchTraversalInfo.GetCaseId(*branch.GetParentBlock()).has_value();
    // ConstAnalysis does not track enum payload fields, so it cannot select a nested table entry.
    CJC_ASSERT(!isTablePattern);
    if (isOuterMatchTable && IsEnumMatchTableEntry(successor, matchTraversalInfo)) {
        auto caseId = matchTraversalInfo.GetCaseId(successor);
        CJC_ASSERT(caseId.has_value());
        if (reportedCaseIds.emplace(caseId.value()).second) {
            PrintWarning(branch, successor, hasProcessed);
        }
        if (auto table = DynamicCast<MultiBranch>(successor.GetTerminator())) {
            PrintMatchTableCases(*table, hasProcessed, reportedCaseIds, constructorGroups);
        } else {
            PrintMatchTableSuccessors(successor, hasProcessed, reportedCaseIds, constructorGroups);
        }
        return;
    }
    PrintWarning(branch, successor, hasProcessed);
    if (sourceExpr == SourceExpr::MATCH_CASE && &successor == branch.GetDefaultBlock()) {
        PrintMatchSuccessors(successor, hasProcessed);
    }
}

void UnreachableBranchCheck::VisitMultiBranch(
    MultiBranch& branch, Block& target, const std::set<Block*>& constructorGroups)
{
    std::set<Block*> hasProcessed;
    const bool isOuterMatchTable = IsOuterEnumMatchTable(branch, matchTraversalInfo);
    std::set<size_t> reportedCaseIds;
    if (isOuterMatchTable && IsEnumMatchTableEntry(target, matchTraversalInfo)) {
        std::set<Block*> visited;
        CollectMatchCaseIds(target, constructorGroups, reportedCaseIds, visited, matchTraversalInfo);
    }
    for (auto successor : branch.GetSuccessors()) {
        if (successor == &target) {
            continue;
        }
        VisitMultiBranchSuccessor(
            branch, *successor, isOuterMatchTable, hasProcessed, reportedCaseIds, constructorGroups);
    }
}

template <typename TConstDomain>
void UnreachableBranchCheck::VisitFunc(Results<TConstDomain>& result, const std::set<Block*>& constructorGroups)
{
    const auto actionBeforeVisitExpr = [](const TConstDomain&, Expression*, size_t) {};
    const auto actionAfterVisitExpr = [](const TConstDomain&, Expression*, size_t) {};

    const auto actionOnTerminator = [this, &constructorGroups](
            const TConstDomain&, Expression* terminator, std::optional<Block*> targetSucc) {
        if (!targetSucc.has_value() ||
            maybeUnreachableBlocks.find(terminator->GetParentBlock()) != maybeUnreachableBlocks.end()) {
            return;
        }
        switch (terminator->GetExprKind()) {
            case ExprKind::BRANCH:
                VisitBranch(*StaticCast<Branch*>(terminator), *targetSucc.value());
                break;
            case ExprKind::MULTIBRANCH:
                VisitMultiBranch(*StaticCast<MultiBranch*>(terminator), *targetSucc.value(), constructorGroups);
                break;
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
    std::set<Block*> constructorGroups;
    CollectMatchConstructorGroups(*func->GetBody(), constructorGroups, matchTraversalInfo);
    if (auto result = analysisWrapper->CheckFuncResult(*func); result) {
        VisitFunc<ConstDomain>(*result, constructorGroups);
    } else if (auto resultPool = analysisWrapper->CheckFuncActiveResult(*func); resultPool) {
        VisitFunc<ConstPoolDomain>(*resultPool, constructorGroups);
    }
}
