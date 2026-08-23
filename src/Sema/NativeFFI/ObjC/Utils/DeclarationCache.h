// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares a factory class for creating AST nodes.
 */

#ifndef CANGJIE_SEMA_OBJ_C_UTILS_DECLARATION_CACHE_H
#define CANGJIE_SEMA_OBJ_C_UTILS_DECLARATION_CACHE_H

#include <unordered_map>
#include <string>

#include "cangjie/AST/Node.h"
#include "cangjie/Utils/SafePointer.h"

namespace Cangjie::Interop::ObjC {

struct DeclarationCache {
    std::unordered_map<std::string, OwnedPtr<AST::VarDecl>> cachedSelectorDecls;
    std::unordered_map<std::string, OwnedPtr<AST::VarDecl>> cachedClassDecls;

    void Clear() noexcept
    {
        cachedSelectorDecls.clear();
        cachedClassDecls.clear();
    }
};

} // namespace Cangjie::Interop::ObjC

#endif // CANGJIE_SEMA_OBJ_C_UTILS_DECLARATION_CACHE_H