// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.
//
// End-to-end exercise of the tuple-destructuring `as` cast introduced for
// issue I9F15H. Each element of the left-hand tuple is cast independently
// to the corresponding target type, producing a tuple of Options.

open class Animal {
    public func name(): String { return "animal" }
}
class Dog <: Animal {
    public func bark(): String { return "woof" }
}

main(): Int64 {
    let tup: (Animal, Animal) = (Dog(), Dog())
    // Tuple-destructuring cast: `e as (Dog, Animal)`. The resulting type is
    // (Option<Dog>, Option<Animal>). Both elements are cast independently.
    let (d, a) = tup as (Dog, Animal)
    let _ = match (d) {
        case Some(v) => v.bark()
        case None => "no dog"
    }
    let _ = match (a) {
        case Some(v) => v.name()
        case None => "no animal"
    }
    return 0
}