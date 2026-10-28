// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package global_wildcard_lambda

var callback: CFunc<(Int64) -> Int64> = { a => a }
let _ = register({ a: Int64 => a + 1 })
func register(f: CFunc<(Int64) -> Int64>): Unit { callback = f }
