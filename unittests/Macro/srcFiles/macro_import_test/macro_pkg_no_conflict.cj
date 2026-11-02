// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// Test case: no conflict diagnostic is ever emitted at declaration time.
// Wildcards and duplicate declarations need no dedicated check (dedup happens
// at .macrocall injection; use-site ambiguity is reported lazily):
// - macro import std.reflect.* + macro import std.collection.*  (wildcards)
// - macro import pkgX.f repeated across the package (same import string)

macro package macro_import_test_pkg_no_conflict

macro import std.reflect.*
macro import std.collection.*
macro import pkgX.f
macro import pkgX.f

public macro Dummy(input: Tokens): Tokens {
    input
}
