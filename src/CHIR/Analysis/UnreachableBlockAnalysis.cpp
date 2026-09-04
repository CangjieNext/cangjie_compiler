// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * Computes source-level unreachable CFG regions and emits diagnostics before DCE
 * removes them.
 */

#include "cangjie/CHIR/Analysis/UnreachableBlockAnalysis.h"
#include "cangjie/CHIR/Analysis/Utils.h"
#include "cangjie/CHIR/IR/Expression/Expression.h"
#include "cangjie/CHIR/Utils/CHIRCasting.h"
#include "cangjie/CHIR/Utils/Utils.h"
#include "cangjie/CHIR/Utils/Visitor/Visitor.h"
#include "cangjie/Utils/TaskQueue.h"

#include <algorithm>
#include <iterator>
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
    std::unordered_set<const Block*> unreachableBlocks;
    for (auto block : blocks) {
        if (reachable.find(block) == reachable.end()) {
            unreachableBlocks.emplace(block);
        }
    }
    return unreachableBlocks;
}

/*
 * Grouping follows both predecessor and successor edges. This treats a loop as
 * one unreachable component even when no block in the loop has an incoming edge
 * from another component. The starting block is an arbitrary remaining one;
 * `unreachableBlocks` is consumed as the component is visited, so every
 * unreachable block is assigned exactly once.
 */
std::unordered_set<const Block*> CollectComponent(
    std::unordered_set<const Block*>& unreachableBlocks)
{
    std::queue<const Block*> workList;
    auto root = *unreachableBlocks.begin();
    std::unordered_set<const Block*> component{root};
    workList.push(root);
    unreachableBlocks.erase(root);
    while (!workList.empty()) {
        auto block = workList.front();
        workList.pop();
        for (auto successor : block->GetSuccessors()) {
            if (unreachableBlocks.erase(successor) != 0) {
                component.emplace(successor);
                workList.push(successor);
            }
        }
        for (auto predecessor : block->GetPredecessors()) {
            if (unreachableBlocks.erase(predecessor) != 0) {
                component.emplace(predecessor);
                workList.push(predecessor);
            }
        }
    }
    return component;
}

/*
 * A component with an incoming edge from outside itself has a natural root: a
 * block whose predecessors all lie outside the component. A pure cycle has none
 * and is skipped because it has no unique source entry to report.
 */
const Block* FindRegionRoot(const std::unordered_set<const Block*>& component)
{
    for (auto block : component) {
        const auto& predecessors = block->GetPredecessors();
        auto inComponent = [&component](auto predecessor) {
            return component.count(predecessor) != 0;
        };
        if (std::none_of(predecessors.begin(), predecessors.end(), inComponent)) {
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
 * Build a single source-level region for `root`. The root's debug location is
 * resolved through GetRootLocation because continuation blocks are often
 * synthesized without a source range of their own.
 */
UnreachableBlockRegion CreateRegion(const BlockGroup& body, const Block& root)
{
    auto rootLocation = GetRootLocation(body.GetBlocks(), root);
    return UnreachableBlockRegion{&body, {&root}, rootLocation};
}

bool RootLocationLess(const DebugLocation& a, const DebugLocation& b)
{
    if (a.GetFileID() != b.GetFileID()) {
        return a.GetFileID() < b.GetFileID();
    }
    auto aBegin = a.GetBeginPos();
    auto bBegin = b.GetBeginPos();
    return aBegin.line < bBegin.line ||
        (aBegin.line == bBegin.line && aBegin.column < bBegin.column);
}

/*
 * Regions are sorted by rootLocation first, so `existing` is earlier. Merge
 * into the first earlier region whose expressions cover the new root.
 */
UnreachableBlockRegions MergeRegion(
    UnreachableBlockRegions regions, const UnreachableBlockRegion& region)
{
    auto merged = std::find_if(regions.begin(), regions.end(), [&region](const auto& existing) {
        return Contains(existing, region.rootLocation);
    });
    if (merged != regions.end()) {
        merged->blocks.insert(merged->blocks.end(), region.blocks.begin(), region.blocks.end());
        return regions;
    }
    regions.push_back(region);
    return regions;
}

/*
 * Analyze a block group in three stages: compute entry reachability, split the
 * complement into CFG components, and emit one source-level region per
 * component. Components are visited until `unreachableBlocks` is drained;
 * CollectComponent consumes the blocks it visits, so each block is assigned to
 * exactly one component.
 */
UnreachableBlockRegions AnalyzeBody(const BlockGroup& body)
{
    auto blocks = body.GetBlocks();
    UnreachableBlockRegions result;
    auto unreachableBlocks = CollectUnreachableBlocks(blocks, CollectReachableBlocks(body));
    while (!unreachableBlocks.empty()) {
        auto component = CollectComponent(unreachableBlocks);
        // A pure cycle has no unique entry to report; skip it.
        auto regionRoot = FindRegionRoot(component);
        if (regionRoot == nullptr) {
            continue;
        }
        result.push_back(CreateRegion(body, *regionRoot));
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return RootLocationLess(a.rootLocation, b.rootLocation);
    });
    UnreachableBlockRegions merged;
    for (auto& region : result) {
        merged = MergeRegion(std::move(merged), region);
    }
    return merged;
}

/// Return whether every user has `kind` and consumes a Nothing-typed operand.
bool AllUsersIsExprKind(const std::vector<Expression*>& users, const ExprKind& kind)
{
    return std::all_of(users.begin(), users.end(), [&kind](auto user) {
        auto res = user->GetExprKind() == kind;
        auto args = user->GetNonSuccessorOperands();
        auto it = std::find_if(args.begin(), args.end(), [](auto item) { return item->GetType()->IsNothing(); });
        if (res && (it != args.end())) {
            return true;
        }
        return false;
    });
}

/*
 * Lowering can wrap a source expression in tuple/apply/array construction or in
 * a store without a source range. In these cases the wrapper is not the useful
 * warning location, so the original expression is kept as the fallback.
 */
bool PreferSourceExpression(const Expression& expr)
{
    if (expr.GetResult()) {
        auto users = expr.GetResult()->GetUsers();
        if (AllUsersIsExprKind(users, ExprKind::TUPLE)) {
            return true;
        }
        if (AllUsersIsExprKind(users, ExprKind::APPLY)) {
            return true;
        }
        if (AllUsersIsExprKind(users, ExprKind::RAW_ARRAY_LITERAL_INIT)) {
            return true;
        }
        if (expr.GetResult()->GetUsers().size() == 1 &&
            expr.GetResult()->GetUsers()[0]->GetExprKind() == ExprKind::STORE) {
            auto storeNode = expr.GetResult()->GetUsers()[0];
            auto [res, nodeRange] = ToRangeIfNotZero(storeNode->GetDebugLocation());
            if (!res) {
                return true;
            }
        }
        return false;
    }
    return true;
}

bool IsCompilerAddedUnit(const Expression& expression)
{
    if (expression.GetExprKind() != ExprKind::CONSTANT) {
        return false;
    }
    auto constant = StaticCast<const Constant*>(&expression);
    return constant->IsUnitLit() && expression.GetResult() != nullptr &&
        expression.GetResult()->TestAttr(Attribute::COMPILER_ADD);
}

/*
 * DebugLocationInfoForWarning points at the source construct that was lowered
 * into the current CHIR expression. Binary/unary expressions with a Nothing
 * operand keep their normal range, because their warning range points at the
 * terminating expression instead.
 */
bool TryGetWarningExpression(
    Expression& expression, Expression*& result, bool& isNormal)
{
    auto posForWarning = expression.Get<DebugLocationInfoForWarning>();
    auto [hasWarningRange, warningRange] = ToRangeIfNotZero(posForWarning);
    if (!hasWarningRange) {
        return false;
    }
    if (Is<BinaryExpressionBase>(&expression) || Is<TryUnaryExpression>(&expression)) {
        auto args = expression.GetNonSuccessorOperands();
        auto hasNothingOperand = std::any_of(args.begin(), args.end(), [](auto item) {
            return item->GetType()->IsNothing();
        });
        if (!hasNothingOperand) {
            result = &expression;
            isNormal = true;
            return true;
        }
    }
    if (expression.GetExprKind() != ExprKind::DEBUGEXPR) {
        result = &expression;
        isNormal = false;
        return true;
    }
    return false;
}

/// Select the expression's own range when it is a valid local-package fallback.
bool TryGetNormalExpression(
    Expression& expression, Expression*& result, DiagnosticEngine& diag, const std::string& packageName)
{
    if (!PreferSourceExpression(expression)) {
        return false;
    }
    auto [hasDebugRange, debugRange] = ToRangeIfNotZero(expression.GetDebugLocation());
    if (!hasDebugRange || IsCrossPackage(debugRange.begin, packageName, diag)) {
        return false;
    }
    result = &expression;
    return true;
}

Expression* GetUnreachableExpression(
    const Block& block, bool& isNormal, DiagnosticEngine& diag, const std::string& packageName)
{
    Expression* resExpression = nullptr;
    for (auto expression : block.GetExpressions()) {
        if (TryGetWarningExpression(*expression, resExpression, isNormal) ||
            TryGetNormalExpression(*expression, resExpression, diag, packageName)) {
            return resExpression;
        }
    }
    return nullptr;
}

/// Code source-inside the terminator (e.g. finally in a try) is not "following" it.
bool IsInsideHint(const Cangjie::Range& hint, const Cangjie::Range& range)
{
    if (hint.begin.fileID != range.begin.fileID || hint.HasZero() || range.begin.IsZero()) {
        return false;
    }
    return range.begin > hint.begin && range.begin <= hint.end;
}

bool IsWarningSuppressed(const Expression& expression, const Cangjie::Range& range,
    const Cangjie::Range& terminalNodeRange, DiagnosticEngine& diag, const std::string& packageName)
{
    if (terminalNodeRange.begin == range.begin || IsInsideHint(terminalNodeRange, range) ||
        IsCrossPackage(terminalNodeRange.begin, packageName, diag) ||
        expression.Get<SkipCheck>() == SkipKind::SKIP_DCE_WARNING) {
        return true;
    }
    return IsCompilerAddedUnit(expression);
}

/*
 * One warning per region. Location selection still has to skip lowering wrappers
 * and same-begin terminator ranges; operator/call DiagKind splitting is not needed.
 */
void ReportRegion(
    DiagnosticEngine& diag, const std::string& packageName, const UnreachableBlockRegion& region)
{
    auto [hasHint, terminalRange] = ToRangeIfNotZero(region.rootLocation);
    if (!hasHint) {
        return;
    }
    for (auto block : region.blocks) {
        bool isNormal = true;
        auto unreachableExpr = GetUnreachableExpression(*block, isNormal, diag, packageName);
        if (unreachableExpr == nullptr) {
            continue;
        }
        auto range = isNormal ? ToRange(unreachableExpr->GetDebugLocation())
            : ToRange(unreachableExpr->Get<DebugLocationInfoForWarning>());
        if (IsWarningSuppressed(*unreachableExpr, range, terminalRange, diag, packageName)) {
            continue;
        }
        if (!isNormal && (Is<BinaryExpression>(unreachableExpr) || Is<UnaryExpression>(unreachableExpr))) {
            auto diagBuilder = diag.DiagnoseRefactor(
                DiagKindRefactor::chir_dce_unreachable, range, "operator");
            diagBuilder.AddMainHintArguments("operator");
            diagBuilder.AddHint(terminalRange);
        } else if (!isNormal && unreachableExpr->GetExprKind() == ExprKind::APPLY) {
            auto diagBuilder = diag.DiagnoseRefactor(
                DiagKindRefactor::chir_dce_unreachable, range, "call");
            diagBuilder.AddMainHintArguments("call");
            diagBuilder.AddHint(terminalRange);
        } else {
            auto diagBuilder = diag.DiagnoseRefactor(
                DiagKindRefactor::chir_dce_unreachable_expression_hint, range);
            diagBuilder.AddHint(terminalRange);
        }
        return;
    }
}

} // namespace

UnreachableBlockAnalysis::UnreachableBlockAnalysis(DiagnosticEngine& diag, const Package& package)
    : diag(diag), package(package), currentPackageName(package.GetName())
{
}

void UnreachableBlockAnalysis::RunOnFunc(Function& func)
{
    UnreachableBlockRegions regions;
    Visitor::Visit(func, [&regions](BlockGroup& body) {
        auto bodyRegions = AnalyzeBody(body);
        regions.insert(regions.end(), std::make_move_iterator(bodyRegions.begin()),
            std::make_move_iterator(bodyRegions.end()));
        return VisitResult::CONTINUE;
    });
    for (const auto& region : regions) {
        ReportRegion(diag, currentPackageName, region);
    }
}

void UnreachableBlockAnalysis::RunOnPackage(size_t threadsNum)
{
    if (threadsNum == 1) {
        for (auto func : package.GetGlobalFuncsWithBody()) {
            if (func->TestAttr(Attribute::SKIP_ANALYSIS)) {
                continue;
            }
            RunOnFunc(*func);
        }
        return;
    }
    Utils::TaskQueue taskQueue(threadsNum);
    for (auto func : package.GetGlobalFuncsWithBody()) {
        if (func->TestAttr(Attribute::SKIP_ANALYSIS)) {
            continue;
        }
        taskQueue.AddTask<void>([this, func]() { RunOnFunc(*func); });
    }
    taskQueue.RunAndWaitForAllTasksCompleted();
}

} // namespace Cangjie::CHIR
