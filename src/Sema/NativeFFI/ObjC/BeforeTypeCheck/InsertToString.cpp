// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements inserting a `toString(): String` declaration into the NSObject @ObjCMirror.
 * Its body is generated later, at the AfterTypeCheck stage, see InsertStringConversions.
 */

#include "Handlers.h"
#include "NativeFFI/ObjC/Utils/ASTQuery.h"
#include "NativeFFI/Utils.h"
#include "cangjie/AST/Create.h"
#include "cangjie/AST/Match.h"

using namespace Cangjie::AST;
using namespace Cangjie::Interop::ObjC;
using namespace Cangjie::Native::FFI;

void InsertToString::HandleImpl(PrepareTypeCheckContext& ctx)
{
    for (auto& mirror : ctx.mirrors) {
        auto mirrorCd = As<ASTKind::CLASS_DECL>(mirror);
        if (!mirrorCd || !IsNSObject(*mirrorCd)) {
            continue;
        }

        auto paramLists = Nodes<FuncParamList>(CreateFuncParamList(std::vector<OwnedPtr<FuncParam>>{}));
        // Transform function will set the identifier
        auto toStringDecl = CreateFuncDecl("", CreateFuncBody(std::move(paramLists), nullptr, CreateBlock({})));
        ctx.astTransformer.TransformToToString(*toStringDecl);
        ctx.astInserter.InsertInto(*mirror, std::move(toStringDecl));
    }
}
