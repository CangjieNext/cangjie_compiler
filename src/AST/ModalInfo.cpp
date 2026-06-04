// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/AST/ModalInfo.h"

#include "cangjie/Utils/Utils.h"

namespace Cangjie {

std::string_view ToString(LocalModal modal)
{
    switch (modal) {
        case LocalModal::NOT:
            return "";
        case LocalModal::HALF:
            return "@local?";
        case LocalModal::FULL:
            return "@local!";
        default:
            CJC_ABORT();
            return "";
    }
}

static_assert(sizeof(ModalInfo) <= 2 * sizeof(void*), "pass ModalInfo by reference instead of by value");

std::string ModalInfo::ToString() const
{
    return std::string{Cangjie::ToString(local)};
}

std::string ModalInfo::LocalString() const
{
    switch (local) {
        case LocalModal::NOT:
            return "@~local";
        case LocalModal::HALF:
            return "@local?";
        case LocalModal::FULL:
            return "@local!";
        default:
            CJC_ABORT();
            return "";
    }
}

std::string ModalInfo::AsTypeSuffixString() const
{
    auto b = Cangjie::ToString(local);
    if (b.empty()) {
        return {};
    }
    return std::string{" "} + std::string{b};
}

int ToIndex(ModalInfo modal)
{
    return static_cast<int>(modal.local) - 1;
}

ModalInfo ToModalInfo(int index)
{
    return ModalInfo{static_cast<LocalModal>(index + 1)};
}

LocalModal operator|(LocalModal lhs, LocalModal rhs)
{
    return static_cast<LocalModal>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

ModalInfo ModalInfo::operator|(ModalInfo other) const
{
    return {local | other.local};
}

bool ModalInfo::IsSubModal(ModalInfo other) const
{
    return (local | other.local) == other.local;
}

} // namespace Cangjie
