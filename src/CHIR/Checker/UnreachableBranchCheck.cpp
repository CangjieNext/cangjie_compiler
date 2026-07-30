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

bool IsMatchCaseFailureEdge(const Branch& branch)
{
    if (branch.GetSourceExpr() != SourceExpr::MATCH_EXPR) {
        return false;
    }
    const auto& caseScope = branch.GetParentBlock()->GetDebugLocation().GetScopeInfo();
    const auto& matchedScope = branch.GetTrueBlock()->GetDebugLocation().GetScopeInfo();
    const auto& nextCaseScope = branch.GetFalseBlock()->GetDebugLocation().GetScopeInfo();
    if (caseScope.empty() || matchedScope != caseScope || nextCaseScope.size() != caseScope.size()) {
        return false;
    }
    // Match case branches stay in the current case scope and advance its last scope index on the false edge.
    return std::equal(caseScope.begin(), caseScope.end() - 1, nextCaseScope.begin()) &&
        nextCaseScope.back() != caseScope.back();
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
    Block& block, std::set<Block*>& hasProcessed)
{
    if (maybeUnreachableBlocks.find(&block) != maybeUnreachableBlocks.end()) {
        return;
    }
    auto terminator = block.GetTerminator();
    while (auto goTo = DynamicCast<GoTo>(terminator)) {
        auto destination = goTo->GetDestination();
        if (maybeUnreachableBlocks.find(destination) != maybeUnreachableBlocks.end()) {
            return;
        }
        if (!hasProcessed.emplace(destination).second) {
            return;
        }
        terminator = destination->GetTerminator();
    }
    if (auto matchBranch = DynamicCast<Branch>(terminator); matchBranch && IsMatchCaseFailureEdge(*matchBranch)) {
        // A match false edge enters the next source-level case. Do not traverse the true case body.
        auto nextCase = matchBranch->GetFalseBlock();
        PrintWarning(*matchBranch, *nextCase, hasProcessed);
        PrintMatchSuccessors(*nextCase, hasProcessed);
    } else if (auto multiBranch = DynamicCast<MultiBranch>(terminator)) {
        // Table lowering can dispatch directly to several source-level cases.
        for (auto successor : multiBranch->GetSuccessors()) {
            PrintWarning(*multiBranch, *successor, hasProcessed);
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
void UnreachableBranchCheck::VisitFunc(Results<TConstDomain>& result)
{
    const auto actionBeforeVisitExpr = [](const TConstDomain&, Expression*, size_t) {};
    const auto actionAfterVisitExpr = [](const TConstDomain&, Expression*, size_t) {};

    const auto actionOnTerminator = [this](
            const TConstDomain&, Expression* terminator, std::optional<Block*> targetSucc) {
        if (maybeUnreachableBlocks.find(terminator->GetParentBlock()) != maybeUnreachableBlocks.end()) {
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
                        PrintWarning(*branchNode, *successor, hasProcessed);
                        if (branchNode->GetSourceExpr() == SourceExpr::MATCH_EXPR &&
                            successor == branchNode->GetFalseBlock()) {
                            PrintMatchSuccessors(*successor, hasProcessed);
                        }
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
                        PrintWarning(*multiBranchNode, *successor, hasProcessed);
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
    if (auto result = analysisWrapper->CheckFuncResult(*func); result) {
        VisitFunc<ConstDomain>(*result);
    } else if (auto resultPool = analysisWrapper->CheckFuncActiveResult(*func); resultPool) {
        VisitFunc<ConstPoolDomain>(*resultPool);
    }
}
