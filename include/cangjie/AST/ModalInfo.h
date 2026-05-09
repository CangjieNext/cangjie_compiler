// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * Modal type info (LocalModal, ModalInfo) shared by AST and CHIR.
 */

#ifndef CANGJIE_UTILS_MODALINFO_H
#define CANGJIE_UTILS_MODALINFO_H

#include <cstdint>
#include <string>
#include <string_view>

namespace Cangjie {

enum class LocalModal : uint8_t {
    NOT = 1,  // @~local
    FULL = 2, // @local!
    HALF = 3, // @local?
};
std::string_view ToString(LocalModal modal);
/// Returns the LocalModal that both \ref lhs and \ref rhs can be converted to.
LocalModal operator|(LocalModal lhs, LocalModal rhs);

/// Modal type info, used by Ty and CHIR Type.
/// This type is small, so pass it by value.
struct ModalInfo {
    LocalModal local;
    /// Construct ModalInfo that does not care about local modal, which can be converted to any other one.
    ModalInfo() : local(LocalModal::NOT)
    {
    }
    ModalInfo(LocalModal modal) : local(modal)
    {
    }
    bool operator==(const ModalInfo& other) const
    {
        return local == other.local;
    }
    bool operator!=(const ModalInfo& other) const
    {
        return local != other.local;
    }
    std::string ToString() const;
    std::string LocalString() const;
    std::string AsTypeSuffixString() const;
    /// Returns the ModalInfo that both \ref lhs and \ref rhs can be converted to.
    ModalInfo operator|(ModalInfo other) const;
    ModalInfo operator&(ModalInfo other) const;

    bool IsSubModal(ModalInfo other) const;
};
constexpr int MODAL_INFO_COUNT = 3;
int ToIndex(ModalInfo modal);
ModalInfo ToModalInfo(int index);

} // namespace Cangjie

#endif // CANGJIE_UTILS_MODALINFO_H
