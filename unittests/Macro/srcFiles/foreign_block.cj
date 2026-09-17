// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

import define.*

foreign {
    @M(func first(): Unit
        func second(): Int32)
    @M func third(): Unit
    @M @M func fourth(): Unit
}

@M(func ordinary() {})
