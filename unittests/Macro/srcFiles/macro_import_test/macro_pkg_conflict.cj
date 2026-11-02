// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// Test case: same-name imports from different packages do NOT conflict at
// declaration time. Symbol conflicts are NOT diagnosed per macro import rule:
// injected symbols enter the same fileImportedDeclsMap as normal private
// imports, and ambiguity is reported lazily at the use site (UT-M-08 input).
// The macro package below must compile without any conflict diagnostic.

macro package macro_import_test_pkg_conflict

macro import pkgX.someFunc
macro import pkgY.someFunc

public macro Dummy(input: Tokens): Tokens {
    input
}
