// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// Test case (NEGATIVE): macro import MUST appear in a macro package only.
// A bare 'macro import' / 'macro {' in a normal package is rejected by the
// parser with the dedicated diagnostic macro_import_in_non_macro_package
// (two-token lookahead in ParseImports.cpp).

package non_macro_pkg

macro import std.reflect.*

main(): Int64 {
    return 0
}
