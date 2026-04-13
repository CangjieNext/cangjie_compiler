// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// Legacy diagnostic id: ordinal from dot-def under src/Basic/diagnostics/DK/ (see DiagnosticOrdinal.cpp).

#ifndef CANGJIE_BASIC_DIAGIDOLD_H
#define CANGJIE_BASIC_DIAGIDOLD_H

namespace Cangjie {
/**
 * Legacy ordinal over the dot-def fragments under \c src/Basic/diagnostics/DK/ (see DiagnosticOrdinal.cpp);
 * refactor ids use \ref DiagId.
 */
class DiagIdOld {
public:
    constexpr explicit DiagIdOld(int ordinal) : raw(ordinal)
    {
    }

    constexpr int Val() const
    {
        return raw;
    }

    constexpr bool operator==(DiagIdOld o) const
    {
        return raw == o.raw;
    }

    constexpr bool operator!=(DiagIdOld o) const
    {
        return raw != o.raw;
    }

private:
    int raw;
};
} // namespace Cangjie

#endif // CANGJIE_BASIC_DIAGIDOLD_H
