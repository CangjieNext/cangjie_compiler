// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_CHIR_CHECKER_UNREACHABLEBLOCKCHECK_H
#define CANGJIE_CHIR_CHECKER_UNREACHABLEBLOCKCHECK_H

#include "cangjie/Basic/DiagnosticEngine.h"
#include "cangjie/CHIR/Analysis/UnreachableBlockAnalysis.h"
#include "cangjie/CHIR/IR/Package.h"

namespace Cangjie::CHIR {

/**
 * Reports source diagnostics for regions found by UnreachableBlockAnalysis.
 *
 * This checker only selects a source expression and emits the corresponding
 * diagnostic. It does not compute reachability or mutate the CHIR package.
 */
class UnreachableBlockCheck {
public:
    /// The diagnostic engine and package name are borrowed by the checker.
    explicit UnreachableBlockCheck(DiagnosticEngine& diag, const std::string& packageName);

    /**
     * Report warnings for all function bodies, optionally in parallel.
     */
    void RunOnPackage(const Package& package, const UnreachableBlockRegions& regions, size_t threadsNum);

    /**
     * Report the regions owned by one top-level function.
     */
    void RunOnFunc(Function& func, const UnreachableBlockRegions& regions);

private:
    bool TryGetWarningExpression(Expression& expression, Ptr<Expression>& result, bool& isNormal) const;
    bool TryGetNormalExpression(Expression& expression, Ptr<Expression>& result) const;
    Ptr<Expression> GetUnreachableExpression(const Block& block, bool& isNormal) const;
    bool IsWarningSuppressed(const Expression& expression, const Cangjie::Range& range,
        const Cangjie::Range& terminalNodeRange) const;
    void EmitWarning(const Expression& expression, bool isNormal, const Cangjie::Range& range,
        const Cangjie::Range& terminalNodeRange);
    void PrintWarning(const Block& block, const DebugLocation& rootLocation, bool& isPrinted);

    DiagnosticEngine& diag;
    const std::string& currentPackageName;
};

} // namespace Cangjie::CHIR

#endif
