// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// pkgB is a macro package. It depends on pkgA via `macro import pkgA` (NOT a
// normal import). pkgB does NOT directly invoke pkgA's macro GenMacroA; instead
// its expansion result emits a call to @GenMacroA. This is exactly the macro
// import use case: pkgB needs pkgA's macro import declaration to propagate to
// downstream packages so the emitted @GenMacroA (and ArrayList it pulls in) is
// resolvable there.

macro package pkgB

import std.ast.*
macro import pkgA

public macro GenMacroB(input: Tokens): Tokens {
    // Expansion result references @GenMacroA (defined in pkgA). pkgB itself does
    // not call GenMacroA, so it needs no normal `import pkgA.*`.
    quote(@GenMacroA($input))
}
