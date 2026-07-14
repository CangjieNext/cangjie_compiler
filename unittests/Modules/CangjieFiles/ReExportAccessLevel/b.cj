// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package b

// Re-export 'A' of package 'a' with 'internal' access level.
// Only 'b' and its sub-packages (e.g. 'b.c') can access 'A' via 'b'.
internal import a.A
