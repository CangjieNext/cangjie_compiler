// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements Tool Options functions.
 */

#include "cangjie/Driver/ToolOptions.h"

#include <unordered_map>

namespace {
using namespace Cangjie;
GlobalOptions::OptimizationLevel GetEffectiveOptimizationLevel(const DriverOptions& driverOptions)
{
    return driverOptions.disableBackendOpt ? GlobalOptions::OptimizationLevel::O0 : driverOptions.optimizationLevel;
}
} // namespace

namespace Cangjie::ToolOptions {

void SetOptionStringArgs(SetFuncType setOptionHandler, const std::string& stringArg)
{
    auto args = Utils::SplitString(stringArg, " ");
    for (const auto& a : args) {
        if (!a.empty()) {
            setOptionHandler(a);
        }
    }
}

void SetOptionIf(SetFuncType setOptionHandler, bool condition, const std::string& stringArg)
{
    if (condition) {
        setOptionHandler(stringArg);
    }
}

void SetMtripleOption(SetFuncType setOptionHandler, const DriverOptions& driverOptions)
{
    bool canSetMtriple = driverOptions.IsCrossCompiling() || driverOptions.target.IsMinGW();
    SetOptionIf(setOptionHandler, canSetMtriple, "--mtriple=" + driverOptions.target.GetEffectiveTripleString());
}

namespace OPT {
static constexpr const char* CJ_OPAQUE_DEFAULT_O2_PIPELINE =
    R"(verify,cj-ir-verifier,function(insert-cj-tbaa),cj-fill-metadata,cj-runtime-lowering,cj-barrier-split,function(cj-simple-opt),annotation2metadata,forceattrs,inferattrs,coro-early,function<eager-inv>(loop(loop-simplifycfg),lower-expect,simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;no-switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>,sroa,early-cse<>),openmp-opt,ipsccp,called-value-propagation,globalopt,function(mem2reg),deadargelim,function<eager-inv>(instcombine,simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>),require<globals-aa>,function(invalidate<aa>),require<profile-summary>,cgscc(devirt<4>(inline<only-mandatory>,inline,function-attrs,openmp-opt-cgscc,cj-devirtual-opt,function(simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>),function(instcombine),function(sccp),cj-pea,function<eager-inv>(cj-simple-opt,sroa,early-cse<memssa>,speculative-execution,jump-threading,correlated-propagation,cj-generic-intrinsic-opt,cj-simple-range-analysis,simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>,instcombine,libcalls-shrinkwrap,tailcallelim,simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>,reassociate,instcombine,require<opt-remark-emit>,loop-mssa(licm<no-allowspeculation>,loop-rotate,indvars,loop-deletion,loop-instsimplify,loop-simplifycfg,licm<no-allowspeculation>,loop-rotate,licm<allowspeculation>,simple-loop-unswitch<no-nontrivial;trivial>),simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>,instcombine,cj-loop-float-opt,loop(loop-idiom,indvars,loop-deletion,loop-unroll-full),sroa,mldst-motion<no-split-footer-bb>,gvn<>,sccp,bdce,instcombine,jump-threading,correlated-propagation,adce,memcpyopt,cj-rssce,dse,loop-mssa(licm<allowspeculation>),coro-elide,simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;hoist-common-insts;sink-common-insts>,instcombine),coro-split)),coro-cleanup,globalopt,globaldce,rpo-function-attrs,recompute-globalsaa,cgscc(function-attrs),function<eager-inv>(irce,simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>,loop(simple-loop-unswitch<no-nontrivial;trivial>),loop-mssa(licm<allowspeculation>),gvn<>,memcpyopt,cj-rssce,dse,mldst-motion<no-split-footer-bb>),function<eager-inv>(float2int,lower-constant-intrinsics,loop(loop-rotate,loop-deletion),loop-distribute,inject-tli-mappings,loop-vectorize<no-interleave-forced-only;no-vectorize-forced-only;>,loop-load-elim,instcombine,simplifycfg<bonus-inst-threshold=1;forward-switch-cond;switch-range-to-icmp;switch-to-lookup;no-keep-loops;hoist-common-insts;sink-common-insts>,slp-vectorizer,vector-combine,instcombine,loop-unroll<O2>,transform-warning,instcombine,require<opt-remark-emit>,loop-mssa(licm<allowspeculation>),alignment-from-assumptions,cj-after-inline-simple-opt,loop(indvars),early-cse<>,loop-sink,instsimplify,div-rem-pairs,tailcallelim,simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts>),elim-avail-extern,globaldce,constmerge,cg-profile,rel-lookup-table-converter,function(annotation-remarks),cj-specific-opt,place-safepoints,cj-barrier-opt,cj-rewrite-statepoint,verify)";

void SetOptimizationLevelOptions(SetFuncType setOptionHandler, const DriverOptions& driverOptions)
{
    auto optimizationLevel =
        driverOptions.disableBackendOpt ? GlobalOptions::OptimizationLevel::O0 : driverOptions.optimizationLevel;
    if (driverOptions.enableOpaque && optimizationLevel == GlobalOptions::OptimizationLevel::O2) {
        setOptionHandler(CJ_OPAQUE_DEFAULT_O2_PIPELINE);
        return;
    }
    setOptionHandler("default<" + OPTIMIZATION_LEVEL_TO_BACKEND_OPTION.find(optimizationLevel)->second.substr(1) + ">");
}

void SetNewPassManagerOptions(SetFuncType setOptionHandler, const DriverOptions& driverOptions)
{
    // the new pass manager only supports the following pass settings:
    // - optimization (in function SetOptimizationLevelOptions)
    // - coverage
    // - obfuscation (ObfData, ObfLayout, ObfControlFlow)
    // - optPassOptions
    SetOptionIf(setOptionHandler, driverOptions.enableCoverage, "-insert-gcov-profiling");
    SetOptionIf(setOptionHandler, driverOptions.EnableAsan(), "-asan-module");
    SetOptionIf(setOptionHandler, driverOptions.EnableTsan(), "-tsan");
    SetOptionIf(setOptionHandler, driverOptions.EnableHwAsan(), "-hwasan");
    SetOptionIf(setOptionHandler, (!driverOptions.optPassOptions.empty()), "-" + driverOptions.optPassOptions);
}

void SetCodeLayoutObfuscationOptions(SetFuncType setOptionHandler, const DriverOptions& driverOptions)
{
    setOptionHandler("--obf-layout=true");
    if (driverOptions.layoutObfInputSymMappingFiles.has_value()) {
        setOptionHandler(
            std::string("-obf-input-symbol-mapping-files=") + driverOptions.layoutObfInputSymMappingFiles.value());
    }
    if (driverOptions.layoutObfOutputSymMappingFile.has_value()) {
        setOptionHandler(
            std::string("-obf-output-symbol-mapping-file=") + driverOptions.layoutObfOutputSymMappingFile.value());
    }
    if (driverOptions.layoutObfUserMappingFile.has_value()) {
        setOptionHandler(
            std::string("-obf-apply-symbol-mapping=") + driverOptions.layoutObfUserMappingFile.value());
    }
    if (driverOptions.layoutObfSymPrefix.has_value()) {
        setOptionHandler(std::string("-obf-sym-prefix=") + driverOptions.layoutObfSymPrefix.value());
    }
    setOptionHandler(
        std::string("-obf-export-symbol=") + (driverOptions.enableObfExportSyms.value_or(true) ? "true" : "false"));
    setOptionHandler(
        std::string("-obf-line-number=") + (driverOptions.enableObfLineNumber.value_or(false) ? "true" : "false"));
    setOptionHandler(
        std::string("-obf-source-path=") + (driverOptions.enableObfSourcePath.value_or(true) ? "true" : "false"));
}

void SetCodeObfuscationOptions(SetFuncType setOptionHandler, const DriverOptions& driverOptions)
{
    bool obfEnabled = false;
    bool obfString = driverOptions.enableStringObfuscation.value_or(false);
    bool obfConst = driverOptions.enableConstObfuscation.value_or(false);
    bool obfLayout = driverOptions.enableLayoutObfuscation.value_or(false);
    bool obfCFFlatten = driverOptions.enableCFflattenObfuscation.value_or(false);
    bool obfCFBogus = driverOptions.enableCFBogusObfuscation.value_or(false);
    if (obfString || obfConst) {
        setOptionHandler(std::string("-obf-string=") + (obfString ? "true" : "false"));
        setOptionHandler(std::string("-obf-const=") + (obfConst ? "true" : "false"));
        obfEnabled = true;
    }
    if (obfLayout) {
        SetCodeLayoutObfuscationOptions(setOptionHandler, driverOptions);
        obfEnabled = true;
    }
    if (obfCFFlatten || obfCFBogus) {
        setOptionHandler(
            std::string("-control-flow-flatten=") + (obfCFFlatten ? "true" : "false"));
        setOptionHandler(
            std::string("-control-flow-bogus=") + (obfCFBogus ? "true" : "false"));
        obfEnabled = true;
    }
    if (obfEnabled) {
        setOptionHandler(std::string("-obf-level=") + std::to_string(driverOptions.obfuscationLevel));
        if (driverOptions.obfuscationSeed.has_value()) {
            setOptionHandler(std::string("-obf-seed=") + std::to_string(driverOptions.obfuscationSeed.value()));
        }
        if (driverOptions.obfuscationConfigFile.has_value()) {
            setOptionHandler(std::string("-obf-config-file=") + driverOptions.obfuscationConfigFile.value());
        }
    }
}

void SetLTOOptions(SetFuncType setOptionHandler, const DriverOptions& driverOptions)
{
    if (driverOptions.IsLTOEnabled()) {
        setOptionHandler("--cangjie-lto");
        setOptionHandler("--module-summary");
        setOptionHandler("--module-hash");
    }
}

void SetVerifyOptions(SetFuncType setOptionHandler, [[maybe_unused]] const DriverOptions& driverOptions)
{
    setOptionHandler("--only-verify-out");
}

void SetOptions(SetFuncType setOptionHandler, const DriverOptions& driverOptions)
{
    setOptionHandler("--cangjie-pipeline");
    SetOptionIf(setOptionHandler, driverOptions.EnableAsan(), "-cj-asan=true");
    SetOptionIf(setOptionHandler, driverOptions.EnableTsan(), "-cj-tsan=true");
    SetOptionIf(setOptionHandler, driverOptions.EnableTsan(), "-tsan-instrument-atomics=false");
    SetOptionIf(setOptionHandler, driverOptions.EnableTsan(), "--cj-tsan-support-for-mutexopt");
    SetOptionIf(setOptionHandler, driverOptions.EnableHwAsan(), "-cj-hwasan=true");
    SetOptionIf(setOptionHandler, driverOptions.enableFuncSections, "-function-sections");
    SetOptionIf(setOptionHandler, driverOptions.enableDataSections, "-data-sections");
    SetOptionIf(setOptionHandler,
        driverOptions.target.env == Triple::Environment::OHOS && driverOptions.target.arch == Triple::ArchType::AARCH64,
        "--cj-ptrauth-backward-cfi=true");
}

void SetTripleOptions(SetFuncType setOptionHandler, const DriverOptions& driverOptions)
{
    SetMtripleOption(setOptionHandler, driverOptions);
}

void SetTransparentOptions(SetFuncType setOptionHandler, const DriverOptions& driverOptions)
{
    // optArg may contain multiple opt arguments separated by spaces.
    SetOptionStringArgs(setOptionHandler, driverOptions.optArg);
}

void SetPgoOptions(SetFuncType setOptionHandler, const DriverOptions& driverOptions)
{
    if (driverOptions.enablePgoInstrGen) {
        setOptionHandler("-pgo-kind=pgo-instr-gen-pipeline");
        if (driverOptions.pgoProfileFile != "") {
            setOptionHandler("-profile-file=" + driverOptions.pgoProfileFile);
        }
        setOptionHandler("-runtime-counter-relocation");
    }

    if (driverOptions.enablePgoInstrUse) {
        setOptionHandler("-pgo-kind=pgo-instr-use-pipeline");
        setOptionHandler("-profile-file=" + driverOptions.pgoProfileFile);
    }
}

} // namespace OPT

namespace LLC {
void SetOptimizationLevelOptions(SetFuncType setOptionHandler, const DriverOptions& driverOptions)
{
    auto optimizationLevel = GetEffectiveOptimizationLevel(driverOptions);
    // when the optimize option '-Os' or 'Oz' is enabled in opt stage, then it will trigger '-O2' in llc stage.
    if (optimizationLevel == GlobalOptions::OptimizationLevel::Os ||
        optimizationLevel == GlobalOptions::OptimizationLevel::Oz) {
        optimizationLevel = GlobalOptions::OptimizationLevel::O2;
    }
    setOptionHandler(OPTIMIZATION_LEVEL_TO_BACKEND_OPTION.find(optimizationLevel)->second);
}

void SetOptions(SetFuncType setOptionHandler, const DriverOptions& driverOptions)
{
    setOptionHandler("--cangjie-pipeline");
    SetOptionIf(setOptionHandler,
        (!driverOptions.enableCompileDebug && !driverOptions.enableCoverage && driverOptions.displayLineInfo),
        "-disable-debug-info-print");
    setOptionHandler("--relocation-model=pic");
    if (driverOptions.target.arch == Triple::ArchType::ARM32 &&
        driverOptions.optimizationLevel == GlobalOptions::OptimizationLevel::O2) {
        setOptionHandler("--frame-pointer=all");
    } else {
        setOptionHandler("--frame-pointer=non-leaf");
    }
    setOptionHandler(driverOptions.GetStackTraceFormat());
    SetOptionIf(setOptionHandler, driverOptions.fastMathMode, "-fp-contract=fast");
    SetOptionIf(setOptionHandler, driverOptions.enableFuncSections, "-function-sections");
    SetOptionIf(setOptionHandler, driverOptions.enableDataSections, "-data-sections");
	// thread sanitizer pass for atomics instrument
    SetOptionIf(setOptionHandler, driverOptions.EnableTsan(), "-tsan");
    SetOptionIf(setOptionHandler, driverOptions.EnableHwAsan(), "-mattr=+tagged-globals");
#ifdef CANGJIE_DISABLE_STACK_GROW_FEATURE
    setOptionHandler("--cj-stack-grow=false");
#else
    SetOptionIf(setOptionHandler,
        driverOptions.target.env == Triple::Environment::OHOS ||
        driverOptions.target.env == Triple::Environment::ANDROID,
        "--cj-stack-grow=false");
#endif
    if (driverOptions.targetCPU.has_value() && !driverOptions.targetCPU.value().empty()) {
        setOptionHandler("-mcpu=" + driverOptions.targetCPU.value());
        // AVX instruction generation may cause runtime fail on stack tracing. Manually disable here.
        SetOptionIf(setOptionHandler, driverOptions.target.arch == Triple::ArchType::X86_64, "-mattr=-avx");
    }
    // safepoint outline improves codesize but may lead to performance degrade.
    SetOptionIf(setOptionHandler, GetEffectiveOptimizationLevel(driverOptions) == GlobalOptions::OptimizationLevel::O2,
        "--cj-safepoint-outline=false");
}

void SetTripleOptions(SetFuncType setOptionHandler, const DriverOptions& driverOptions)
{
    SetMtripleOption(setOptionHandler, driverOptions);
}

void SetTransparentOptions(SetFuncType setOptionHandler, const DriverOptions& driverOptions)
{
    // llcArg may contain multiple opt arguments separated by spaces.
    SetOptionStringArgs(setOptionHandler, driverOptions.llcArg);
}

} // namespace LLC

namespace LLD {
void SetLTOOptimizationLevelOptions(SetFuncType setOptionHandler, const DriverOptions& driverOptions)
{
    auto optimizationLevel = GetEffectiveOptimizationLevel(driverOptions);
    switch (optimizationLevel) {
        case GlobalOptions::OptimizationLevel::O3:
            setOptionHandler("--lto-O3");
            break;
        // when the optimize option '-Os' or 'Oz' is enabled in opt stage, then it will trigger '-O2' in llc stage.
        case GlobalOptions::OptimizationLevel::Os:
        case GlobalOptions::OptimizationLevel::Oz:
        case GlobalOptions::OptimizationLevel::O2:
            setOptionHandler("--lto-O2");
            break;
        case GlobalOptions::OptimizationLevel::O1:
            setOptionHandler("--lto-O1");
            break;
        case GlobalOptions::OptimizationLevel::O0:
            setOptionHandler("--lto-O0");
            break;
        default:
            setOptionHandler("--lto-O2");
            break;
    }
}

void SetLTOOptions(SetFuncType setOptionHandler, const DriverOptions& driverOptions)
{
    if (driverOptions.IsFullLTOEnabled()) {
        setOptionHandler("--mllvm");
        setOptionHandler("--cangjie-full-lto");
    } else {
        // The backend lld defaults to supporting incremental compilation in thin LTO mode.
        auto normalizedPath = Cangjie::FileUtil::NormalizePath(driverOptions.compilationCachedPath);
        auto absolutePath = Cangjie::FileUtil::GetAbsPath(normalizedPath);
        if (absolutePath.has_value()) {
            auto escapedPath = Cangjie::FileUtil::TransferEscapeBacktick(absolutePath.value());
            setOptionHandler("--thinlto-cache-dir=" + escapedPath);
        }
    }
    // safepoint outline improves codesize but may lead to performance degrade.
    if (GetEffectiveOptimizationLevel(driverOptions) == GlobalOptions::OptimizationLevel::O2) {
        setOptionHandler("--mllvm");
        setOptionHandler("--cj-safepoint-outline=false");
    }
    // Differentiate the optimization in the LLD and OPT phases.
    setOptionHandler("--mllvm");
    setOptionHandler("--cj-lto-opt");
    setOptionHandler("--allow-multiple-definition");
    if (driverOptions.IsCompileAsExeEnabled()) {
        setOptionHandler("--compile-as-exe");
    }
}

void SetPgoOptions(SetFuncType setOptionHandler, const DriverOptions& driverOptions)
{
    if (driverOptions.enablePgoInstrGen) {
        setOptionHandler("-cj-pgo-kind=pgo-instr-gen-pipeline");
        if (driverOptions.pgoProfileFile != "") {
            setOptionHandler("-cj-profile-file=" + driverOptions.pgoProfileFile);
        }
    }

    if (driverOptions.enablePgoInstrUse) {
        setOptionHandler("-cj-pgo-kind=pgo-instr-use-pipeline");
        setOptionHandler("-cj-profile-file=" + driverOptions.pgoProfileFile);
    }
}
} // namespace LLD

void SetOptions(SetFuncType setOptionHandler, const DriverOptions& driverOptions, const ToolOptionType typeFunc)
{
    typeFunc(setOptionHandler, driverOptions);
}

void SetOptions(
    SetFuncType setOptionHandler, const DriverOptions& driverOptions, const std::vector<ToolOptionType>& typeFuncs)
{
    for (const auto& typefunc : typeFuncs) {
        typefunc(setOptionHandler, driverOptions);
    }
}

} // namespace Cangjie::ToolOptions
