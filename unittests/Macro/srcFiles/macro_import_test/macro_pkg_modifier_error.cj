// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// Test case (NEGATIVE): macro import MUST NOT carry an importModifier (no
// re-export). 'public macro import ...' is rejected by the parser with the
// dedicated diagnostic macro_import_no_modifier_allowed (three-token lookahead
// 'modifier macro import' in ParseImports.cpp).

macro package macro_import_test_pkg_err

public macro import std.reflect.*

public macro Dummy(input: Tokens): Tokens {
    input
}
