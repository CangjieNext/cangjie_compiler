// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// Exercises the Walker dispatch for the various container kinds.
// `os == "Linux"` evaluates true on the test host, so every guarded node
// is kept and all child containers are walked.

@When[os == "Linux"]
class MyClass {
    @When[os == "Linux"]
    func memberFunc(): Unit {}
}

@When[os == "Linux"]
struct MyStruct {
    @When[os == "Linux"]
    var x: Int64
}

@When[os == "Linux"]
interface MyInterface {
    @When[os == "Linux"]
    func ifaceMethod(): Unit
}

@When[os == "Linux"]
enum MyEnum {
    | A
}

@When[os == "Linux"]
extend MyClass {
    @When[os == "Linux"]
    func extended(): Unit {}
}

@When[os == "Linux"]
class WithProp {
    @When[os == "Linux"]
    prop myProp: Int64 {
        get() { return 0 }
        set(value) { value }
    }
}

@When[os == "Linux"]
func withParams(@When[os == "Linux"] a: Int64): Unit {}

func funcWithWhenBody(): Unit {
    let i = 0
    let j = 1
    let sum = i + j
}

func plainFunc(): Unit {}

main() {}