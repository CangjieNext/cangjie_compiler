// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares the ConditionalCompilation related classes, which provides conditional compilation capabilities.
 */
#ifndef CANGJIE_CONDITIONAL_COMPILATION_H
#define CANGJIE_CONDITIONAL_COMPILATION_H

#include "cangjie/AST/Node.h"
#include "cangjie/Frontend/CompilerInstance.h"

namespace Cangjie {
namespace AST {

class ConditionalCompilation {
public:
    friend class CompilerInstance;
    explicit ConditionalCompilation(CompilerInstance* ci);
    ~ConditionalCompilation();

    /// entrance of conditional compilation stage
    void HandleConditionalCompilation(const Package& root) const;
    /// file entrance of conditional compilation stage. Used by \ref HandleConditionalCompilation and macro expansion
    void HandleFileConditionalCompilation(File& file) const;

    /// Evaluate the @When annotation(s) on a macro-imported ImportSpec in the current
    /// downstream target's context, via the normal-import @When path (semantic rule 2:
    /// macro import is equivalent to a downstream ordinary import including conditional
    /// compilation). Reuses EvalNodeCondition<ImportSpec> so the condition semantics are
    /// identical to ordinary imports — no text-ify/reparse detour. @When annotations
    /// whose condition is false are erased from the node's annotations, mirroring how
    /// HandleFileConditionalCompilation filters ordinary imports.
    /// Returns true when the import survives (no @When, or condition holds).
    bool EvalImportWhenCond(ImportSpec& import) const;

    /// Evaluate the @When annotation(s) on a MacroImportSpec in the current downstream
    /// target's context. Same semantics as EvalImportWhenCond but for the structured
    /// macro-import node (replaces the former token-stream re-parse path). Reuses
    /// EvalNodeCondition<MacroImportSpec> — the template only depends on
    /// node->annotations, so the condition semantics are identical to ordinary imports.
    /// Returns true when the import survives (no @When, or condition holds).
    bool EvalMacroImportWhenCond(MacroImportSpec& spec) const;

private:
    class ConditionalCompilationImpl* impl;
};
} // namespace AST
} // namespace Cangjie

#endif
