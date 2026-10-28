// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.
//
// Reproduction for Issue #1239: let members cannot be reassigned in init.
//
// This pattern is required for iterative segment tree implementations
// (e.g. segmenttree4cj's IterativeSegmentTree<T>) where 'let size' and 'let log'
// must be computed via a while-loop in init (size = 2^ceil(log2(n))).
//
// Current behavior (cjc 1.1.3): compile error
//   error: cannot assign to immutable member 'size' in initializer
//
// Expected: should compile, since 'let size' is assigned exactly once during init.

package pkg

// Minimal reproduction: compute size=2^ceil(log2(n)) via while-loop in init
// This is the core pattern in iterative segment tree (AtCoder Library style)
class IterativeSegmentTreeLike {
    let n: Int64
    let size: Int64   // next power of two >= n

    init(n: Int64) {
        this.n = n
        // Issue #1239: this while-loop reassignment to 'let size' is rejected
        var s: Int64 = 1
        while (s < n) {
            s = s * 2
        }
        size = s   // ERROR: cannot assign to immutable member in initializer
    }
}

// Same pattern with 'let log' member
class IterativeLazySegmentTreeLike {
    let n: Int64
    let size: Int64
    let log: Int64

    init(n: Int64) {
        this.n = n
        var s: Int64 = 1
        var l: Int64 = 0
        while (s < n) {
            s = s * 2
            l = l + 1
        }
        size = s   // ERROR
        log = l    // ERROR
    }
}
