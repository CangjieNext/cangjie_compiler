// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * `IfAvailableExpr` — semantic-only AST node (not produced by the parser / libast).
 */

#ifndef CANGJIE_AST_IFAVAILABLEEXPR_H
#define CANGJIE_AST_IFAVAILABLEEXPR_H

#include "cangjie/AST/Nodes/Annotation.h"
#include "cangjie/AST/Nodes/Expr.h"
#include "cangjie/AST/Nodes/LambdaExpr.h"
#include "cangjie/AST/Nodes/Modifier.h"

namespace Cangjie::AST {
/// @IfAvailable(name: arg, lambda1, lambda2) after macro expansion (before it is a MacroExpandExpr).
struct IfAvailableExpr : public Expr {
    IfAvailableExpr(OwnedPtr<FuncArg> namedArg, OwnedPtr<LambdaExpr> lambdaArg1, OwnedPtr<LambdaExpr> lambdaArg2);
    ~IfAvailableExpr() override;

    Ptr<FuncArg> GetArg() const
    {
        return arg.get();
    }
    Ptr<LambdaExpr> GetLambda1() const
    {
        return lambda1.get();
    }
    Ptr<LambdaExpr> GetLambda2() const
    {
        return lambda2.get();
    }

private:
    OwnedPtr<FuncArg> arg;
    OwnedPtr<LambdaExpr> lambda1;
    OwnedPtr<LambdaExpr> lambda2;
};

} // namespace Cangjie::AST
#endif // CANGJIE_AST_IFAVAILABLEEXPR_H
