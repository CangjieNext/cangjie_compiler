// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/AST/Nodes/Package.h"

#include "cangjie/AST/Nodes/ImportSpec.h"

namespace Cangjie::AST {

bool Package::IsEmpty() const
{
    for (auto& file : files) {
        if (file->package || !file->decls.empty()) {
            return false;
        }
        // Filter compiler added importSpec of core in stdlib.
        for (auto& import : file->imports) {
            if (!import->TestAttr(Attribute::COMPILER_ADD)) {
                return false;
            }
        }
    }
    return true;
}

} // namespace Cangjie::AST
