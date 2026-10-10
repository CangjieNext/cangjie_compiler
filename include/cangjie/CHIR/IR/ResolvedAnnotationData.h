// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_CHIR_RESOLVED_ANNOTATION_DATA_H
#define CANGJIE_CHIR_RESOLVED_ANNOTATION_DATA_H

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace Cangjie::CHIR {
struct EvaluatedAnnotationField {
    std::string name;
    std::variant<std::string, bool> value;
};

// Owns all values copied out of the auxiliary constant-evaluation context.
struct ResolvedAnnotationData {
    std::string qualifiedClassName;
    std::string declarationSymbol;
    std::string sourceFile;
    unsigned fileId{0};
    unsigned beginLine{0};
    unsigned beginColumn{0};
    unsigned endLine{0};
    unsigned endColumn{0};
    // nullopt is Unavailable; an empty vector is Complete with no fields.
    std::optional<std::vector<EvaluatedAnnotationField>> scalarFields;
};
} // namespace Cangjie::CHIR

#endif
