// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_CHIR_TRANSFORMATION_DEVIRTUALIZATION_H
#define CANGJIE_CHIR_TRANSFORMATION_DEVIRTUALIZATION_H

#include "cangjie/CHIR/Analysis/AnalysisWrapper.h"
#include "cangjie/CHIR/Analysis/DevirtualizationInfo.h"
#include "cangjie/CHIR/Analysis/TypeAnalysis.h"
#include "cangjie/CHIR/IR/Package.h"
#include "cangjie/CHIR/IR/Value/Value.h"
#include <unordered_map>

namespace Cangjie::CHIR {
struct FuncCallContext;

class Devirtualization {
public:
    /**
     * @brief wrapper for type analysis.
     */
    using TypeAnalysisWrapper = AnalysisWrapper<TypeAnalysis, TypeDomain>;

    /**
     * @brief rewrite info if a invoke can be de-virtualize.
     */
    struct RewriteInfo {
        InvokeBase* invoke;  // Invoke or TryInvoke
        Function* realCallee;
        Type* thisType;
    };

    Devirtualization() = delete;

    /**
     * @brief constructor for Devirtualization pass.
     * @param typeAnalysisWrapper
     * @param devirtFuncInfo
     * @param builder CHIR builder for generating IR.
     */
    explicit Devirtualization(TypeAnalysisWrapper* typeAnalysisWrapper, DevirtualizationInfo& devirtFuncInfo,
        CHIRBuilder& builder);

    /**
     * @brief main optimization pass entry.
     * @param funcs funcs to devirtualization.
     */
    void RunOnFuncs(const std::vector<Function*>& funcs);

    /// get optimized functions which are marked frozen.
    const std::vector<Function*>& GetFrozenInstFuns() const;

    /// after first devirt pass, do second devirtualization for frozen func.
    /// this function mainly get results from second type analysis.
    void AppendFrozenFuncState(const Function* func, std::unique_ptr<Results<TypeDomain>> analysisRes);

private:
    void RunOnFunc(const Function* func);

    Function* FindRealCallee(const TypeValue* typeState, const InvokeBase& invoke) const;

    /// Collect all transitive subtypes of @p specific from subtypeMap (instantiated).
    std::vector<Type*> CollectAllSubTypes(ClassType& specific) const;

    /// Create an instantiated function for @p func if possible; may clear @p context.instTypeArgs.
    Function* CreateInstFuncIfPossible(Function* func, FuncCallContext& context, Type& retType);

    void RewriteToApply(std::vector<RewriteInfo>& rewriteInfos);

    bool RewriteToBuiltinOp(const RewriteInfo& info);

    static bool CheckAllGenericTypeVisible(
        const Type& thisType, const std::unordered_set<const GenericType*>& visibleSet);

    TypeAnalysisWrapper* analysisWrapper;
    DevirtualizationInfo& devirtFuncInfo;
    CHIRBuilder& builder;
    std::vector<RewriteInfo> rewriteInfos{};

    // frozen inst functions after devirt, these func need a devirt optimization too after first devirt opt
    std::vector<Function*> frozenInstFuns;
    // extra type state from outside
    std::unordered_map<const Function*, std::unique_ptr<Results<TypeDomain>>> frozenStates;

    std::unordered_map<std::string, Function*> frozenInstFuncMap;
};
} // namespace Cangjie::CHIR

#endif