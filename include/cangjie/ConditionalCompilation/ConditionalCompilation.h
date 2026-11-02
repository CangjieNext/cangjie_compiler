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

    /// Evaluate the @When annotation(s) on a structured MacroImportSpec in the current
    /// target's context. Condition semantics are identical to ordinary imports
    /// (evaluated against target options, never a scope lookup) — EvalNodeCondition is
    /// shared, change one, affect both. False conditions are erased from
    /// node->annotations. Returns true when the import survives (no @When, or condition
    /// holds).
    bool EvalMacroImportWhenCond(MacroImportSpec& spec) const;

    /// Non-destructive twin of \ref EvalMacroImportWhenCond: evaluates the @When
    /// annotation(s) without erasing them. Use for cjo-backed MacroImportSpec nodes
    /// owned by the CjoManager cache — they are shared across consumer packages, so
    /// an erasing evaluation would corrupt the condition for subsequent consumers.
    /// Condition semantics are identical to \ref EvalMacroImportWhenCond.
    bool PeekMacroImportWhenCond(const MacroImportSpec& spec) const;

private:
    class ConditionalCompilationImpl* impl;
};
} // namespace AST
} // namespace Cangjie

#endif
