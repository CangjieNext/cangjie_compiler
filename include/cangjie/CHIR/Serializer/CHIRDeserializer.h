// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_CHIR_SERIALIZER_DESERIALIZER_H
#define CANGJIE_CHIR_SERIALIZER_DESERIALIZER_H

#include "cangjie/AST/Node.h"
#include "cangjie/CHIR/CHIR.h"
#include "cangjie/CHIR/IR/CHIRBuilder.h"

#include <string>
#include <unordered_map>

namespace Cangjie::CHIR {
class CHIRDeserializer {
    class CHIRDeserializerImpl;

public:
    /// \param canonicalCommonDecls AST winners (rawMangle → Decl*). Required when compilePlatform.
    static bool Deserialize(const std::string& fileName, Cangjie::CHIR::CHIRBuilder& chirBuilder, ToCHIR::Phase& phase,
        bool compilePlatform = false,
        const std::unordered_map<std::string, Ptr<AST::Decl>>* canonicalCommonDecls = nullptr);
    static bool Deserialize(uint8_t* data, int64_t size, CHIRBuilder& chirBuilder);

private:
    explicit CHIRDeserializer()
    {
    }
};

} // namespace Cangjie::CHIR

#endif
