// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_IROPTIMIZER_H
#define CANGJIE_IROPTIMIZER_H

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/MemorySSA.h"

namespace Cangjie::CodeGen {
class CGModule;
class CGFunction;
class LICMOptimizer {
public:
    LICMOptimizer(CGModule& cgMod, const CGFunction& cgFunc)
        : cgMod(cgMod), cgFunc(cgFunc), dtAndLoopInfo(nullptr)
    {
    }

    void Run();

private:
    struct DTAndLoopInfo {
        std::unique_ptr<llvm::DominatorTree> dt = nullptr;
        std::unique_ptr<llvm::LoopInfoBase<llvm::BasicBlock, llvm::Loop>> loopInfo = nullptr;
        DTAndLoopInfo() = delete;
        DTAndLoopInfo(std::unique_ptr<llvm::DominatorTree> dt,
            std::unique_ptr<llvm::LoopInfoBase<llvm::BasicBlock, llvm::Loop>> loopInfo)
            : dt(std::move(dt)), loopInfo(std::move(loopInfo))
        {
        }
    };

    void MoveLoopInvariant4GetMethodOuterTI();

    llvm::Loop* GetMostInnerLoopFor(const llvm::Instruction& inst);

    CGModule& cgMod;
    const CGFunction& cgFunc;

    std::unique_ptr<DTAndLoopInfo> dtAndLoopInfo;
};
} // namespace Cangjie::CodeGen
#endif
