// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.

#include "cangjie/CHIR/Checker/UnreachableBlockCheck.h"

#include "cangjie/CHIR/Analysis/Utils.h"
#include "cangjie/CHIR/Utils/CHIRCasting.h"
#include "cangjie/CHIR/Utils/Utils.h"
#include "cangjie/Utils/TaskQueue.h"

#include <algorithm>

namespace Cangjie::CHIR {
namespace {
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

bool CheckUsersOfExpr(const Expression& expr)
{
    // If the expression is the arg of apply, we should use the pos of this expression, not pos of apply.
    // Example code:
    // var a = 2
    // A(1, 2,return A(), a + 2)
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
        /*  If expr has user of store, but the store has zero position, then DCE use the position of this expr.

            func foo() {
                return 1
                1       // have user of store
            }
        */
        if (expr.GetResult()->GetUsers().size() == 1 && // 1 denote that if epxr has one user of store.
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
} // namespace

UnreachableBlockCheck::UnreachableBlockCheck(DiagnosticEngine& diag, const std::string& packageName)
    : diag(diag), currentPackageName(packageName)
{
}

void UnreachableBlockCheck::RunOnPackage(
    const Package& package, const UnreachableBlockRegions& regions, size_t threadsNum)
{
    if (threadsNum == 1) {
        for (auto func : package.GetGlobalFuncsWithBody()) {
            RunOnFunc(*func, regions);
        }
    } else {
        Utils::TaskQueue taskQueue(threadsNum);
        for (auto func : package.GetGlobalFuncsWithBody()) {
            bool isCommonFunctionWithoutBody = func->TestAttr(Attribute::SKIP_ANALYSIS);
            if (isCommonFunctionWithoutBody) {
                continue; // Nothing to visit
            }

            taskQueue.AddTask<void>([this, func, &regions]() { RunOnFunc(*func, regions); });
        }
        taskQueue.RunAndWaitForAllTasksCompleted();
    }
}

void UnreachableBlockCheck::RunOnFunc(Function& func, const UnreachableBlockRegions& regions)
{
    for (const auto& region : regions) {
        if (region.owner->GetTopLevelFunc() != &func) {
            continue;
        }
        bool isPrinted = false;
        for (auto block : region.blocks) {
            PrintWarning(*block, region.rootLocation, isPrinted);
        }
    }
}

Ptr<Expression> UnreachableBlockCheck::GetUnreachableExpression(const Block& block, bool& isNormal) const
{
    auto expressions = block.GetExpressions();
    Ptr<Expression> resExpression = nullptr;
    for (auto expression : expressions) {
        if (TryGetWarningExpression(*expression, resExpression, isNormal) ||
            TryGetNormalExpression(*expression, resExpression)) {
            return resExpression;
        }
    }
    return nullptr;
}

bool UnreachableBlockCheck::TryGetWarningExpression(
    Expression& expression, Ptr<Expression>& result, bool& isNormal) const
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

bool UnreachableBlockCheck::TryGetNormalExpression(
    Expression& expression, Ptr<Expression>& result) const
{
    if (!CheckUsersOfExpr(expression)) {
        return false;
    }
    auto [hasDebugRange, debugRange] = ToRangeIfNotZero(expression.GetDebugLocation());
    if (!hasDebugRange || IsCrossPackage(debugRange.begin, currentPackageName, diag)) {
        return false;
    }
    result = &expression;
    return true;
}

bool UnreachableBlockCheck::IsWarningSuppressed(
    const Expression& expression, const Cangjie::Range& range, const Cangjie::Range& terminalNodeRange) const
{
    if (terminalNodeRange.begin == range.begin ||
        IsCrossPackage(terminalNodeRange.begin, currentPackageName, diag) ||
        expression.Get<SkipCheck>() == SkipKind::SKIP_DCE_WARNING) {
        return true;
    }
    return expression.GetExprKind() == ExprKind::CONSTANT &&
        StaticCast<const Constant*>(&expression)->IsUnitLit() &&
        expression.GetResult()->TestAttr(Attribute::COMPILER_ADD);
}

void UnreachableBlockCheck::EmitWarning(const Expression& expression, bool isNormal,
    const Cangjie::Range& range, const Cangjie::Range& terminalNodeRange)
{
    if (!isNormal && (Is<BinaryExpression>(&expression) || Is<UnaryExpression>(&expression))) {
        auto diagBuilder = diag.DiagnoseRefactor(DiagKindRefactor::chir_dce_unreachable, range, "operator");
        diagBuilder.AddMainHintArguments("operator");
        diagBuilder.AddHint(terminalNodeRange);
    } else if (!isNormal && expression.GetExprKind() == ExprKind::APPLY) {
        auto diagBuilder = diag.DiagnoseRefactor(DiagKindRefactor::chir_dce_unreachable, range, "call");
        diagBuilder.AddMainHintArguments("call");
        diagBuilder.AddHint(terminalNodeRange);
    } else {
        auto diagBuilder = diag.DiagnoseRefactor(
            DiagKindRefactor::chir_dce_unreachable_expression_hint, range);
        diagBuilder.AddHint(terminalNodeRange);
    }
}

void UnreachableBlockCheck::PrintWarning(
    const Block& block, const DebugLocation& rootLocation, bool& isPrinted)
{
    // Get position of nothing type node.
    auto [res, terminalNodeRange] = ToRangeIfNotZero(rootLocation);

    if (!res || isPrinted) {
        return;
    }
    bool isNormal = true;
    auto unreachableExpr = GetUnreachableExpression(block, isNormal);
    if (unreachableExpr == nullptr) {
        return;
    }
    auto range = isNormal ? ToRange(unreachableExpr->GetDebugLocation())
        : ToRange(unreachableExpr->Get<DebugLocationInfoForWarning>());
    if (IsWarningSuppressed(*unreachableExpr, range, terminalNodeRange)) {
        return;
    }
    EmitWarning(*unreachableExpr, isNormal, range, terminalNodeRange);
    isPrinted = true;
}

} // namespace Cangjie::CHIR
