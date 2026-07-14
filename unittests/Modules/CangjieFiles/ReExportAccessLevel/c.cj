// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package c

// 'c' is NOT a sub-package of 'b', so the 'internal' re-exported 'A' of 'b'
// must not be accessible from here.
import b.*
