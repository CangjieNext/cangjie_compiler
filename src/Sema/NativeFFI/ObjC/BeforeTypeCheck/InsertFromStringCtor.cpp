// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * TBD
 */

#include "Handlers.h"
#include "NativeFFI/ObjC/Utils/ASTQuery.h"
#include "cangjie/AST/Match.h"

using namespace Cangjie::AST;
using namespace Cangjie::Interop::ObjC;

void InsertFromStringCtor::HandleImpl(PrepareTypeCheckContext& ctx)
{
    for (auto& mirror : ctx.mirrors) {
        auto mirrorCd = As<ASTKind::CLASS_DECL>(mirror);
        if (!mirrorCd || !IsNSString(*mirrorCd)) {
            continue;
        }

        auto initFromStringDecl = ctx.astFactory.CreateFuncDecl();
        ctx.astTransformer.TransformToInitFromString(*initFromStringDecl);
        ctx.astInserter.InsertInto(*mirror, std::move(initFromStringDecl));
    }
}
