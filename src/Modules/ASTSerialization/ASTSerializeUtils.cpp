// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "ASTSerializeUtils.h"

#include "cangjie/Utils/CheckUtils.h"

namespace Cangjie {
namespace {
/**
 * Convert AST::LocalModal to PackageFormat::LocalMode.
 * AST::LocalModal values: NOT=1, FULL=2, HALF=3
 * PackageFormat::LocalMode values: NOT=0, FULL=1, HALF=2
 */
PackageFormat::LocalMode AstLocalToFormat(AST::LocalModal astModal)
{
    return static_cast<PackageFormat::LocalMode>(static_cast<int>(astModal) - 1);
}

/**
 * Convert PackageFormat::LocalMode to AST::LocalModal.
 * PackageFormat::LocalMode values: NOT=0, FULL=1, HALF=2
 * AST::LocalModal values: NOT=1, FULL=2, HALF=3
 */
AST::LocalModal FormatToAstLocal(PackageFormat::LocalMode formatModal)
{
    return static_cast<AST::LocalModal>(static_cast<int>(formatModal) + 1);
}
} // namespace

TModeOffset SaveModal(flatbuffers::FlatBufferBuilder& fbb, AST::ModalInfo modal)
{
    if (modal == AST::ModalInfo{}) {
        return {};
    }
    return PackageFormat::CreateMode(fbb, AstLocalToFormat(modal.local));
}

AST::ModalInfo ReadModal(const PackageFormat::Mode* mode)
{
    if (mode == nullptr) {
        return {};
    }
    return {FormatToAstLocal(mode->local())};
}

TVectorOffset<TModeOffset> CreateTyModesVector(
    flatbuffers::FlatBufferBuilder& fbb, const std::vector<TModeOffset>& modes)
{
    if (modes.empty()) {
        return {};
    }
    for (const auto& mode : modes) {
        if (mode.o != 0) {
            return fbb.CreateVector(modes);
        }
    }
    return {};
}

TModeOffset SaveModeTable(flatbuffers::FlatBufferBuilder& fbb, AST::ModalInfo modal)
{
    return PackageFormat::CreateMode(fbb, AstLocalToFormat(modal.local));
}

TVectorOffset<TModeOffset> SaveModalVector(
    flatbuffers::FlatBufferBuilder& fbb, const std::vector<AST::ModalInfo>& modals)
{
    for (const auto& modal : modals) {
        if (modal != AST::ModalInfo{}) {
            std::vector<TModeOffset> offsets;
            offsets.reserve(modals.size());
            for (const auto& m : modals) {
                offsets.push_back(SaveModeTable(fbb, m));
            }
            return CreateTyModesVector(fbb, offsets);
        }
    }
    return {};
}

AST::ModalTy ApplyLoadedModal(AST::DataTy ty, const PackageFormat::Mode* mode)
{
    if (mode == nullptr) {
        return ty;
    }
    return AST::ModalTy{ty, ReadModal(mode)};
}
} // namespace Cangjie
