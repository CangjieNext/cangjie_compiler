// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// Refactor diagnostic id: ordinal from dot-def under src/Basic/diagnostics/DKRef/ (see DiagnosticOrdinal.cpp).

#ifndef CANGJIE_BASIC_DIAGID_H
#define CANGJIE_BASIC_DIAGID_H

#include <cstdint>

namespace Cangjie {
/**
 * Opaque diagnostic id for the refactor table: value is the ordinal index over the dot-def fragments
 * under \c src/Basic/diagnostics/DKRef/ (see DiagnosticOrdinal.cpp).
 */
class DiagId {
public:
    constexpr DiagId() : raw(0)
    {
    }

    constexpr explicit DiagId(int rawValue) : raw(rawValue)
    {
    }

    constexpr int Val() const
    {
        return raw;
    }

private:
    int raw;
};
} // namespace Cangjie

#endif // CANGJIE_BASIC_DIAGID_H
