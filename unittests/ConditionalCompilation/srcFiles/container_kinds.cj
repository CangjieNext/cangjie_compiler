// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// Exercises the Walker dispatch for the various container kinds.
// `os == "Linux"` evaluates true on the test host, so every guarded node
// is kept and all child containers are walked.

@When[!test]
class MyClass {
    @When[!test]
    func memberFunc(): Unit {}
}

@When[!test]
struct MyStruct {
    @When[!test]
    var x: Int64
}

@When[!test]
interface MyInterface {
    @When[!test]
    func ifaceMethod(): Unit
}

@When[!test]
enum MyEnum {
    | A
}

@When[!test]
extend MyClass {
    @When[!test]
    func extended(): Unit {}
}

@When[!test]
class WithProp {
    @When[!test]
    prop myProp: Int64 {
        get() { return 0 }
        set(value) { value }
    }
}

@When[!test]
func withParams(@When[!test] a: Int64): Unit {}

func funcWithWhenBody(): Unit {
    let i = 0
    let j = 1
    let sum = i + j
}

func plainFunc(): Unit {}

main() {}