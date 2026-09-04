// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.

#include "cangjie/CHIR/Analysis/UnreachableBlockAnalysis.h"

#include "cangjie/CHIR/Utils/Visitor/Visitor.h"

namespace Cangjie::CHIR {

UnreachableBlockMap UnreachableBlockAnalysis::Parse(const Package& package)
{
    UnreachableBlockMap result;
    for (auto func : package.GetGlobalFuncsWithBody()) {
        Visitor::Visit(*func, [&result](Block& block) {
            if (!block.TestAttr(Attribute::UNREACHABLE_CONTINUATION)) {
                return VisitResult::CONTINUE;
            }
            auto rootLocation = block.Get<DebugLocationInfoForWarning>();
            result.emplace(&block, rootLocation);
            return VisitResult::CONTINUE;
        });
    }
    return result;
}

} // namespace Cangjie::CHIR
