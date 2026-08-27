// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements function inline feature
 */

#include "cangjie/CHIR/Optimization/FunctionInline.h"

#include <algorithm>
#include <list>

#include "cangjie/CHIR/Utils/CHIRCasting.h"
#include "cangjie/CHIR/IR/Expression/Terminator.h"
#include "cangjie/CHIR/IR/Type/PrivateTypeConverter.h"
#include "cangjie/CHIR/IR/Type/Type.h"
#include "cangjie/CHIR/Utils/Utils.h"
#include "cangjie/CHIR/Utils/Visitor/Visitor.h"
#include "cangjie/CHIR/Optimization/BlockGroupCopyHelper.h"

using namespace Cangjie::CHIR;

// Set the threshold value of inlined call in one function to avoid code expansion
constexpr static size_t INLINED_COUNT_THRESHOLD = 20;
// This is for efficiency and avoid overflowing on `size`, we will
// terminate the counting if the size is already 3 times of the threshold.
// Bumped from 60 (=INIT*3) to 200 after the cost metric switched to asm-weighted
// counting (Phase 1): weighted per-expr cost is 0..4, so the old 60 cut-off starved
// medium functions. Kept as a hard cap to bound counting on huge functions.
constexpr static size_t SEARCH_THRESHOLD = 200;

// Phase 1: per-optLevel base inline threshold for the #4 front-end decision.
// Lower than the LLVM back-end thresholds (O2=225/Os=50/Oz=5) on purpose:
// the front-end #4 only inlines small functions to expose cross-
// function optimizations; the back-end owns the real machine-cost inlining decision.
constexpr static size_t BASE_THRESHOLD_O0 = 0;   // no front-end inline
constexpr static size_t BASE_THRESHOLD_O1 = 30;
constexpr static size_t BASE_THRESHOLD_O2 = 50;   // aligned with rustc inline_mir_threshold
constexpr static size_t BASE_THRESHOLD_O3 = 80;  // more aggressive than O2
constexpr static size_t BASE_THRESHOLD_OS = 24;  // ~half of O2 (size-aware)
constexpr static size_t BASE_THRESHOLD_OZ = 16;  // most conservative

// Bonus multipliers are expressed as fractions over the base threshold (numerator/
// denominator). The old code used `=` assignment which wiped earlier bonuses; we now
// scale multiplicatively and cap so bonuses compose instead of clobbering each other.
constexpr static size_t BONUS_NUMERATOR_ONLY_CALLED_ONCE = 25;   // +25%  (was +20% via /5)
constexpr static size_t BONUS_DENOMINATOR = 100;
constexpr static size_t BONUS_NUMERATOR_HOTSPOT = 25;            // +25%
constexpr static size_t BONUS_NUMERATOR_LAMBDA_ARG = 100;        // +100% (was *2)
// Hard cap on the post-bonus threshold so a tiny base cannot balloon unboundedly.
constexpr static size_t THRESHOLD_HARD_CAP = 200;

// --- Legacy (pre-Phase1) decision constants, used only under --chir-inline-legacy. ---
// Restored verbatim from the Phase 0 baseline so the legacy path is a faithful A/B reference.
constexpr static size_t LEGACY_INIT_INLINE_THRESHOLD = 20;
constexpr static size_t LEGACY_SEARCH_THRESHOLD = LEGACY_INIT_INLINE_THRESHOLD * 3;  // = 60
constexpr static size_t LEGACY_INCREASE_THRESHOLD = 5;            // +20% via /5
constexpr static size_t LEGACY_INCREASE_WHEN_CALLEE_WITH_LAMBDA_ARG = 2;  // *2

// Set the threshold of CHIR in one block for inline
constexpr static size_t INLINED_BLOCKSIZE_THRESHOLD = 10000;

static const std::vector<FuncInfo> functionInlineWhiteList = {
    FuncInfo("get", "Array", {NOT_CARE}, ANY_TYPE, "std.core"),
    FuncInfo("set", "Array", {NOT_CARE}, ANY_TYPE, "std.core"),
    FuncInfo("[]", "Array", {NOT_CARE}, ANY_TYPE, "std.core"),
    FuncInfo("copyTo", "Array", {NOT_CARE}, ANY_TYPE, "std.core"),
    FuncInfo("utf8Size", "Rune", {NOT_CARE}, ANY_TYPE, "std.core"),
    FuncInfo("[]", "String", {NOT_CARE}, ANY_TYPE, "std.core"),
    FuncInfo("init", "ArrayList", {NOT_CARE}, ANY_TYPE, "std.collection"),
    FuncInfo("get", "ArrayList", {NOT_CARE}, ANY_TYPE, "std.collection"),
    FuncInfo("set", "ArrayList", {NOT_CARE}, ANY_TYPE, "std.collection"),
    FuncInfo("append", "ArrayList", {NOT_CARE}, ANY_TYPE, "std.collection"),
    FuncInfo("[]", "ArrayList", {NOT_CARE}, ANY_TYPE, "std.collection"),
    FuncInfo("checkRange", "ArrayList", {NOT_CARE}, ANY_TYPE, "std.collection"),
    FuncInfo("remove", "ArrayList", {NOT_CARE}, ANY_TYPE, "std.collection"),
    FuncInfo("==", "HashSet", {NOT_CARE}, ANY_TYPE, "std.collection"),
    FuncInfo("!=", "HashSet", {NOT_CARE}, ANY_TYPE, "std.collection"),
};

static const std::vector<FuncInfo> functionInlineBlackList = {
    // Do not inline the `Future::init` for the conveninence of redundant future removal optimization
    FuncInfo("init", "Future", {NOT_CARE}, ANY_TYPE, "std.core"),
    // Do not inline the `arrayInitByFunction` for the conveninence of array lambda optimization
    FuncInfo("arrayInitByFunction", "", {NOT_CARE}, NOT_CARE, "std.core"),
    // Temporary disable the inline of `callNativeFunc` due to a weird bug in Windows
    FuncInfo("callNativeFunc", NOT_CARE, {NOT_CARE}, NOT_CARE, "std.fs"),
    // inline wrappingShl in for loop can result in signficant performance hit
    FuncInfo("wrappingShl", NOT_CARE, {NOT_CARE}, ANY_TYPE, "std.overflow"),
    FuncInfo("wrappingShr", NOT_CARE, {NOT_CARE}, ANY_TYPE, "std.overflow"),
};

void FunctionInline::InlineImpl(BlockGroup& bg)
{
    auto preVisit = [this](Expression& e) {
        if (e.GetExprKind() != ExprKind::APPLY) {
            return VisitResult::CONTINUE;
        }

        auto& apply = StaticCast<Apply&>(e);
        if (CheckCanRewrite(apply)) {
            DoFunctionInline(apply, optName);
        }

        return VisitResult::CONTINUE;
    };
    Visitor::Visit(bg, preVisit);
}

FunctionInline::InlineStats& FunctionInline::GetStats()
{
    // Meyers singleton: function-local static, thread-safe since C++11.
    // Aggregates across all FunctionInline instances in one compilation unit.
    static InlineStats stats;
    return stats;
}

void FunctionInline::PrintStatsSummary(bool dbg)
{
    if (!dbg) {
        return;
    }
    auto& s = GetStats();
    std::cout << "[FunctionInline] decisions=" << s.inlineDecisions
              << " optsize=" << s.optsizeStamped
              << " minsize=" << s.minsizeStamped
              << " noinline=" << s.noinlineStamped << std::endl;
}

void FunctionInline::Run(Function& func)
{
    globalFunc = &func;
    InlineImpl(*func.GetBody());
}

const OptEffectCHIRMap& FunctionInline::GetEffectMap() const
{
    return effectMap;
}

static bool InBlackList(const Function& func)
{
    if (func.GetFuncKind() == FuncKind::MACRO_FUNC || func.GetFuncKind() == FuncKind::GLOBALVAR_INIT ||
        func.GetFuncKind() == FuncKind::MAIN_ENTRY) {
        return true;
    }
    if (func.GetFuncType()->IsCFunc()) {
        return true;
    }
    if (func.TestAttr(Attribute::NO_INLINE)) {
        return true;
    }

    for (auto element : functionInlineBlackList) {
        if (IsExpectedFunction(func, element)) {
            return true;
        }
    }

    return false;
}

static bool InWhiteList(const Function& func)
{
    for (auto element : functionInlineWhiteList) {
        if (IsExpectedFunction(func, element)) {
            return true;
        }
    }
    return false;
}

static bool OnlyCalledOnce(const Function& func)
{
    bool alreadyHasUser = false;
    for (auto user : func.GetUsers()) {
        if (Cangjie::Is<ApplyBase>(user)) {
            if (alreadyHasUser) {
                return false;
            }
            alreadyHasUser = true;
        }
    }
    return true;
}

static bool IsHotSpotCall(const Function& callee)
{
    // Case A: the callee is operator overloading function like `[]`, `+`, `-`
    if (callee.TestAttr(Attribute::OPERATOR)) {
        return true;
    }
    // Case B: the call site is located in a loop
    // note: currently unsupported
    return false;
}

static bool FunctionWithLambdaArg(const Function& func)
{
    for (auto arg : func.GetParams()) {
        if (arg->GetType()->IsFunc()) {
            return true;
        }
    }
    return false;
}

// Phase 1: base inline threshold by optLevel, with optional CLI override.
// Honors --chir-inline-threshold=base_<level>=<val> from the global options' override map.
static size_t GetBaseThreshold(const Cangjie::GlobalOptions::OptimizationLevel& optLevel,
                               const std::unordered_map<std::string, size_t>& overrides)
{
    size_t base = 0;
    switch (optLevel) {
        case Cangjie::GlobalOptions::OptimizationLevel::O0: base = BASE_THRESHOLD_O0; break;
        case Cangjie::GlobalOptions::OptimizationLevel::O1: base = BASE_THRESHOLD_O1; break;
        case Cangjie::GlobalOptions::OptimizationLevel::O2: base = BASE_THRESHOLD_O2; break;
        case Cangjie::GlobalOptions::OptimizationLevel::O3: base = BASE_THRESHOLD_O3; break;
        case Cangjie::GlobalOptions::OptimizationLevel::Os: base = BASE_THRESHOLD_OS; break;
        case Cangjie::GlobalOptions::OptimizationLevel::Oz: base = BASE_THRESHOLD_OZ; break;
        default: break;
    }
    // CLI override (Phase 0 data channel): key "base_o0".."base_oz", lowercased by the parser.
    static const std::unordered_map<Cangjie::GlobalOptions::OptimizationLevel, std::string> keyMap = {
        {Cangjie::GlobalOptions::OptimizationLevel::O0, "base_o0"},
        {Cangjie::GlobalOptions::OptimizationLevel::O1, "base_o1"},
        {Cangjie::GlobalOptions::OptimizationLevel::O2, "base_o2"},
        {Cangjie::GlobalOptions::OptimizationLevel::O3, "base_o3"},
        {Cangjie::GlobalOptions::OptimizationLevel::Os, "base_os"},
        {Cangjie::GlobalOptions::OptimizationLevel::Oz, "base_oz"},
    };
    auto it = overrides.find(keyMap.at(optLevel));
    if (it != overrides.end()) {
        base = it->second;
    }
    return base;
}

// Phase 1: compose bonuses multiplicatively over the base threshold, then cap.
// Replaces the old `=`-assignment logic in CalculateThreshold which wiped earlier
// bonuses (OnlyCalledOnce, HotSpot, LambdaArg were mutually exclusive due to overwrite).
// Bonuses only apply below -Os (the original guard), where speed is preferred.
static size_t ApplyBonusAdjustments(size_t base, const Function& func,
                                    const Cangjie::GlobalOptions::OptimizationLevel& optLevel)
{
    if (base == 0) {
        return 0;  // O0: no front-end inline regardless of bonuses.
    }
    if (optLevel >= Cangjie::GlobalOptions::OptimizationLevel::Os) {
        return std::min(base, THRESHOLD_HARD_CAP);  // size levels: no speed bonuses.
    }
    // Each bonus is an additive fraction of the base; they stack (no longer clobber).
    size_t numerator = 0;
    if (OnlyCalledOnce(func)) {
        numerator += BONUS_NUMERATOR_ONLY_CALLED_ONCE;
    }
    if (IsHotSpotCall(func)) {
        numerator += BONUS_NUMERATOR_HOTSPOT;
    }
    if (FunctionWithLambdaArg(func)) {
        numerator += BONUS_NUMERATOR_LAMBDA_ARG;
    }
    size_t adjusted = base + base * numerator / BONUS_DENOMINATOR;
    return std::min(adjusted, THRESHOLD_HARD_CAP);
}

// --- Legacy (pre-Phase1) decision path, active under --chir-inline-legacy. ---
// Verbatim from the Phase 0 baseline: fixed threshold 20, `=`-assignment bonuses
// (so HotSpot/LambdaArg clobber an earlier OnlyCalledOnce bonus), flat per-expr
// cost (each expression counts 1, lambda body recurses). Kept as a faithful A/B
// reference so regressions can be attributed to Phase1 weighting/threshold changes
// vs. something else.
//
// Override channel: under --chir-inline-legacy, `--chir-inline-threshold=legacy=N`
// replaces the fixed base 20, and every bonus computation derives from N too, so
// the override truly scales the legacy path rather than only seeding the initial
// value. Without the override, the legacy path is byte-for-byte the Phase 0 baseline.
static size_t LegacyCalculateThreshold(const Value& callee,
                                        const Cangjie::GlobalOptions::OptimizationLevel& optLevel,
                                        const std::unordered_map<std::string, size_t>& overrides)
{
    size_t effectiveBase = LEGACY_INIT_INLINE_THRESHOLD;
    auto it = overrides.find("legacy");
    if (it != overrides.end()) {
        effectiveBase = it->second;
    }
    size_t realThreshold = effectiveBase;
    const auto& func = Cangjie::StaticCast<const Function&>(callee);
    if (OnlyCalledOnce(func)) {
        // Increase the threshold value by 20%
        realThreshold += realThreshold / LEGACY_INCREASE_THRESHOLD;
    }
    if (optLevel < Cangjie::GlobalOptions::OptimizationLevel::Os) {
        if (IsHotSpotCall(func)) {
            // Increase the threshold value by 20%  (note: `=` overwrites the OnlyCalledOnce bonus above)
            realThreshold = effectiveBase + effectiveBase / LEGACY_INCREASE_THRESHOLD;
        }
        if (FunctionWithLambdaArg(func)) {
            realThreshold = effectiveBase * LEGACY_INCREASE_WHEN_CALLEE_WITH_LAMBDA_ARG;
        }
    }
    return realThreshold;
}

static size_t LegacyGetExprSize(const Expression& expr)
{
    size_t exprSize = 0;
    if (expr.GetExprKind() != ExprKind::LAMBDA) {
        return ++exprSize;
    }
    auto postVisit = [&exprSize](Expression&) {
        exprSize++;
        return VisitResult::CONTINUE;
    };
    Visitor::Visit(*Cangjie::StaticCast<const Lambda&>(expr).GetBody(), postVisit);
    return exprSize;
}

static size_t LegacyCountFuncSize(const Function& func)
{
    size_t funcSize = 0;
    for (auto block : func.GetBody()->GetBlocks()) {
        for (auto e : block->GetExpressions()) {
            funcSize += LegacyGetExprSize(*e);
            if (funcSize >= LEGACY_SEARCH_THRESHOLD) {
                break;
            }
        }
    }
    return funcSize;
}

// Phase 1: asm-footprint-approximated weighted cost per expression, dispatched by ExprKind.
// Replaces GetExprSize which charged a flat 1 for every expression. Weights are a proxy
// (CHIR->asm is not 1:1 due to mem2reg/instcombine/CodeGen), but they only drive the
// "small enough to inline" #4 decision and attribute stamping; the LLVM back-end retains
// the real machine-cost inlining decision. Enum names verified against ExprKind.inc.
static size_t GetAsmWeightedCost(const Expression& expr)
{
    switch (expr.GetExprKind()) {
        case ExprKind::APPLY:        // arg setup(mov*arity) + call + ret ~3-5
        case ExprKind::TRY_APPLY:    // exception variant, same weight
            return 4;
        case ExprKind::LAMBDA: {
            // closure = function body + captured env; recurse over the body (existing path),
            // plus a floor for env construction.
            size_t cost = 4;
            auto postVisit = [&cost](Expression&) {
                cost += 1;
                return VisitResult::CONTINUE;
            };
            Visitor::Visit(*Cangjie::StaticCast<const Lambda&>(expr).GetBody(), postVisit);
            return cost;
        }
        case ExprKind::ALLOCATE:     // stack slot; mem2reg often removes it
        case ExprKind::LOAD:
        case ExprKind::STORE:
        case ExprKind::GOTO:
        case ExprKind::BRANCH:
        case ExprKind::MULTIBRANCH:
            return 1;
        case ExprKind::BOX:          // alloc + store pointer ~2-3
        case ExprKind::UNBOX_TO_VALUE:
        case ExprKind::UNBOX_TO_REF:
        case ExprKind::CAST_TO_GENERIC:
        case ExprKind::CAST_TO_CONCRETE:
            return 2;
        case ExprKind::CONSTANT:     // usually folded as immediate into the user
            return 0;
        default:                      // arithmetic/comparison/bitwise ~1
            return 1;
    }
}

static size_t CountFuncSize(const Function& func)
{
    size_t funcSize = 0;
    for (auto block : func.GetBody()->GetBlocks()) {
        for (auto e : block->GetExpressions()) {
            funcSize += GetAsmWeightedCost(*e);
            if (funcSize >= SEARCH_THRESHOLD) {
                break;
            }
        }
    }
    return funcSize;
}

bool FunctionInline::CheckCanRewrite(const Apply& apply)
{
    auto callee = apply.GetCallee();
    // imported func decl, intrinsic func decl, foreign func decl are excluded
    if (!callee->IsFuncWithBody()) {
        return false;
    }
    auto func = StaticCast<Function*>(callee);
    CJC_NULLPTR_CHECK(callee);

    // when the terminator of this block is RaiseException, do not inline this apply because it rarely happens
    if (auto block = apply.GetParentBlock(); Is<RaiseException>(block->GetTerminator())) {
        return false;
    }

    // Omit the function inline in block that exceed the Blocksize
    // threshold to avoid the huge time consume.
    auto block = apply.GetParentBlock();
    if (block->GetExpressions().size() >= INLINED_BLOCKSIZE_THRESHOLD) {
        return false;
    }

    // recursive function doesn't need to inline
    // if you really want to inline it, yes, you can, it won't cause a problem
    if (callee == globalFunc) {
        return false;
    }
    if (InBlackList(*func)) {
        // Closure: CHIR rejected this callee (blacklist vector: Future::init etc., C func,
        // MAIN_ENTRY, already-NO_INLINE). Stamp NO_INLINE so the LLVM backend honors the
        // same decision (CGModule::DeclareLLVMFunction bridges it). Without this, the backend
        // could re-inline e.g. Future::init and erase the anchor RedundantFutureRemoval relies on.
        func->EnableAttr(Attribute::NO_INLINE);
        return false;
    }
    if (InWhiteList(*func)) {
        return true;
    }
    if (func->GetFuncKind() == FuncKind::INSTANCEVAR_INIT) {
        return true;
    }
    // @ObjCMirror functions are generated and always have the same body
    // that calls relevant native code
    // according to benchmarks, they can always be inlined
    if (func->TestAttr(Attribute::OBJ_C_MIRROR)) {
        return true;
    }
    // Determine if we can inline by checking the size of callee exceed the threshold
    if (inlinedCountMap[globalFunc] >= INLINED_COUNT_THRESHOLD) {
        return false;
    }
    // `res` is std::pair<iterator, bool>, so
    // `res.second == true` means `callee` is emplaced successfully, then we must set correct function size
    // `res.second == false` means `callee` has already been emplaced before, we can use its size directly
    auto res = funcSizeMap.emplace(func, 0);
    size_t realThreshold = 0;
    if (globalOpts.chirInlineLegacy) {
        // Legacy path: flat per-expr cost + fixed threshold 20 + `=` bonuses.
        if (res.second) {
            res.first->second = LegacyCountFuncSize(*func);
        }
        realThreshold = LegacyCalculateThreshold(*callee, optLevel, globalOpts.inlineThresholdOverrides);
    } else {
        // Phase 1 path: asm-weighted cost + graded threshold + multiplicative bonuses.
        size_t baseThreshold = GetBaseThreshold(optLevel, globalOpts.inlineThresholdOverrides);
        realThreshold = ApplyBonusAdjustments(baseThreshold, *func, optLevel);
        if (res.second) {
            res.first->second = CountFuncSize(*func);
        }
    }
    if (res.first->second <= realThreshold) {
        inlinedCountMap[globalFunc]++;
        return true;
    }
    return false;
}

static Block* GetEntryBlock(const BlockGroup& oldBG, std::vector<Block*>& blocks)
{
    auto oldBlocks = oldBG.GetBlocks();
    CJC_ASSERT(oldBlocks.size() == blocks.size());
    CJC_ASSERT(!blocks.empty());
    for (size_t i = 0; i < oldBlocks.size(); ++i) {
        if (oldBlocks[i] == oldBG.GetEntryBlock()) {
            return blocks[i];
        }
    }
    CJC_ABORT();
    return blocks.front();
}

static std::vector<Block*> GetExitBlocks(std::vector<Block*>& blocks)
{
    std::vector<Block*> exitBlocks;
    for (auto block : blocks) {
        auto term = block->GetTerminator();
        CJC_NULLPTR_CHECK(term);
        if (term->GetExprKind() == ExprKind::EXIT) {
            // Need refactor
            exitBlocks.emplace_back(block);
        }
    }
    // there may be not exit block, all block's terminator is `Raise`
    return exitBlocks;
}

static void ReplaceFuncArgs(
    std::vector<Parameter*>& src, std::vector<Value*>& dst, const std::vector<Block*>& newBlocks)
{
    CJC_ASSERT(src.size() == dst.size());
    std::unordered_set<Block*> allNewBlocks;
    auto preVisit = [&allNewBlocks](Block& block) {
        allNewBlocks.emplace(&block);
        return VisitResult::CONTINUE;
    };
    for (auto block : newBlocks) {
        Visitor::Visit(*block, preVisit);
    }
    for (size_t i = 0; i < src.size(); ++i) {
        for (auto user : src[i]->GetUsers()) {
            if (allNewBlocks.count(user->GetParentBlock()) == 0) {
                continue;
            }
            user->ReplaceOperand(src[i], dst[i]);
        }
    }
}

void FunctionInline::ReplaceFuncResult(LocalVar* resNew, LocalVar* resOld)
{
    auto valType = resNew->GetType();
    CJC_ASSERT(valType->IsRef());
    auto users = resOld->GetUsers();
    // Don't need to handle it when the result of apply is not used.
    if (users.size() == 0) {
        return;
    }
    valType = Cangjie::StaticCast<RefType*>(valType)->GetBaseType();
    // Insert a load from the new local variable of result at the end of Exit Block of Inlined function.
    // Replace the use of the old local variable of result with the result of load.
    for (auto user : users) {
        auto newLoad =
            builder.CreateExpression<Load>(user->GetDebugLocation(), valType, resNew, user->GetParentBlock());
        newLoad->MoveBefore(user);
        user->ReplaceOperand(resOld, newLoad->GetResult());
        auto cast = TypeCastOrBoxIfNeeded(
            *newLoad->GetResult(), *resOld->GetType(), builder, *newLoad->GetParentBlock(), INVALID_LOCATION);
        if (cast != newLoad->GetResult()) {
            CJC_ASSERT(cast->IsLocalVar());
            StaticCast<LocalVar*>(cast)->GetExpr()->MoveBefore(user);
            user->ReplaceOperand(newLoad->GetResult(), cast);
        }
    }
    return;
}

void FunctionInline::SetGroupDebugLocation(const std::vector<Block*>& blocks, const DebugLocation& loc)
{
    auto changeLoc = [&loc](Expression& expr) {
        expr.SetDebugLocation(loc);
        return VisitResult::CONTINUE;
    };
    for (auto block : blocks) {
        Visitor::Visit(*block, changeLoc);
    }
}

void FunctionInline::DoFunctionInline(const Apply& apply, const std::string& name)
{
    RecordEffectMap(apply);
    PrintOptInfo(apply, debug, name);
    ++GetStats().inlineDecisions;
    // inline foo1 to foo2, `apply` node belongs to foo2, apply's callee is foo1, for example:
    // func foo1(param: Int64) { return param }
    // func foo2() { foo1(2) }
    // CHIR graph is like:
    // Function foo1(%7: Int64) { // Block Group: 1
    // Block #7:
    //   %8: Unit = Debug(%7, param)
    //   [ret]%9: Int64& = Allocate(Int64)
    //   %10: Unit = Store(%7, %9)
    //   Exit()
    // }
    // Function foo2 { // Block Group: 2
    // Block #14:
    //   %[ret]%11: Int64& = Allocate(Int64)
    //   %12: Int64 = ConstantInt(2)
    //   %13: Int64 = Apply(@_CN7default4foo1El, %12)  // triggered function inline
    //   %14: Unit = Store(%13, %11)
    //   Exit()
    // }

    // step 1: clone function body
    // `oldFuncGroup` is `Block Group: 1`
    BlockGroup* oldFuncGroup = nullptr;
    std::vector<Parameter*> funcArgs;
    if (apply.GetCallee()->IsFuncWithBody()) {
        auto func = StaticCast<Function*>(apply.GetCallee());
        oldFuncGroup = func->GetBody();
        funcArgs = func->GetParams();
    } else {
        CJC_ASSERT(apply.GetCallee()->IsLocalVar());
        auto lambda = DynamicCast<Lambda*>(StaticCast<LocalVar*>(apply.GetCallee())->GetExpr());
        CJC_NULLPTR_CHECK(lambda);
        oldFuncGroup = lambda->GetBody();
        funcArgs = lambda->GetParams();
    }
    CJC_NULLPTR_CHECK(oldFuncGroup->GetTopLevelFunc());
    // after cloned, we get a new block group, but func arg is value node, can't be cloned:
    // { // Block Group: 7
    // Block #27:
    //   %19: Unit = Debug(%7, param)
    //   %20: Int64& = Allocate(Int64)
    //   %21: Unit = Store(%7, %20)
    //   Exit()
    // }
    // `newFuncGroup` is `Block Group: 7`
    auto [newBlocks, returnVal] = CloneBlockGroupForInline(*oldFuncGroup, apply);
    SetGroupDebugLocation(newBlocks, apply.GetDebugLocation());
    // `funcEntry` is `Block #27`
    auto funcEntry = GetEntryBlock(*oldFuncGroup, newBlocks);
    // `exitBlock` is `Block #27`, `returnVal` is `%20`
    auto exitBlocks = GetExitBlocks(newBlocks);

    // step 2: change connection of func args
    // Function foo2 { // Block Group: 4
    // Block #14:
    //   [ret] %11: Int64& = Allocate(Int64)
    //   %12: Int64 = Constant(2)
    //   %13: Int64 = Apply(@_CN7default4foo1El, %12)
    //   %14: Unit = Store(%13, %11)
    //   Exit()
    // Block #27:
    //   %19: Unit = Debug(%12, param)
    //   %20: Int64& = Allocate(Int64)
    //   %21: Unit = Store(%12, %20)    // func arg %7 -> %12
    //   Exit()
    // }
    auto applyArgs = apply.GetArgs();
    ReplaceFuncArgs(funcArgs, applyArgs, newBlocks);

    FixCastProblemAfterInst(newBlocks, builder);

    // if callee must throw exception, then we can't get return value
    // step 4 : Insert a load for return value of inlined function and replace the use
    // Function foo2 { // Block Group: 4
    // Block #14:
    //   [ret] %11: Int64& = Allocate(Int64)
    //   %12: Int64 = Constant(2)
    //   %13: Int64 = Apply(@_CN7default4foo1El, %12)
    //   %22: Int64 = Load(%20)      // Insert Load before the use of result
    //   %14: Unit = Store(%22, %11) // Replace the use of old return value %13 -> %22
    // Block #27:
    //   %19: Unit = Debug(%12, param)
    //   %20: Int64& = Allocate(Int64)
    //   Exit()                      // add terminator to goto block 2: #28
    // }
    if (returnVal != nullptr) {
        ReplaceFuncResult(returnVal, StaticCast<LocalVar*>(apply.GetResult()));
    }
    // step 5: split block to 2 pieces and remove `apply` node
    // Function foo2 { // Block Group: 4
    // Block #14:                       // `Block #14` is split to `Block #14` and `Block #28`
    //   [ret] %11: Int64& = Allocate(Int64)
    //   %12: Int64 = Constant(2)
    //   GoTo(#28)
    // Block #27:
    //   %19: Unit = Debug(%12, param)
    //   %20: Int64& = Allocate(Int64)
    //   %21: Unit = Store(%12, %20)
    //   Exit()
    // Block #28:
    //   %22: Int64 = Load(%20)
    //   %14: Unit = Store(%22, %11)
    //   Exit()
    // }
    auto [block1, block2] = builder.SplitBlock(apply);

    // step 6: change connection in `Block Group 4`
    // Function foo2 { // Block Group: 4
    // Block #21:
    //   [ret] %11: Int64& = Allocate(Int64)
    //   %12: Int64 = Constant(2)
    //   GoTo(#27)               // change successor #28 -> #27
    // Block #27:
    //   %19: Unit = Debug(%12, param)
    //   %20: Int64& = Allocate(Int64)
    //   %21: Unit = Store(%12, %20)
    //   Goto(#28)
    // Block #28:
    //   %22: Int64 = Load(%20)
    //   %14: Unit = Store(%22, %11)
    //   Exit()
    // }
    CJC_NULLPTR_CHECK(block1->GetTerminator());
    block1->GetTerminator()->ReplaceOperand(block2, funcEntry);
    // if callee must throw exception, then we can't get `exitBlock`
    for (auto exitBlock : exitBlocks) {
        if (exitBlock != nullptr) {
            CJC_NULLPTR_CHECK(exitBlock->GetTerminator());
            exitBlock->GetTerminator()->RemoveSelfFromBlock();
            auto term2 = builder.CreateTerminator<GoTo>(block2, exitBlock);
            exitBlock->AppendExpression(term2);
        }
    }
}

void FunctionInline::RecordEffectMap(const Apply& apply)
{
    // `callee` may be a lambda, we only record global function
    if (!apply.GetCallee()->IsFuncWithBody()) {
        return;
    }
    auto callee = StaticCast<Function*>(apply.GetCallee());
    auto parentFunc = apply.GetTopLevelFunc();
    CJC_NULLPTR_CHECK(parentFunc);
    if (!callee->IsLambda() && !parentFunc->IsLambda()) {
        effectMap[callee].emplace(parentFunc);
    }
}

// Clone a new block group for Function Inline and return the new local variableCHIRBuilder::CloneBlock
// for function result value.
std::pair<std::vector<Block*>, LocalVar*> FunctionInline::CloneBlockGroupForInline(
    const BlockGroup& oldBG, const Apply& apply)
{
    BlockGroupCopyHelper helper(builder);
    helper.GetInstMapFromApply(apply);
    return helper.CloneBlockGroup(oldBG, *apply.GetParentBlockGroup());
}
