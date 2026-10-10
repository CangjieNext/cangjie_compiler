// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_CHIR_CHECKER_ANNOTATIONMETADATA_H
#define CANGJIE_CHIR_CHECKER_ANNOTATIONMETADATA_H

#include <optional>
#include "cangjie/AST/Node.h"
#include "cangjie/CHIR/IR/ResolvedAnnotationData.h"
#include "cangjie/CHIR/IR/Value/Value.h"

namespace Cangjie::CHIR {
using ResolvedAnnotationMap = std::unordered_map<const AST::Decl*, std::vector<ResolvedAnnotationData>>;

std::optional<std::vector<EvaluatedAnnotationField>> TryReadAnnotationScalarFields(const GlobalVar& instance);
std::optional<ResolvedAnnotationMap> ReadResolvedAnnotationMetadata(
    const std::vector<std::pair<const AST::Decl*, Function*>>& factories,
    const std::unordered_map<unsigned, std::string>& fileNames, bool scalarsEvaluated = true);
} // namespace Cangjie::CHIR
#endif
