// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.
// Issue #375: type alias should be usable for numeric type conversion

type word = UInt16
type myInt = Int64
type myFloat = Float32
type myRune = Rune

main(): Int64 {
    var a = word(20)
    println(a)
    var b = myInt(-42)
    println(b)
    var c = myFloat(3.14)
    println(c)
    var d = myRune(65)
    println(d)
    // Verify type is correct
    if (typeid(a) != typeid(UInt16)) {
        return 1
    }
    if (typeid(b) != typeid(Int64)) {
        return 2
    }
    return 0
}