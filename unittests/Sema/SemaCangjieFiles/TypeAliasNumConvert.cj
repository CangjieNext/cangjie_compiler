// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.
// Issue #375: type alias should be usable for numeric type conversion

// === Type aliases for all numeric/rune types ===
type word = UInt16
type myInt8 = Int8
type myUInt32 = UInt32
type myInt64 = Int64
type myFloat32 = Float32
type myFloat64 = Float64
type myRune = Rune
type myUInt8 = UInt8

// Chained type alias
type myWord = word

main(): Int64 {
    // === Basic numeric type alias conversion ===
    var a = word(20)
    if (typeid(a) != typeid(UInt16)) { return 1 }

    var b = myInt8(-8)
    if (typeid(b) != typeid(Int8)) { return 2 }

    var c = myUInt32(100000)
    if (typeid(c) != typeid(UInt32)) { return 3 }

    var d = myInt64(-999999)
    if (typeid(d) != typeid(Int64)) { return 4 }

    var e = myFloat32(3.14)
    if (typeid(e) != typeid(Float32)) { return 5 }

    var f = myFloat64(2.71828)
    if (typeid(f) != typeid(Float64)) { return 6 }

    var g = myRune(65)
    if (typeid(g) != typeid(Rune)) { return 7 }

    // === Edge cases: zero, min, max, negative ===
    var h = word(0)
    if (typeid(h) != typeid(UInt16)) { return 8 }

    var i = myUInt8(255)
    if (typeid(i) != typeid(UInt8)) { return 9 }

    var j = myInt64(-9223372036854775807)
    if (typeid(j) != typeid(Int64)) { return 10 }

    // === Chained type alias: myWord → word → UInt16 ===
    var k = myWord(42)
    if (typeid(k) != typeid(UInt16)) { return 11 }

    // === Usage in expressions ===
    var l = word(10) + word(20)
    if (l != 30) { return 12 }

    var m = myInt64(100) * myInt64(2)
    if (m != 200) { return 13 }

    var n = myFloat32(5.5) / myFloat32(2.0)
    if (n < 2.5 || n > 3.0) { return 14 }

    // === Type alias conversion with different source expression types ===
    var o = word(1 + 2)
    if (o != 3) { return 15 }

    var p = myInt64(if (true) { 10 } else { 20 })
    if (p != 10) { return 16 }

    return 0
}
