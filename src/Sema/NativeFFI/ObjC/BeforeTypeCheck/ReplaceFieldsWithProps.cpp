// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements replacing the fields of @ObjCMirror classes with properties, as a mirror keeps no
 * state on the Cangjie side and every member access has to be routed to the Objective-C runtime.
 */

#include "Handlers.h"
#include "cangjie/AST/Match.h"
#include "cangjie/AST/Utils.h"

using namespace Cangjie::AST;
using namespace Cangjie::Interop::ObjC;

void ReplaceFieldsWithProps::HandleImpl(PrepareTypeCheckContext& ctx)
{
    for (auto& mirror : ctx.mirrors) {
        if (auto mirrorCd = As<ASTKind::CLASS_DECL>(mirror)) {
            InsertMirrorVarProp(*mirrorCd, Attribute::OBJ_C_MIRROR);
        }
    }
}
