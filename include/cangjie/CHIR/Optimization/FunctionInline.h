// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares FunctionInline class for CHIR
 */

#ifndef CANGJIE_CHIR_TRANSFORMATION_FUNCTION_INLINE_H
#define CANGJIE_CHIR_TRANSFORMATION_FUNCTION_INLINE_H

#include <cstddef>

#include "cangjie/CHIR/IR/CHIRBuilder.h"
#include "cangjie/CHIR/IR/Expression/Terminator.h"
#include "cangjie/CHIR/IR/Package.h"
#include "cangjie/CHIR/Utils/Utils.h"
#include "cangjie/Option/Option.h"

namespace Cangjie::CHIR {
/**
 * CHIR Opt Pass: do function inline for CHIR IR.
 */
class FunctionInline {
public:
    /**
     * @brief constructor for function inline pass
     * @param builder CHIR builder for generating IR.
     * @param optLevel optimization level from Cangjie inputs.
     * @param debug flag whether print debug log.
     */
    FunctionInline(CHIRBuilder& builder, const GlobalOptions::OptimizationLevel& optLevel, bool debug)
        : builder(builder), optLevel(optLevel), debug(debug)
    {
    }

    /**
     * @brief Main process to do function inline.
     * @param func func to do function inline.
     */
    void Run(Function& func);

    /**
     * @brief Get effect map after this pass.
     * @return effect map affected by this pass.
     */
    const OptEffectCHIRMap& GetEffectMap() const;

    /**
     * @brief Main process to do function inline per apply.
     * @param apply apply expression to do function inline.
     * @param name pass name to pring log.
     */
    void DoFunctionInline(const Apply& apply, const std::string& name);

    /**
     * @brief Counters for inline decisions and attribute stamping (optsize/minsize/noinline).
     *        Aggregated across all FunctionInline instances within one compilation unit.
     *        Populated incrementally in later phases; only `inlineDecisions` is counted in Phase 0.
     */
    struct InlineStats {
        size_t inlineDecisions{0};   // total successful inlines (DoFunctionInline calls)
        size_t optsizeStamped{0};    // baseline(Os) + selective(O2 caller growth) - Phase 2
        size_t minsizeStamped{0};    // baseline(Oz) - Phase 2
        size_t noinlineStamped{0};   // blacklist closure(Phase 0) + callee budget veto(Phase 3)
    };
    static InlineStats& GetStats();
    static void PrintStatsSummary(bool debug);

private:
    bool CheckCanRewrite(const Apply& apply);
    void RecordEffectMap(const Apply& apply);
    void ReplaceFuncResult(LocalVar* resNew, LocalVar* resOld);

    std::pair<std::vector<Block*>, LocalVar*> CloneBlockGroupForInline(
        const BlockGroup& oldBG, const Apply& apply);

    void SetGroupDebugLocation(const std::vector<Block*>& blocks, const DebugLocation& loc);

    void InlineImpl(BlockGroup& bg);

    CHIRBuilder& builder;
    const GlobalOptions::OptimizationLevel& optLevel;
    bool debug{false};
    Function* globalFunc{nullptr};
    std::unordered_map<Function*, size_t> inlinedCountMap;
    std::unordered_map<Function*, size_t> funcSizeMap;
    const std::string optName{"Function Inline"};
    OptEffectCHIRMap effectMap;
};
} // namespace Cangjie::CHIR

#endif
