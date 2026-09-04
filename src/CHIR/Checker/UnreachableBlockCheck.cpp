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
    auto it = std::find_if(expressions.begin(), expressions.end(), [&isNormal, &resExpression, this](auto expression) {
        auto posForWarning = expression->template Get<DebugLocationInfoForWarning>();
        auto [res, warningRange] = ToRangeIfNotZero(posForWarning);
        if (res) {
            // when report deadcode in BinaryExpr, it has 4 situations in tests/LLT/CHIR/DCE/testUnusedOp03.cj
            // if operand in binaryExpr is nothing, use warninglocation as report position
            // if all operand in binaryExpr is normal, use binaryExpr location as report position
            if (Is<BinaryExpressionBase>(expression) || Is<TryUnaryExpression>(expression)) {
                auto args = expression->GetNonSuccessorOperands();
                auto it =
                    std::find_if(args.begin(), args.end(), [](auto item) { return item->GetType()->IsNothing(); });
                if (it == args.end()) {
                    resExpression = expression;
                    isNormal = true;
                    return true;
                }
            }
            if (expression->GetExprKind() != ExprKind::DEBUGEXPR) {
                // variable debug node has warn position, but we want get it's normal position.
                resExpression = expression;
                isNormal = false;
                return true;
            }
        }
        if (!CheckUsersOfExpr(*expression)) {
            return false;
        }
        auto& debugInfo = expression->GetDebugLocation();
        auto [newResult, debugRange] = ToRangeIfNotZero(debugInfo);
        if (newResult && !IsCrossPackage(debugRange.begin, currentPackageName, diag)) {
            resExpression = expression;
            isNormal = true;
            return true;
        }
        return false;
    });
    if (it != expressions.end()) {
        return resExpression;
    }
    return nullptr;
}

void UnreachableBlockCheck::PrintWarning(
    const Block& block, const DebugLocation& rootLocation, bool& isPrinted)
{
    // Get position of nothing type node.
    auto [res, terminalNodeRange] = ToRangeIfNotZero(rootLocation);

    if (res && !isPrinted) {
        bool isNormal = true;
        if (auto unreableExpr = GetUnreachableExpression(block, isNormal)) {
            auto range = isNormal ? ToRange(unreableExpr->GetDebugLocation())
                : ToRange(unreableExpr->Get<DebugLocationInfoForWarning>());
            // Need to remove after find the cause.
            if (terminalNodeRange.begin == range.begin) {
                return;
            }
            if (IsCrossPackage(terminalNodeRange.begin, currentPackageName, diag)) {
                return;
            }
            if (unreableExpr->Get<SkipCheck>() == SkipKind::SKIP_DCE_WARNING) {
                return;
            }
            // Note: For the code:`if (let a <- 1) {return 1}`
            // the position of the Unit node is the position of the ifExpr,so it will generate a wrong
            // warnging,we should skip this scenario.
            if (unreableExpr->GetExprKind() == ExprKind::CONSTANT &&
                StaticCast<Constant*>(unreableExpr)->IsUnitLit()) {
                if (unreableExpr->GetResult()->TestAttr(Attribute::COMPILER_ADD)) {
                    return;
                }
            }
            if (!isNormal && (Is<BinaryExpression>(unreableExpr) || Is<UnaryExpression>(unreableExpr))) {
                auto diagBuilder = diag.DiagnoseRefactor(DiagKindRefactor::chir_dce_unreachable, range, "operator");
                diagBuilder.AddMainHintArguments("operator");
                diagBuilder.AddHint(terminalNodeRange);
            } else if (!isNormal && unreableExpr->GetExprKind() == ExprKind::APPLY) {
                auto diagBuilder = diag.DiagnoseRefactor(DiagKindRefactor::chir_dce_unreachable, range, "call");
                diagBuilder.AddMainHintArguments("call");
                diagBuilder.AddHint(terminalNodeRange);
            } else {
                auto diagBuilder =
                    diag.DiagnoseRefactor(DiagKindRefactor::chir_dce_unreachable_expression_hint, range);
                diagBuilder.AddHint(terminalNodeRange);
            }
            isPrinted = true;
        }
    }
}

} // namespace Cangjie::CHIR
