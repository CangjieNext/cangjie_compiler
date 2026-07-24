// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/Checker/UnreachableBranchCheck.h"

using namespace Cangjie::CHIR;

UnreachableBranchCheck::UnreachableBranchCheck(
    ConstAnalysisWrapper* constAnalysisWrapper, DiagnosticEngine& diag, const std::string& packageName,
    const UnreachableBlockMap& maybeUnreachableBlocks)
    : diag(diag), analysisWrapper(constAnalysisWrapper), currentPackageName(packageName),
      maybeUnreachableBlocks(maybeUnreachableBlocks)
{
}

namespace {
std::string GetKeyWordBySourceExpr(const Branch& branch)
{
    if (branch.GetSourceExpr() == SourceExpr::IF_EXPR) {
        return "if";
    } else if (branch.GetSourceExpr() == SourceExpr::WHILE_EXPR) {
        return "while";
    } else if (branch.GetSourceExpr() == SourceExpr::FOR_IN_EXPR) {
        return "for";
    }
    return "";
}

bool IsPatternGuard(const Branch& branch)
{
    const auto& trueLoc = branch.GetTrueBlock()->GetDebugLocation();
    return !trueLoc.IsInvalidPos() && branch.GetCondition()->GetDebugLocation() == trueLoc;
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

void UnreachableBranchCheck::PrintMatchSuccessors(
    Block& block, std::set<Block*>& hasProcessed, const UnreachableBlockRegionMap& regions)
{
    if (!block.Get<MatchDispatchContinuation>()) {
        return;
    }
    auto terminator = block.GetTerminator();
    while (auto goTo = DynamicCast<GoTo>(terminator)) {
        auto destination = goTo->GetDestination();
        if (!destination->Get<MatchDispatchContinuation>()) {
            return;
        }
        if (!hasProcessed.emplace(destination).second) {
            return;
        }
        terminator = destination->GetTerminator();
    }
    if (auto matchBranch = DynamicCast<Branch>(terminator); matchBranch &&
        matchBranch->GetSourceExpr() == SourceExpr::MATCH_EXPR) {
        if (IsPatternGuard(*matchBranch)) {
            // A guard true edge stays within the current case; the false edge enters the next case.
            auto falseBlock = matchBranch->GetFalseBlock();
            PrintWarning(
                *matchBranch, *falseBlock, hasProcessed, falseBlock->Get<MatchDispatchContinuation>(), regions);
        } else {
            // Pattern dispatch edges can enter distinct source-level cases, including nested matches.
            for (auto successor : matchBranch->GetSuccessors()) {
                PrintWarning(
                    *matchBranch, *successor, hasProcessed, successor->Get<MatchDispatchContinuation>(), regions);
            }
        }
    } else if (DynamicCast<MultiBranch>(terminator)) {
        // Match table/or-pattern lowering uses MultiBranch, while ordinary nested control flow uses Branch.
        for (auto successor : terminator->GetSuccessors()) {
            PrintWarning(*terminator, *successor, hasProcessed, successor->Get<MatchDispatchContinuation>(), regions);
        }
    }
}

void UnreachableBranchCheck::PrintWarning(
    const Terminator& node, Block& block, std::set<Block*>& hasProcessed, bool shouldTraverseMatchCases,
    const UnreachableBlockRegionMap& regions)
{
    if (hasProcessed.find(&block) != hasProcessed.end()) {
        return;
    }
    hasProcessed.emplace(&block);
    if (regions.find(&block) != regions.end()) {
        return;
    }
    // Do-while expr do not need to check unreachable branch.
    if (block.Get<SkipCheck>() == SkipKind::SKIP_DCE_WARNING || node.Get<SkipCheck>() == SkipKind::SKIP_DCE_WARNING) {
        return;
    }
    // Ordinary CFG successors can be shared continuations or nested if/else blocks. Only match lowering carries
    // independent source-level pattern diagnostics that require structural traversal beyond the direct successor.
    if (shouldTraverseMatchCases) {
        PrintMatchSuccessors(block, hasProcessed, regions);
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
        if (branch->GetSourceExpr() == SourceExpr::QUEST || branch->GetSourceExpr() == SourceExpr::BINARY) {
            (void)diag.DiagnoseRefactor(DiagKindRefactor::chir_dce_unreachable_expression, range);
        } else if (branch->GetSourceExpr() == SourceExpr::MATCH_EXPR) {
            (void)diag.DiagnoseRefactor(DiagKindRefactor::chir_unreachable_pattern, range);
        } else {
            auto keyWord = GetKeyWordBySourceExpr(*branch);
            auto builder =
                diag.DiagnoseRefactor(DiagKindRefactor::chir_dce_unreachable_block_in_expression, range, keyWord);
            builder.AddMainHintArguments(keyWord);
        }
    } else {
        (void)diag.DiagnoseRefactor(DiagKindRefactor::chir_unreachable_pattern, range);
    }
}

template <typename TConstDomain>
void UnreachableBranchCheck::VisitFunc(Results<TConstDomain>& result, const UnreachableBlockRegionMap& regions)
{
    const auto actionBeforeVisitExpr = [](const TConstDomain&, Expression*, size_t) {};
    const auto actionAfterVisitExpr = [](const TConstDomain&, Expression*, size_t) {};

    const auto actionOnTerminator = [this, &regions](
            const TConstDomain&, Terminator* terminator, std::optional<Block*> targetSucc) {
        if (regions.find(terminator->GetParentBlock()) != regions.end()) {
            return;
        }
        switch (terminator->GetExprKind()) {
            case ExprKind::BRANCH: {
                if (targetSucc.has_value()) {
                    auto branchNode = StaticCast<Branch*>(terminator);
                    auto successors = terminator->GetSuccessors();
                    std::set<Block*> hasProcessed;
                    for (auto successor : successors) {
                        if (successor == targetSucc.value()) {
                            continue;
                        }
                        bool shouldTraverseMatchCases = successor->Get<MatchDispatchContinuation>();
                        PrintWarning(*branchNode, *successor, hasProcessed, shouldTraverseMatchCases, regions);
                    }
                }
                break;
            }
            case ExprKind::MULTIBRANCH: {
                if (targetSucc.has_value()) {
                    auto multiBranchNode = StaticCast<MultiBranch*>(terminator);
                    auto successors = terminator->GetSuccessors();
                    std::set<Block*> hasProcessed;
                    for (auto successor : successors) {
                        if (successor == targetSucc.value()) {
                            continue;
                        }
                        PrintWarning(*multiBranchNode, *successor, hasProcessed,
                            successor->Get<MatchDispatchContinuation>(), regions);
                    }
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
    auto regions = GetUnreachableBlockRegions(*func->GetBody(), maybeUnreachableBlocks);
    if (auto result = analysisWrapper->CheckFuncResult(*func); result) {
        VisitFunc<ConstDomain>(*result, regions);
    } else if (auto resultPool = analysisWrapper->CheckFuncActiveResult(*func); resultPool) {
        VisitFunc<ConstPoolDomain>(*resultPool, regions);
    }
}
