// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares some types.
 */

#ifdef CANGJIE_TARGET_TRIPLE
    const std::string targetTriple = CANGJIE_TARGET_TRIPLE;
    if (targetTriple == "arm-linux-android23") {
        using TargetUInt = uint32_t;
    } else {
        using TargetUInt = uint64_t;
    }
#else
    using TargetUInt = uint64_t;
#endif
