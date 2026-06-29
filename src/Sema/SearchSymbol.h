// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares the SearchSymbol utility class, which wraps the symbol-search
 * queries against the ASTContext searcher used during type checking.
 */

#ifndef CANGJIE_SEMA_SEARCHSYMBOL_H
#define CANGJIE_SEMA_SEARCHSYMBOL_H

#include <vector>

#include "cangjie/AST/Searcher.h"
#include "cangjie/AST/Symbol.h"

namespace Cangjie {
class ASTContext;

/**
 * Stateless helper that issues symbol-table searches against the @p ASTContext searcher.
 *
 * These queries do not depend on any TypeChecker state, so they are grouped here rather
 * than on the TypeChecker implementation class.
 */
class SearchSymbol {
public:
    /**
     * Warmup the searcher cache for top-level scopes in parallel.
     * Only effective when the host has enough cores.
     */
    static void WarmupCache(const ASTContext& ctx);

    /** Get all decl symbols. */
    static std::vector<AST::Symbol*> GetAllDecls(const ASTContext& ctx);

    /** Get symbols of decl kinds that may carry generic type parameters. */
    static std::vector<AST::Symbol*> GetGenericCandidates(const ASTContext& ctx);

    /** Get symbols of struct-like decl kinds (class/interface/struct/enum/extend). */
    static std::vector<AST::Symbol*> GetAllStructDecls(const ASTContext& ctx);

    /** Get toplevel decl symbols (scope_level == 0). */
    static std::vector<AST::Symbol*> GetToplevelDecls(const ASTContext& ctx);

    /**
     * Get symbols of the given AST kind, ordered by @p order.
     * @param order defaults to position descending.
     */
    static std::vector<AST::Symbol*> GetSymsByASTKind(
        const ASTContext& ctx, AST::ASTKind astKind, const Order& order = Sort::posDesc);
};
} // namespace Cangjie

#endif // CANGJIE_SEMA_SEARCHSYMBOL_H
