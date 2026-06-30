// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_SEMA_PARAM_TYPES_H
#define CANGJIE_SEMA_PARAM_TYPES_H

#include <optional>
#include <vector>

#include "cangjie/AST/Types.h"

namespace Cangjie::AST {
struct FuncDecl;
}

namespace Cangjie::TypeCheckUtil {

/** Function parameter list identity: explicit params plus optional receiver `this` mode. */
struct ParamTypes {
    std::vector<AST::ModalTy> paramTys;
    /** Set when the function has a receiver `this`; absent for static/top-level functions. */
    std::optional<ModalInfo> thisMode;

    static ParamTypes FromFuncTyOnly(const AST::FuncTy& ft)
    {
        return {ft.paramTys, std::nullopt};
    }
};

ParamTypes GetParamTypes(const AST::FuncDecl& fd);

} // namespace Cangjie::TypeCheckUtil

#endif
