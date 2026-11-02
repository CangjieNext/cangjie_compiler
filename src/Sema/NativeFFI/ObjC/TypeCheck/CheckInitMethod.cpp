// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements checks of @ObjCInit annotated method.
 */

#include "Diags.h"
#include "Handlers.h"
#include "NativeFFI/ObjC/Utils/ASTQuery.h"
#include "cangjie/AST/Match.h"

using namespace Cangjie::AST;
using namespace Cangjie::Interop::ObjC;

void CheckInitMethod::HandleImpl(TypeCheckContext& ctx)
{
    if (!ctx.target.IsObjCForeign()) {
        return;
    }

    for (auto member : ctx.target.GetMemberDeclPtrs()) {
        auto method = As<ASTKind::FUNC_DECL>(member);
        if (!method || !IsObjCInitMethod(*method)) {
            continue;
        }
<<<<<<< HEAD
        auto fTy = DynamicCast<FuncTy*>(method->GetTy());
        if (!fTy || !method->funcBody->retType) {
||||||| constructed merge base
        auto& method = *StaticAs<ASTKind::FUNC_DECL>(member);
        auto fTy = DynamicCast<FuncTy*>(method.GetTy());
        if (!fTy || !method.funcBody->retType) {
=======
        auto& method = *StaticAs<ASTKind::FUNC_DECL>(member);
        auto fTy = DynamicCast<FuncTy*>(method.DataTy());
        if (!fTy || !method.funcBody->retType) {
>>>>>>> feat: local mode
            continue;
        }
        auto retTy = fTy->retTy;
        auto mirrorTy = method->outerDecl->GetTy();
        if (ctx.typeManager.IsTyEqual(retTy, mirrorTy)) {
            continue;
        }

<<<<<<< HEAD
        ctx.diag.DiagnoseRefactor(DiagKindRefactor::sema_mismatched_types, *method->funcBody->retType)
            .AddMainHintArguments(Ty::ToString(mirrorTy), Ty::ToString(retTy));
||||||| constructed merge base
        ctx.diag.DiagnoseRefactor(DiagKindRefactor::sema_mismatched_types, *method.funcBody->retType)
            .AddMainHintArguments(Ty::ToString(mirrorTy), Ty::ToString(retTy));
=======
        Sema::DiagSemaMismatchedTypes(
            ctx.diag, *method.funcBody->retType, Ty::ToString(mirrorTy.Ty()), Ty::ToString(retTy.Ty()));
>>>>>>> feat: local mode
        ctx.target.EnableAttr(Attribute::IS_BROKEN);
    }
}
