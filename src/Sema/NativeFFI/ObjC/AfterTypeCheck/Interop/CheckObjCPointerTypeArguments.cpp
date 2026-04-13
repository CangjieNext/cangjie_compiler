// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements checks of types used with ObjCPointer
 */

#include "Handlers.h"
#include "NativeFFI/ObjC/Utils/Common.h"
#include "NativeFFI/Utils.h"
#include "cangjie/AST/Walker.h"

using namespace Cangjie::AST;
using namespace Cangjie::Interop::ObjC;

void CheckObjCPointerTypeArguments::HandleImpl(InteropContext& ctx)
{
    for (auto& file : ctx.pkg.files) {
        Walker(file, Walker::GetNextWalkerID(), [&file, &ctx](auto node) {
            if (!node->IsSamePackage(*file->curPackage)) {
                return VisitAction::WALK_CHILDREN;
            }
            Ptr<Type> typeUsage = As<ASTKind::TYPE>(node);
            if (typeUsage && typeUsage->GetTypeArgs().size() == 1 &&
                ctx.typeMapper.IsObjCPointer(*typeUsage->GetTy()) &&
                !ctx.typeMapper.IsObjCCompatible(*typeUsage->GetTy()->typeArgs[0])) {
                ctx.diag.DiagnoseRefactor("sema_objc_pointer_argument_must_be_objc_compatible", *typeUsage);
                typeUsage->EnableAttr(Attribute::IS_BROKEN);
            }
            return VisitAction::WALK_CHILDREN;
        }).Walk();
    }
}
