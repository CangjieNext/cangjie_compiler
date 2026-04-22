// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_REFEXPR_H
#define CANGJIE_AST_NODES_REFEXPR_H

#include "cangjie/AST/Nodes/NameReferenceExpr.h"
#include "cangjie/AST/Nodes/ModalASTInfo.h"
#include "cangjie/AST/Nodes/Reference.h"
#include "cangjie/AST/Types.h"

namespace Cangjie::AST {
/**
 * RefExpr represents an expression of reference.
 */
struct RefExpr : NameReferenceExpr {
    Reference ref; /**< Reference of the expr. */
    ModalASTInfo modal; /**< Modal type info for constructor call or type conversion. */
    bool isThis{false};
    bool isSuper{false};
    bool isQuoteDollar{false};
    RefExpr() : NameReferenceExpr(ASTKind::REF_EXPR)
    {
    }
    void Clear() noexcept override
    {
        Expr::Clear();
        ref.SetTarget(nullptr);
        ref.SetTargets({});
        instTys.clear();
        matchedParentTy = nullptr;
    }
    Ptr<Decl> GetTarget() const override;
    void SetTarget(Ptr<Decl> newTarget) override;
    std::vector<Ptr<Decl>> GetTargets() const override;
    void SetTargets(std::vector<Ptr<Decl>> decls) override;
    void AddTarget(Ptr<Decl> decl) override;

    std::string ToString() const override;
    /**
     * Get the identifier Position of the RefExpr.
     * @return identifier Position in macrocall file if the RefExpr is expanded from macrocall,
     *  identifier position in curfile otherwise.
     */
    Position GetIdentifierPos() const;
};
} // namespace Cangjie::AST

#endif // CANGJIE_AST_NODES_REFEXPR_H
