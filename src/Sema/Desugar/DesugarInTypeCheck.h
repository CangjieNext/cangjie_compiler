// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares private functions of desugar.
 */
#ifndef CANGJIE_SEMA_DESUGAR_IN_TYPECHECK_H
#define CANGJIE_SEMA_DESUGAR_IN_TYPECHECK_H

#include "cangjie/AST/ASTContext.h"

namespace Cangjie {
namespace AST {
struct AssignExpr;
struct BinaryExpr;
struct CallExpr;
struct PrimaryCtorDecl;
struct SubscriptExpr;
struct UnaryExpr;
} // namespace AST

void DesugarOperatorOverloadExpr(ASTContext& ctx, AST::BinaryExpr& be);
void DesugarOperatorOverloadExpr(ASTContext& ctx, AST::UnaryExpr& ue);
void DesugarOperatorOverloadExpr(ASTContext& ctx, AST::SubscriptExpr& se);
void DesugarOperatorOverloadExpr(ASTContext& ctx, AST::AssignExpr& ae);
void DesugarSubscriptOverloadExpr(ASTContext& ctx, AST::AssignExpr& ae);

void DesugarFlowExpr(ASTContext& ctx, AST::BinaryExpr& fe);
void DesugarCallExpr(ASTContext& ctx, AST::CallExpr& ce);

/**
 * Desugar variable-length arguments to array literal. For example:
 * *************** before desugar ****************
 * f(true, 1, 2, 3)
 * *************** after desugar ****************
 * f(true, [1, 2, 3]) // Suppose the `fixedPositionalArity` is 1.
 * */
void DesugarVariadicCallExpr(ASTContext& ctx, AST::CallExpr& ce, size_t fixedPositionalArity);

/** Add primary constructor for class or struct. */
void DesugarPrimaryCtor(AST::Decl& decl, AST::PrimaryCtorDecl& fd);
} // namespace Cangjie

#endif
