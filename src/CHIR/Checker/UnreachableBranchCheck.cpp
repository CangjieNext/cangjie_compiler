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
    const std::unordered_map<Block*, Expression*>& maybeUnreachableBlocks)
    : diag(diag), analysisWrapper(constAnalysisWrapper), currentPackageName(packageName),
      maybeUnreachableBlocks(maybeUnreachableBlocks)
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

bool IsEnumMatchTableEntry(const Block& block)
{
    return block.TestAttr(Attribute::MATCH_PATTERN) && block.Get<MatchCaseId>().has_value();
}

bool IsOuterEnumMatchTable(const MultiBranch& table)
{
    if (table.GetSourceExpr() != SourceExpr::MATCH_EXPR ||
        table.GetParentBlock()->TestAttr(Attribute::MATCH_PATTERN)) {
        return false;
    }
    auto successors = table.GetNormalBlocks();
    return std::any_of(successors.begin(), successors.end(),
        [](const Block* block) { return IsEnumMatchTableEntry(*block); });
}

std::pair<Expression*, Block*> GetNextMatchCase(
    Branch& branch, bool withinTable, const std::set<Block*>& constructorGroups)
{
    auto nextCase = branch.GetFalseBlock();
    if (branch.GetSourceExpr() == SourceExpr::MATCH_CASE) {
        if (withinTable &&
            (!IsEnumMatchTableEntry(*nextCase) || constructorGroups.find(nextCase) != constructorGroups.end())) {
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
    MultiBranch& branch, bool withinTable, const std::set<Block*>& constructorGroups)
{
    auto nextCase = branch.GetDefaultBlock();
    if (branch.GetSourceExpr() == SourceExpr::MATCH_CASE) {
        if (withinTable &&
            (!IsEnumMatchTableEntry(*nextCase) || constructorGroups.find(nextCase) != constructorGroups.end())) {
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
    const std::set<Block*>& constructorGroups = NO_CONSTRUCTOR_GROUPS)
{
    auto block = &caseBlock;
    bool isFirstBlock = true;
    while (block != nullptr) {
        const bool withinTable = !constructorGroups.empty();
        if (maybeUnreachableBlocks.find(block) != maybeUnreachableBlocks.end() ||
            (withinTable && !isFirstBlock && constructorGroups.find(block) != constructorGroups.end()) ||
            !block->TestAttr(Attribute::MATCH_PATTERN)) {
            return {};
        }
        isFirstBlock = false;
        auto terminator = block->GetTerminator();
        if (auto goTo = Cangjie::DynamicCast<GoTo>(terminator)) {
            if (!goTo->TestAttr(Attribute::MATCH_PATTERN)) {
                return {};
            }
            block = goTo->GetDestination();
            continue;
        }
        std::pair<Expression*, Block*> next;
        if (auto branch = Cangjie::DynamicCast<Branch>(terminator)) {
            next = GetNextMatchCase(*branch, withinTable, constructorGroups);
        } else if (auto multiBranch = Cangjie::DynamicCast<MultiBranch>(terminator)) {
            next = GetNextMatchCase(*multiBranch, withinTable, constructorGroups);
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

void CollectMatchConstructorGroups(const BlockGroup& group, std::set<Block*>& constructorGroups);

void CollectMatchConstructorGroups(const MultiBranch& multiBranch, std::set<Block*>& constructorGroups)
{
    if (!IsOuterEnumMatchTable(multiBranch)) {
        return;
    }
    for (auto successor : multiBranch.GetNormalBlocks()) {
        if (IsEnumMatchTableEntry(*successor)) {
            constructorGroups.emplace(successor);
        }
    }
}

void CollectMatchConstructorGroups(const Block& block, std::set<Block*>& constructorGroups)
{
    if (auto multiBranch = Cangjie::DynamicCast<MultiBranch>(block.GetTerminator())) {
        CollectMatchConstructorGroups(*multiBranch, constructorGroups);
    }
    for (auto expression : block.GetExpressions()) {
        if (auto lambda = Cangjie::DynamicCast<Lambda>(expression)) {
            CollectMatchConstructorGroups(*lambda->GetBody(), constructorGroups);
        }
    }
}

void CollectMatchConstructorGroups(const BlockGroup& group, std::set<Block*>& constructorGroups)
{
    for (auto block : group.GetBlocks()) {
        CollectMatchConstructorGroups(*block, constructorGroups);
    }
}

void CollectMatchCaseIds(Block& block, Block& root, const std::set<Block*>& constructorGroups,
    std::set<size_t>& caseIds, std::set<Block*>& visited)
{
    if ((&block != &root && constructorGroups.find(&block) != constructorGroups.end()) ||
        !block.TestAttr(Attribute::MATCH_PATTERN) || !visited.emplace(&block).second) {
        return;
    }
    auto caseId = block.Get<MatchCaseId>();
    CJC_ASSERT(caseId.has_value());
    caseIds.emplace(caseId.value());
    auto terminator = block.GetTerminator();
    CJC_ASSERT(terminator);
    // Table patterns continue through keyed entries and pattern-failure edges inside one constructor group.
    if (auto multiBranch = Cangjie::DynamicCast<MultiBranch>(terminator)) {
        for (auto successor : multiBranch->GetNormalBlocks()) {
            if (IsEnumMatchTableEntry(*successor)) {
                CollectMatchCaseIds(*successor, root, constructorGroups, caseIds, visited);
            }
        }
        auto defaultBlock = multiBranch->GetDefaultBlock();
        if (IsEnumMatchTableEntry(*defaultBlock)) {
            CollectMatchCaseIds(*defaultBlock, root, constructorGroups, caseIds, visited);
        }
    } else if (auto branch = Cangjie::DynamicCast<Branch>(terminator)) {
        auto falseBlock = branch->GetFalseBlock();
        if (IsEnumMatchTableEntry(*falseBlock)) {
            CollectMatchCaseIds(*falseBlock, root, constructorGroups, caseIds, visited);
        }
    }
}

void ClearMatchTraversalMetadata(const BlockGroup& group)
{
    for (auto block : group.GetBlocks()) {
        block->DisableAttr(Attribute::MATCH_PATTERN);
        block->Remove<MatchCaseId>();
        if (auto goTo = Cangjie::DynamicCast<GoTo>(block->GetTerminator())) {
            goTo->DisableAttr(Attribute::MATCH_PATTERN);
        }
        for (auto expression : block->GetExpressions()) {
            if (auto lambda = Cangjie::DynamicCast<Lambda>(expression)) {
                ClearMatchTraversalMetadata(*lambda->GetBody());
            }
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
    for (auto func : package.GetGlobalFuncsWithBody()) {
        ClearMatchTraversalMetadata(*func->GetBody());
    }
}

void UnreachableBranchCheck::PrintMatchTableCases(MultiBranch& table, std::set<Block*>& hasProcessed,
    std::set<size_t>& reportedCaseIds, const std::set<Block*>& constructorGroups)
{
    for (auto successor : table.GetNormalBlocks()) {
        if (!IsEnumMatchTableEntry(*successor)) {
            continue;
        }
        auto caseId = successor->Get<MatchCaseId>();
        CJC_ASSERT(caseId.has_value());
        if (reportedCaseIds.emplace(caseId.value()).second) {
            PrintWarning(table, *successor, hasProcessed);
        }
        PrintMatchTableSuccessors(*successor, hasProcessed, reportedCaseIds, constructorGroups);
    }
    PrintMatchTableSuccessors(*table.GetParentBlock(), hasProcessed, reportedCaseIds, constructorGroups);
}

void UnreachableBranchCheck::PrintMatchTableSuccessors(Block& block, std::set<Block*>& hasProcessed,
    std::set<size_t>& reportedCaseIds, const std::set<Block*>& constructorGroups)
{
    auto current = &block;
    while (true) {
        auto [terminator, nextCase] = GetNextMatchCase(*current, maybeUnreachableBlocks, constructorGroups);
        if (!terminator || !nextCase || hasProcessed.find(nextCase) != hasProcessed.end()) {
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

void UnreachableBranchCheck::PrintMatchSuccessors(Block& block, std::set<Block*>& hasProcessed)
{
    auto current = &block;
    while (true) {
        auto [terminator, nextCase] = GetNextMatchCase(*current, maybeUnreachableBlocks);
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
    if (maybeUnreachableBlocks.find(&block) != maybeUnreachableBlocks.end()) {
        return;
    }
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

void UnreachableBranchCheck::VisitBranch(
    Branch& branch, Block& target, const std::set<Block*>& constructorGroups)
{
    std::set<Block*> hasProcessed;
    auto sourceExpr = branch.GetSourceExpr();
    const bool isMatchSource = sourceExpr == SourceExpr::MATCH_EXPR || sourceExpr == SourceExpr::MATCH_CASE;
    const bool isTablePattern = branch.GetParentBlock()->Get<MatchCaseId>().has_value();
    for (auto successor : branch.GetSuccessors()) {
        if (successor == &target) {
            continue;
        }
        const bool isFailureEdge = isMatchSource && successor == branch.GetFalseBlock();
        if (isTablePattern && isFailureEdge && constructorGroups.find(successor) != constructorGroups.end()) {
            continue;
        }
        PrintWarning(branch, *successor, hasProcessed);
        if (!isFailureEdge) {
            continue;
        }
        if (isTablePattern && IsEnumMatchTableEntry(*successor)) {
            auto caseId = successor->Get<MatchCaseId>();
            CJC_ASSERT(caseId.has_value());
            std::set<size_t> reportedCaseIds{caseId.value()};
            PrintMatchTableSuccessors(*successor, hasProcessed, reportedCaseIds, constructorGroups);
        } else if (!isTablePattern) {
            PrintMatchSuccessors(*successor, hasProcessed);
        }
    }
}

void UnreachableBranchCheck::VisitMultiBranchSuccessor(MultiBranch& branch, Block& successor,
    bool isOuterMatchTable, std::set<Block*>& hasProcessed, std::set<size_t>& reportedCaseIds,
    const std::set<Block*>& constructorGroups)
{
    auto sourceExpr = branch.GetSourceExpr();
    const bool isMatchSource = sourceExpr == SourceExpr::MATCH_EXPR || sourceExpr == SourceExpr::MATCH_CASE;
    const bool isTablePattern = branch.GetParentBlock()->Get<MatchCaseId>().has_value();
    const bool isFailureEdge = isMatchSource && &successor == branch.GetDefaultBlock();
    if (isTablePattern && isFailureEdge && constructorGroups.find(&successor) != constructorGroups.end()) {
        return;
    }
    if (isOuterMatchTable && IsEnumMatchTableEntry(successor)) {
        auto caseId = successor.Get<MatchCaseId>();
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
    if (isTablePattern && isMatchSource && IsEnumMatchTableEntry(successor)) {
        auto caseId = successor.Get<MatchCaseId>();
        CJC_ASSERT(caseId.has_value());
        std::set<size_t> tableCaseIds{caseId.value()};
        PrintMatchTableSuccessors(successor, hasProcessed, tableCaseIds, constructorGroups);
    } else if (sourceExpr == SourceExpr::MATCH_CASE && &successor == branch.GetDefaultBlock()) {
        PrintMatchSuccessors(successor, hasProcessed);
    }
}

void UnreachableBranchCheck::VisitMultiBranch(
    MultiBranch& branch, Block& target, const std::set<Block*>& constructorGroups)
{
    std::set<Block*> hasProcessed;
    const bool isOuterMatchTable = IsOuterEnumMatchTable(branch);
    std::set<size_t> reportedCaseIds;
    if (isOuterMatchTable && IsEnumMatchTableEntry(target)) {
        std::set<Block*> visited;
        CollectMatchCaseIds(target, target, constructorGroups, reportedCaseIds, visited);
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
                VisitBranch(*StaticCast<Branch*>(terminator), *targetSucc.value(), constructorGroups);
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
    CollectMatchConstructorGroups(*func->GetBody(), constructorGroups);
    if (auto result = analysisWrapper->CheckFuncResult(*func); result) {
        VisitFunc<ConstDomain>(*result, constructorGroups);
    } else if (auto resultPool = analysisWrapper->CheckFuncActiveResult(*func); resultPool) {
        VisitFunc<ConstPoolDomain>(*resultPool, constructorGroups);
    }
}
