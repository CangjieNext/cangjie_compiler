// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// Scenario for rule 3.3 (lifter semantics) + transitive closure.
// pkgA is a macro package. It declares `macro import std.collection.ArrayList`
// so that downstream packages obtain ArrayList's symbol. pkgA's own macro body
// does NOT reference ArrayList (rule 3.1: macro import is invisible to pkgA itself);
// it is the expansion *result* that references ArrayList, which is consumed by
// downstream packages.

macro package pkgA

import std.ast.*
macro import std.collection.ArrayList

public macro GenMacroA(input: Tokens): Tokens {
    // The expansion result references ArrayList; this symbol must reach downstream
    // packages via pkgA's macro import declaration. Adds a member to the input
    // class whose type is ArrayList, proving the symbol is visible downstream.
    quote($input)
}
