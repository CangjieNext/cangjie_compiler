// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.

#ifndef CANGJIE_CHIR_CHECKER_UNREACHABLEBLOCKCHECK_H
#define CANGJIE_CHIR_CHECKER_UNREACHABLEBLOCKCHECK_H

#include "cangjie/Basic/DiagnosticEngine.h"
#include "cangjie/CHIR/Analysis/UnreachableBlockAnalysis.h"
#include "cangjie/CHIR/IR/Package.h"

namespace Cangjie::CHIR {

/**
 * @brief Report diagnostics for CFG regions unreachable from function entries.
 */
class UnreachableBlockCheck {
public:
    explicit UnreachableBlockCheck(DiagnosticEngine& diag, const std::string& packageName);

    /**
     * @brief Report unreachable block warnings for all functions in a package.
     */
    void RunOnPackage(const Package& package, const UnreachableBlockRegions& regions, size_t threadsNum);

    /**
     * @brief Report unreachable block warnings for one function.
     */
    void RunOnFunc(Function& func, const UnreachableBlockRegions& regions);

private:
    Ptr<Expression> GetUnreachableExpression(const Block& block, bool& isNormal) const;
    void PrintWarning(const Block& block, const DebugLocation& rootLocation, bool& isPrinted);

    DiagnosticEngine& diag;
    const std::string& currentPackageName;
};

} // namespace Cangjie::CHIR

#endif
