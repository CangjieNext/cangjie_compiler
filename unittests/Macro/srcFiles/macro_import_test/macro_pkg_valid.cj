// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// Test case: valid macro import in a macro package.
// Rule 1: macroImportList := 'macro' 'import' importContent (no importModifier).
// Rule 2.1: macro import only in macro package.
// Rule 2.2: macro import can mix with normal import.

macro package macro_import_test_pkg

import std.ast.*
macro import std.reflect.*
macro import std.collection.{ArrayList, HashMap}
macro import std.collection.Vector

public macro Dummy(input: Tokens): Tokens {
    input
}
