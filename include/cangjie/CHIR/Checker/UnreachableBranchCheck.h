// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_CHIR_CHECKER_UNREACHABLE_BRANCH_CHECK_H
#define CANGJIE_CHIR_CHECKER_UNREACHABLE_BRANCH_CHECK_H

#include "cangjie/CHIR/Analysis/ConstAnalysisWrapper.h"
#include "cangjie/CHIR/Analysis/ConstAnalysis.h"
#include "cangjie/CHIR/Analysis/Utils.h"
#include "cangjie/Basic/DiagnosticEngine.h"
#include "cangjie/CHIR/IR/Package.h"
#include "cangjie/Utils/TaskQueue.h"

namespace Cangjie::CHIR {

class UnreachableBranchCheck {
public:
    explicit UnreachableBranchCheck(
        ConstAnalysisWrapper* constAnalysisWrapper, DiagnosticEngine& diag, const std::string& packageName);

    void RunOnPackage(const Package& package, size_t threadNum);

    void RunOnFunc(const Ptr<Function> func);

private:
    void PrintWarning(const Expression& node, Block& block, std::set<Block*>& hasProcessed);
    /** Reports later cases only while their entries remain unreachable. */
    template <typename TConstDomain>
    void PrintMatchSuccessors(Results<TConstDomain>& result, Block& block, std::set<Block*>& hasProcessed);
    /** Groups unreachable enum-table fragments by source case. */
    template <typename TConstDomain>
    bool TryVisitEnumMatchTable(
        Results<TConstDomain>& result, MultiBranch& table, Block& target, std::set<Block*>& hasProcessed);
    /** Diagnoses excluded successors of Branch and MultiBranch using the same edge and entry-state rules. */
    template <typename TConstDomain>
    void VisitTerminator(
        Results<TConstDomain>& result, Expression& terminator, Block& target, std::set<Block*>& hasProcessed);

    template <typename TConstDomain>
    void VisitFunc(Results<TConstDomain>& result);

    DiagnosticEngine& diag;
    ConstAnalysisWrapper* analysisWrapper;

    const std::string& currentPackageName;
};

} // namespace Cangjie::CHIR

#endif
