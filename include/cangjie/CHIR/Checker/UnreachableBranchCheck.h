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
    /** Walks later source cases after ConstAnalysis proves a match failure edge cannot be taken. */
    void PrintMatchSuccessors(Block& block, std::set<Block*>& hasProcessed);
    /** Walks one unreachable enum constructor group and deduplicates fragments of the same source case. */
    void PrintMatchTableSuccessors(Block& block, std::set<Block*>& hasProcessed,
        std::set<size_t>& reportedCaseIds, const std::set<Block*>& constructorGroups);
    /** Maps unreachable second-level enum table entries back to their source cases. */
    void PrintMatchTableCases(MultiBranch& table, std::set<Block*>& hasProcessed,
        std::set<size_t>& reportedCaseIds, const std::set<Block*>& constructorGroups);
    /** Handles a constant Branch result and follows source match-case boundaries when present. */
    void VisitBranch(Branch& branch, Block& target);
    /** Handles a constant MultiBranch result while preserving enum source-case grouping. */
    void VisitMultiBranch(MultiBranch& branch, Block& target);

    template <typename TConstDomain>
    void VisitFunc(Results<TConstDomain>& result);

    DiagnosticEngine& diag;
    ConstAnalysisWrapper* analysisWrapper;

    const std::string& currentPackageName;
};

} // namespace Cangjie::CHIR

#endif
