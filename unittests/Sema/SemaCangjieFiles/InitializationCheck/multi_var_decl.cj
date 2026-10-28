// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.
//
// Reproduction for Issue #1240: Support comma-separated multi-variable declarations.
//
// This pattern is common in competitive programming data structures (segment trees,
// Fenwick trees, HLD) where multiple loop variables need simultaneous initialization.
// AtCoder Library (C++) and Rust both support this syntax natively.

package pkg

class RangeUpdateLike {
    let n: Int64
    let size: Int64
    let log: Int64
    var d: Array<Int64>

    init(n: Int64) {
        this.n = n
        // Issue #1240: multi-variable declaration on one line is not supported
        // The line below should be valid but is rejected with syntax error:
        //   var l2 = left, r2 = right
        // Workaround: split into two lines (adds clutter in hot-path code)
        var l2: Int64 = 0
        var r2: Int64 = 0
        
        // Iterative segment tree hot path uses this pattern:
        //   var l2 = left, r2 = right  // currently illegal
        size = 1; log = 0
        while (size < n) { size = size * 2; log = log + 1 }
        d = Array<Int64>(2 * size, { _ => 0 })
    }

    func rangeUpdate(left: Int64, right: Int64, value: Int64) {
        // Typical segment tree update pattern where single-line multi-decl
        // would cleanly express the dual-pointer merge
        var l: Int64 = left + size
        var r: Int64 = right + size
        // Desired: var l = left + size, r = right + size
    }

    func query(left: Int64, right: Int64): Int64 {
        // Typical segment tree query pattern
        var l: Int64 = left + size
        var r: Int64 = right + size
        // Desired: var l = left + size, r = right + size
        return 0
    }
}

// Another common pattern in iterative binary search
class FindFirstNonZeroLike {
    let d: Array<Int64>
    let size: Int64

    init(d: Array<Int64>, size: Int64) {
        this.d = d
        this.size = size
    }

    func find(): Int64 {
        var p: Int64 = 1
        // Desired: while (p < size) { var left = d[2*p], right = d[2*p+1]; ... }
        // Without multi-decl, competitive segment trees become verbose
        while (p < size) {
            var left = d[2 * p]
            var right = d[2 * p + 1]
            p = if (left > 0) { 2 * p } else { 2 * p + 1 }
        }
        return p - size
    }
}
