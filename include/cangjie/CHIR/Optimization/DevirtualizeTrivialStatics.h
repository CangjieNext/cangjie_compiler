// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
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
     * @brief wrapper for type analysis.
     */
    using TypeAnalysisWrapper = AnalysisWrapper<TypeAnalysis, TypeDomain>;

    /**
     * @brief rewrite info if a invoke can be de-virtualize.
     */
    struct RewriteInfo {
        InvokeStatic* invokeStatic;
        Function* realCallee;
        Type* thisType;
        std::vector<Type*> typeArgs;
        Apply* newApply = nullptr;
    };

    /**
     * @brief constructor for DevirtualizeTrivialStatics pass.
     * @param typeAnalysisWrapper
     * @param devirtFuncInfo
     */
    explicit DevirtualizeTrivialStatics() = default;

    /**
     * @brief main optimization pass entry.
     * @param funcs funcs to devirtualization.
     * @param builder CHIR builder for generating IR.
     * @param isDebug flag whether print debug log.
     */
    void RunOnFuncs(const std::vector<Function*>& funcs, CHIRBuilder& builder, bool isDebug);

    /**
     * @brief get functions containing invoke expression.
     * @param package user package to optimization.
     * @return return functions containing invoke expression.
     */
    static std::vector<Function*> CollectContainInvokeExprFuncs(const Ptr<const Package>& package);

    /// get optimized functions which are marked frozen.
    const std::vector<Function*>& GetFrozenInstFuns() const;

    /// after first devirt pass, do second devirtualization for frozen func.
    /// this function mainly get results from second type analysis.
    void AppendFrozenFuncState(const Function* func, std::unique_ptr<Results<TypeDomain>> analysisRes);

    /// function signature to determine a certain function.
    struct FuncSig {
        std::string name;
        std::vector<Type*> types;
        std::vector<Type*> typeArgs;
    };

private:
    void RunOnFunc(const Function* func, CHIRBuilder& builder);

    void InstantiateFuncIfPossible(CHIRBuilder& builder, std::vector<RewriteInfo>& rewriteInfoList);

    static void RewriteToApply(CHIRBuilder& builder, std::vector<RewriteInfo>& rewriteInfos, bool isDebug);

    /**
     * check func whether has invoke expression, implement func for CollectContainInvokeExprFuncs
     */
    static bool CheckFuncHasInvoke(const BlockGroup& bg);

    static bool CheckAllGenericTypeVisible(
        const Type& thisType, const std::unordered_set<const GenericType*>& visibleSet);

    std::vector<RewriteInfo> rewriteInfos{};

    // frozen inst functions after devirt, these func need a devirt optimization too after first devirt opt
    std::vector<Function*> frozenInstFuns;
    // extra type state from outside
    std::unordered_map<const Function*, std::unique_ptr<Results<TypeDomain>>> frozenStates;

    std::unordered_map<std::string, Function*> frozenInstFuncMap;
};
} // namespace Cangjie::CHIR

#endif