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

#include <unordered_map>

namespace Cangjie::CHIR {

class UnreachableBranchCheck {
public:
    explicit UnreachableBranchCheck(
        ConstAnalysisWrapper* constAnalysisWrapper, DiagnosticEngine& diag, const std::string& packageName,
        const std::unordered_map<Block*, Expression*>& maybeUnreachableBlocks);

    void RunOnPackage(const Package& package, size_t threadNum);

    void RunOnFunc(const Ptr<Function> func);

private:
    void PrintWarning(const Expression& node, Block& block, std::set<Block*>& hasProcessed);
    void PrintMatchSuccessors(Block& block, std::set<Block*>& hasProcessed);
    void PrintMatchTableSuccessors(Block& block, std::set<Block*>& hasProcessed,
        std::set<size_t>& reportedCaseIds, const std::set<Block*>& constructorGroups);
    void PrintMatchTableCases(MultiBranch& table, std::set<Block*>& hasProcessed,
        std::set<size_t>& reportedCaseIds, const std::set<Block*>& constructorGroups);
    void VisitBranch(Branch& branch, Block& target, const std::set<Block*>& constructorGroups);
    void VisitMultiBranch(MultiBranch& branch, Block& target, const std::set<Block*>& constructorGroups);
    void VisitMultiBranchSuccessor(MultiBranch& branch, Block& successor, bool isOuterMatchTable,
        std::set<Block*>& hasProcessed, std::set<size_t>& reportedCaseIds,
        const std::set<Block*>& constructorGroups);

    template <typename TConstDomain>
    void VisitFunc(Results<TConstDomain>& result, const std::set<Block*>& constructorGroups);

    DiagnosticEngine& diag;
    ConstAnalysisWrapper* analysisWrapper;

    const std::string& currentPackageName;
    const std::unordered_map<Block*, Expression*>& maybeUnreachableBlocks;
};

} // namespace Cangjie::CHIR

#endif
