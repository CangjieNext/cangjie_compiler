// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "TypeCheckerImpl.h"

#include "Diags.h"
#include "TypeCheckUtil.h"

using namespace Cangjie;
using namespace Sema;
using namespace TypeCheckUtil;

bool TypeChecker::TypeCheckerImpl::ChkTupleLit(ASTContext& ctx, ModalTy target, TupleLit& tl)
{
    if (target->IsAny()) {
        tl.SetTy(Synthesize({ctx, SynPos::EXPR_ARG}, &tl));
        ReplaceIdealTy(tl);
        return tl.GetTy().IsCorrect();
    }
    ModalTy targetTy = UnboxOptionType(target);
    if (!Ty::IsTyCorrect(targetTy) || !targetTy->IsTuple()) {
        DiagMismatchedTypesWithFoundTy(diag, tl, targetTy->String(), "Tuple");
        tl.SetTy(TypeManager::GetNonNullTy(tl.GetTy()));
        return false;
    }
    auto tupleTy = StaticCast<TupleTy*>(targetTy.Ty());
    auto typeArgs = tupleTy->typeArgs;
    if (typeArgs.size() != tl.children.size()) {
        tl.SetTy(Synthesize({ctx, SynPos::EXPR_ARG}, &tl));
        ReplaceIdealTy(tl);
        DiagMismatchedTypes(diag, tl, targetTy);
        return false;
    }
    // If the size of target elemTys and elements are equal, check one by one.
    std::vector<ModalTy> realElemTys;
    for (size_t i = 0; i < typeArgs.size(); ++i) {
        CJC_NULLPTR_CHECK(tl.children[i]);
        // apply modal to each element type
        auto itarget = typeArgs[i].With(target.Mode());
        if (!Check(ctx, itarget, tl.children[i].get())) {
            if (Ty::IsTyCorrect(itarget) && tl.children[i]->GetTy().IsCorrect()) {
                DiagMismatchedTypes(diag, *tl.children[i], itarget);
            }
            tl.SetTy(Synthesize({ctx, SynPos::EXPR_ARG}, &tl));
            ReplaceIdealTy(tl);
            return false;
        } else {
            realElemTys.push_back(tl.children[i]->GetTy());
        }
    }
    // Should use SetTy(), but have bugs on ideal type, use Join() instead at current stage.
    // TupleLit allow elements been boxed by given target type.
    // Eg. Option<Int64>*Option<Int64> <=> (1,1) or I1*I1 <=> (1,1) where Int64 extends I1 allow box.
    tl.SetTy(targetTy);
    return true;
}

ModalTy TypeChecker::TypeCheckerImpl::SynTupleLit(ASTContext& ctx, TupleLit& tl)
{
    std::vector<Ptr<Ty>> elemTy;
    ModalInfo modal{static_cast<LocalModal>(0)};
    // Synthesize the type of each element.
    for (auto& it : tl.children) {
        if (!it) {
            tl.SetTy({TypeManager::GetInvalidTy()});
            return tl.GetTy();
        }
        if (!it->GetTy().IsCorrect()) {
            Synthesize({ctx, SynPos::EXPR_ARG}, it.get());
        }
        ReplaceIdealTy(*it);
        elemTy.push_back(it->DataTy());
        modal = modal | it->TyMode();
    }
    if (modal == ModalInfo{static_cast<LocalModal>(0)}) {
        modal = {};
    }
    tl.SetTy({typeManager.GetTupleTy(elemTy), modal});
    return tl.GetTy();
}
