// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

type MyInt64 = Int64
type MyFloat64 = Float64
type MyUInt32 = UInt32
type MyRune = Rune

func testAliasCast() {
    // Test 1: Alias to underlying type (Int64)
    let a: MyInt64 = MyInt64(42)
    let b: Int64 = MyInt64(100)
    
    // Test 2: Alias to Float64
    let c: MyFloat64 = MyFloat64(3.14)
    
    // Test 3: Alias to UInt32
    let d: MyUInt32 = MyUInt32(999)
    
    // Test 4: Alias to Rune
    let e: MyRune = MyRune(65)
}

main() {
    testAliasCast()
    return 0
}
