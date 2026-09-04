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
/// Zero-location blocks are kept: they may only be CFG bridges, or they may
/// still hold located expressions. Dropping them splits a component so a later
/// located block becomes its own root and is reported twice.
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
 * from another component. The seed is an arbitrary remaining block: later
 * sorting by rootLocation, not collection order, decides merge. Remaining
 * blocks are consumed as they are visited, so each is assigned exactly once.
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
 * Continuation blocks are often synthesized without their own source range. In
 * that case use a predecessor terminator, which is the source expression that
 * transferred control into the continuation.
 */
DebugLocation GetRootLocation(const Block& root)
{
    auto rootLocation = root.GetDebugLocation();
    if (!rootLocation.GetBeginPos().IsZero()) {
        return rootLocation;
    }

    // A continuation created after a control-flow expression may not inherit
    // a source location. For example, `return 1` creates an isolated block
    // for the following statements; the block itself can be zero-located,
    // while its Exit still points to the return expression.
    if (auto terminator = root.GetTerminator()) {
        rootLocation = terminator->GetDebugLocation();
        if (!rootLocation.GetBeginPos().IsZero()) {
            return rootLocation;
        }
    }

    // Some continuation blocks have no useful location on the block or its
    // terminator. In that case, use the terminator of a CFG predecessor. This
    // preserves the source location of the expression that transfers control
    // into the continuation without relying on the order of BlockGroup blocks.
    for (auto predecessor : root.GetPredecessors()) {
        if (auto terminator = predecessor->GetTerminator()) {
            rootLocation = terminator->GetDebugLocation();
            if (!rootLocation.GetBeginPos().IsZero()) {
                return rootLocation;
            }
        }
    }

    CJC_ASSERT(false && "unreachable region root has no source location");
    return rootLocation;
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

UnreachableBlockRegion CreateRegion(
    const BlockGroup& body, const std::unordered_set<const Block*>& component)
{
    std::vector<const Block*> blocks(component.begin(), component.end());
    /*
     * ReportRegion warns on the first reportable expression.
     * Orphan continuations come first: they hold the dead statements after
     * return/throw/continue. Joins they flow into have predecessors and often
     * the enclosing if/try location. Example:
     *   continue
     *   var a = 1
     * The join after `else if` has the if location; the orphan holds `var a`.
     * Zero-location bridges come last so an empty continuation is not chosen
     * as the region's hint. Example: after `continue`, the empty match join
     * and `var a = 1` are both orphans; the empty block must not win.
     */
    std::sort(blocks.begin(), blocks.end(), [](const Block* a, const Block* b) {
        auto aOrphan = a->GetPredecessors().empty();
        auto bOrphan = b->GetPredecessors().empty();
        if (aOrphan != bOrphan) {
            return aOrphan;
        }
        auto aZero = a->GetDebugLocation().GetBeginPos().IsZero();
        auto bZero = b->GetDebugLocation().GetBeginPos().IsZero();
        if (aZero != bZero) {
            return !aZero;
        }
        return RootLocationLess(a->GetDebugLocation(), b->GetDebugLocation());
    });
    auto rootLocation = GetRootLocation(*blocks.front());
    return UnreachableBlockRegion{&body, std::move(blocks), rootLocation};
}

/*
 * `regions` is already sorted by rootLocation, so every existing region starts
 * no later than `region`. Nested source ranges therefore only need a
 * unidirectional Contains(existing, region.rootLocation): an earlier region's
 * expressions may cover a later root, not the reverse. The first match is the
 * outermost earlier region.
 */
UnreachableBlockRegions MergeRegion(
    UnreachableBlockRegions regions, const UnreachableBlockRegion& region)
{
    auto merged = std::find_if(regions.begin(), regions.end(), [&region](const auto& existing) {
        if (!Contains(existing, region.rootLocation)) {
            return false;
        }
        // A root already inside the existing hint would be dropped by
        // IsInsideHint after merge. Example:
        //   throw try { throw A(); A() } finally {}
        //   return 0
        // The throw-try range covers `throw A()`; keep `A()` as its own region.
        return !Contains(existing.rootLocation, region.rootLocation);
    });
    if (merged != regions.end()) {
        merged->blocks.insert(merged->blocks.end(), region.blocks.begin(), region.blocks.end());
        return regions;
    }
    regions.push_back(region);
    return regions;
}

/*
 * Collect unreachable CFG components, take each component's entry as a region,
 * then sort by rootLocation so MergeRegion can be unidirectional.
 *
 * CollectComponent's seed is arbitrary and remaining is consumed, so each
 * located unreachable block is assigned once. Sort puts earlier roots first;
 * MergeRegion then folds a later region into the first existing region whose
 * expressions contain the new root. Without that order an inner region would
 * be existing first, Contains(inner, outer.root) would fail, and both would
 * be reported.
 */
UnreachableBlockRegions AnalyzeBody(const BlockGroup& body)
{
    auto blocks = body.GetBlocks();
    UnreachableBlockRegions result;
    auto unreachableBlocks = CollectUnreachableBlocks(blocks, CollectReachableBlocks(body));
    while (!unreachableBlocks.empty()) {
        auto region = CreateRegion(body, CollectComponent(unreachableBlocks));
        result.push_back(std::move(region));
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
    if (expr.GetResult() == nullptr) {
        return true;
    }
    auto users = expr.GetResult()->GetUsers();
    if (AllUsersIsExprKind(users, ExprKind::TUPLE)) {
        /*  Report the later tuple element, not the whole tuple.
            Example code:
            (return 1, 3, 2)
        */
        return true;
    }
    if (AllUsersIsExprKind(users, ExprKind::APPLY)) {
        /*  Report this argument, not the apply.
            Example code:
            var a = 2
            A(1, 2, return A(), a + 2)
        */
        return true;
    }
    if (AllUsersIsExprKind(users, ExprKind::RAW_ARRAY_LITERAL_INIT)) {
        /*  Report the later array element, not the whole array literal.
            Example code:
            [return 1, 3, 2]
        */
        return true;
    }
    if (users.size() == 1 && users[0]->GetExprKind() == ExprKind::STORE) {
        /*  If the only user is a Store with zero position, use this expr.
            Example code:
            func foo() {
                return 1
                1       // have user of store
            }
        */
        auto [hasStoreRange, storeRange] = ToRangeIfNotZero(users[0]->GetDebugLocation());
        if (!hasStoreRange) {
            return true;
        }
    }
    return false;
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
 * An expression chosen as the warning site. `useWarningLocation` selects
 * DebugLocationInfoForWarning instead of the expression's own range.
 */
struct UnreachableExpr {
    Expression* expr = nullptr;
    bool useWarningLocation = false;
};

/*
 * DebugLocationInfoForWarning points at the source construct that was lowered
 * into the current CHIR expression. Binary/unary expressions without a Nothing
 * operand keep the expression's own range: their warning location points at the
 * terminator rather than the operator.
 */
bool TryGetWarningExpression(Expression& expression, UnreachableExpr& result)
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
            result = UnreachableExpr{&expression, false};
            return true;
        }
    }
    if (expression.GetExprKind() != ExprKind::DEBUGEXPR) {
        result = UnreachableExpr{&expression, true};
        return true;
    }
    return false;
}

/// Select the expression's own range when it is a valid local-package fallback.
bool TryGetNormalExpression(
    Expression& expression, UnreachableExpr& result, DiagnosticEngine& diag, const std::string& packageName)
{
    if (!PreferSourceExpression(expression)) {
        return false;
    }
    auto [hasDebugRange, debugRange] = ToRangeIfNotZero(expression.GetDebugLocation());
    if (!hasDebugRange || IsCrossPackage(debugRange.begin, packageName, diag)) {
        return false;
    }
    result = UnreachableExpr{&expression, false};
    return true;
}

UnreachableExpr GetUnreachableExpression(
    const Block& block, DiagnosticEngine& diag, const std::string& packageName)
{
    UnreachableExpr result;
    for (auto expression : block.GetExpressions()) {
        if (TryGetWarningExpression(*expression, result) ||
            TryGetNormalExpression(*expression, result, diag, packageName)) {
            return result;
        }
    }
    return {};
}

Cangjie::Range GetUnreachableRange(const UnreachableExpr& unreachable)
{
    if (unreachable.useWarningLocation) {
        return ToRange(unreachable.expr->Get<DebugLocationInfoForWarning>());
    }
    return ToRange(unreachable.expr->GetDebugLocation());
}

/*
 * Warning-location sites use chir_dce_unreachable so APPLY/operator print
 * "unreachable 'call'" / "unreachable 'operator'". Source-location sites, and
 * any other warning-location expression, use the generic expression hint.
 */
void DiagnoseUnreachable(DiagnosticEngine& diag, const UnreachableExpr& unreachable,
    const Cangjie::Range& range, const Cangjie::Range& terminalRange)
{
    auto& expr = *unreachable.expr;
    // Nothing-typed binary/unary: squiggle the operator via the warning location.
    if (unreachable.useWarningLocation &&
        (Is<BinaryExpression>(&expr) || Is<UnaryExpression>(&expr))) {
        auto diagBuilder = diag.DiagnoseRefactor(DiagKindRefactor::chir_dce_unreachable, range, "operator");
        diagBuilder.AddMainHintArguments("operator");
        diagBuilder.AddHint(terminalRange);
        return;
    }
    // Nothing-typed call: squiggle the callee via the warning location.
    if (unreachable.useWarningLocation && expr.GetExprKind() == ExprKind::APPLY) {
        auto diagBuilder = diag.DiagnoseRefactor(DiagKindRefactor::chir_dce_unreachable, range, "call");
        diagBuilder.AddMainHintArguments("call");
        diagBuilder.AddHint(terminalRange);
        return;
    }
    auto diagBuilder = diag.DiagnoseRefactor(DiagKindRefactor::chir_dce_unreachable_expression_hint, range);
    diagBuilder.AddHint(terminalRange);
}

/// Code source-inside the terminator (e.g. finally in a try) is not "following" it.
bool IsInsideHint(const Cangjie::Range& hint, const Cangjie::Range& range)
{
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
 * One warning per region. Walk merged roots until an expression survives
 * location selection and suppression, then emit a single diagnostic.
 */
void ReportRegion(
    DiagnosticEngine& diag, const std::string& packageName, const UnreachableBlockRegion& region)
{
    auto [hasHint, terminalRange] = ToRangeIfNotZero(region.rootLocation);
    if (!hasHint) {
        return;
    }
    for (auto block : region.blocks) {
        // Unreachable match cases mark the synthesized body entry; pattern diagnostics own them.
        if (block->Get<SkipCheck>() == SkipKind::SKIP_DCE_WARNING) {
            continue;
        }
        auto unreachable = GetUnreachableExpression(*block, diag, packageName);
        if (unreachable.expr == nullptr) {
            continue;
        }
        auto range = GetUnreachableRange(unreachable);
        if (IsWarningSuppressed(*unreachable.expr, range, terminalRange, diag, packageName)) {
            continue;
        }
        DiagnoseUnreachable(diag, unreachable, range, terminalRange);
        return;
    }
}

} // namespace

UnreachableBlockAnalysis::UnreachableBlockAnalysis(DiagnosticEngine& diag, const Package& package)
    : diag(diag), package(package)
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
        ReportRegion(diag, package.GetName(), region);
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
