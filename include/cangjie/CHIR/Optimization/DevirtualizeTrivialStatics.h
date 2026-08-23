// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_CHIR_TRANSFORMATION_DEVIRTUALIZE_TRIVIAL_STATICS_H
#define CANGJIE_CHIR_TRANSFORMATION_DEVIRTUALIZE_TRIVIAL_STATICS_H

#include "cangjie/CHIR/Analysis/AnalysisWrapper.h"
#include "cangjie/CHIR/Analysis/DevirtualizationInfo.h"
#include "cangjie/CHIR/Analysis/TypeAnalysis.h"
#include "cangjie/CHIR/IR/Package.h"
#include "cangjie/CHIR/IR/Value/Value.h"
#include <unordered_map>

namespace Cangjie::CHIR {
class DevirtualizeTrivialStatics {
public:
    /**
     * @brief rewrite info if a invoke can be de-virtualize.
     */
    struct RewriteInfo {
        InvokeStatic* invokeStatic;
        Function* realCallee;
        Type* thisType;
        Apply* newApply = nullptr;
    };

    /**
     * @brief constructor for DevirtualizeTrivialStatics pass.
     */
    explicit DevirtualizeTrivialStatics() = default;

    /**
     * @brief main optimization pass entry.
     * @param package package to perform devirtualization.
     * @param builder CHIR builder for generating IR.
     * @param isDebug flag whether print debug log.
     */
    void RunOnPackage(const Package& package, CHIRBuilder& builder, bool isDebug);

private:
    void RunOnFunc(const Function* func, CHIRBuilder& builder);

    static void RewriteToApply(CHIRBuilder& builder, std::vector<RewriteInfo>& rewriteInfos, bool isDebug);

    std::vector<RewriteInfo> rewriteInfos{};
};
} // namespace Cangjie::CHIR

#endif