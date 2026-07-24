// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_CHIR_IR_UNREACHABLE_BLOCK_INFO_H
#define CANGJIE_CHIR_IR_UNREACHABLE_BLOCK_INFO_H

#include <unordered_map>

namespace Cangjie::CHIR {
class Block;
class Terminator;

struct UnreachableBlockInfo {
    UnreachableBlockInfo(Terminator* terminator, Block* sourceRoot = nullptr)
        : terminator(terminator), sourceRoot(sourceRoot) {}
    UnreachableBlockInfo(Terminator* terminator, bool startsNewRoot, Block* sourceRoot = nullptr,
        bool isPendingCallRoot = false)
        : terminator(terminator), startsNewRoot(startsNewRoot), sourceRoot(sourceRoot),
          isPendingCallRoot(isPendingCallRoot) {}

    Terminator* terminator;
    bool startsNewRoot{false};
    Block* sourceRoot{nullptr};
    // A final Nothing argument retains its root until a following source expression supersedes the call warning.
    bool isPendingCallRoot{false};
};

using UnreachableBlockMap = std::unordered_map<Block*, UnreachableBlockInfo>;
} // namespace Cangjie::CHIR

#endif
