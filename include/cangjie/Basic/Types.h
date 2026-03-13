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
#ifndef CANGJIE_BASIC_TYPES_H
#define CANGJIE_BASIC_TYPES_H
#include <cstdint>

#ifdef __arm__
    using TargetUInt = uint32_t;
#else
    using TargetUInt = uint64_t;
#endif

#endif