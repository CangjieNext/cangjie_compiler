// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package global_wildcard_lambda

var provider: () -> String = { => "" }
let _ = registerCJ({ => "provider" })
func registerCJ(f: () -> String): Unit { provider = f }

main(): Int64 {
    if (unsafe { callback(41) } != 42 || provider() != "provider") {
        return 1
    }
    return 0
}
