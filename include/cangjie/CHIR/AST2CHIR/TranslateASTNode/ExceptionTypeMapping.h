// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares normal type and exception type maping struct template.
 */

#ifndef CANGJIE_CHIR_EXCEPTION_TYPEMAPPING_H
#define CANGJIE_CHIR_EXCEPTION_TYPEMAPPING_H

#include "cangjie/CHIR/IR/Expression/Terminator.h"

namespace Cangjie::CHIR {
template <typename T> struct CHIRNodeMap {
};

// Defined CHIR type mapping register macro.
#define DEFINE_CHIR_TYPE_MAPPING(TYPE)                                                                                 \
    template <> struct CHIRNodeMap<TYPE> {                                                                             \
        using Normal = TYPE;                                                                                           \
        using Exception = Try##TYPE;                                                                                   \
    }

DEFINE_CHIR_TYPE_MAPPING(Apply);
DEFINE_CHIR_TYPE_MAPPING(Invoke);
DEFINE_CHIR_TYPE_MAPPING(InvokeStatic);
DEFINE_CHIR_TYPE_MAPPING(NumericCast);
DEFINE_CHIR_TYPE_MAPPING(Allocate);
DEFINE_CHIR_TYPE_MAPPING(Spawn);
DEFINE_CHIR_TYPE_MAPPING(Intrinsic);
DEFINE_CHIR_TYPE_MAPPING(RawArrayAllocate);
DEFINE_CHIR_TYPE_MAPPING(UnaryExpression);
DEFINE_CHIR_TYPE_MAPPING(BinaryExpression);

template <typename T> using CHIRNodeNormalT = typename CHIRNodeMap<T>::Normal;
template <typename T> using CHIRNodeExceptionT = typename CHIRNodeMap<T>::Exception;
} // namespace Cangjie::CHIR
#endif
