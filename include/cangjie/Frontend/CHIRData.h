// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares the CompilerInstance, which holds all compile context.
 */

#ifndef CANGJIE_FRONTEND_CHIRDATA_H
#define CANGJIE_FRONTEND_CHIRDATA_H

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

#include "cangjie/IncrementalCompilation/CompilationCache.h"

namespace Cangjie {
namespace AST {
struct StructTy;
}
namespace CHIR {
class CHIRContext;
class Package;
class Function;
class ConstAnalysisWrapper;
class CHIRBuilder;
}

class CHIRData {
public:
    CHIRData();
    ~CHIRData();

    void InitData(std::unordered_map<unsigned int, std::string>* fileNameMap, size_t threadNum);

    CHIR::CHIRContext& GetCHIRContext();

    void AppendNewPackage(CHIR::Package* package);
    std::vector<CHIR::Package*> GetAllCHIRPackages() const;
    CHIR::Package* GetCurrentCHIRPackage() const;

    void SetImplicitFuncs(const std::unordered_map<std::string, CHIR::Function*>& funcs);
    std::unordered_map<std::string, CHIR::Function*> GetImplicitFuncs() const;

    void SetConstVarInitFuncs(const std::vector<CHIR::Function*>& funcs);
    std::vector<CHIR::Function*> GetConstVarInitFuncs() const;

    CHIR::ConstAnalysisWrapper& GetConstAnalysisResultRef();
    const CHIR::ConstAnalysisWrapper& GetConstAnalysisResult() const;

    /**
     * @brief Manually destruct constAnalysisWrapper
     */
    void FreeConstAnalysisWrapper();

private:
    std::unique_ptr<CHIR::CHIRContext> cctx;
    std::vector<CHIR::Package*> chirPkgs;
    // used by codegen
    std::unordered_map<std::string, CHIR::Function*> implicitFuncs;
    // used by interpreter
    std::vector<CHIR::Function*> initFuncsForConstVar;
    // only for AnalysisWrapper
    std::unique_ptr<CHIR::CHIRBuilder> builder;
    // provide the capability and results of constant analysis, used by cjlint
    std::unique_ptr<CHIR::ConstAnalysisWrapper> constAnalysisWrapper;
};

/**
    * Infomation generated in CHIR stage
    */
    struct CHIRInfo {
    OptEffectStrMap optEffectMap;
    std::unordered_set<Ptr<const AST::StructTy>> ccTys;
    VirtualWrapperDepMap curVirtFuncWrapDep;
    VirtualWrapperDepMap delVirtFuncWrapForIncr;
    std::set<std::string> ccOutFuncsRawMangle;
    VarInitDepMap varInitDepMap;
};
}
#endif
