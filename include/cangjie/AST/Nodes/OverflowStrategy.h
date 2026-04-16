// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_OVERFLOWSTRATEGY_H
#define CANGJIE_AST_NODES_OVERFLOWSTRATEGY_H
#include <cstdint>

namespace Cangjie {
enum class OverflowStrategy : uint8_t { NA, CHECKED, WRAPPING, THROWING, SATURATING, OVERFLOW_STRATEGY_END };
}

#endif // CANGJIE_AST_NODES_OVERFLOWSTRATEGY_H