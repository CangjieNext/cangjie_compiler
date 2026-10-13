// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 * This file implements CJMP (Cangjie Multi-Platform) related loading functions.
 */

#ifndef CANGJIE_MODULES_ASTSERIALIZATION_ASTLOADER_CJMP_H
#define CANGJIE_MODULES_ASTSERIALIZATION_ASTLOADER_CJMP_H

#include "ASTLoaderImpl.h"

#include "cangjie/AST/Node.h"
#include "cangjie/Basic/DiagnosticEngine.h"

#include "ASTSerializeUtils.h"
#include "flatbuffers/CjoFormat_generated.h"

#include <optional>
#include <string>

namespace Cangjie {

namespace ASTLoaderCJMP {

OwnedPtr<AST::FeaturesDirective> LoadFeaturesDirective(const PackageFormat::FeaturesDirective* raw);

void ReportPackageMismatch(
    DiagnosticEngine& diag, AST::Package& node, std::string& expectedPackageName, std::string& actualPackageName);

std::set<std::string> CollectFeaturesFromPackage(AST::Package& pkg);

/**
 * @brief Find the first problem which makes a cjo unusable as a common part: a missing field used when the common
 * part is loaded, inconsistent per-file data or a position referring to an unknown file or package.
 * @return Description of the problem, or nothing if the cjo can be loaded as a common part.
 */
std::optional<std::string> FindCommonPartCjoDefect(const PackageFormat::Package& package);

/**
 * @brief Report that the cjo can not be used as a common part, with a note on the reason: a cjo of a non common
 * part compilation, or the @p defect of a cjo with the CJMP file info.
 */
void ReportUnusableCommonPartCjo(DiagnosticEngine& diag, const std::string& cjoPath, const std::string& packageName,
    const PackageFormat::Package& package, const std::optional<std::string>& defect);

void ValidateCommonSpecificFeatureSetsRelations(
    const AST::File& commonFile,
    const std::set<std::string>& specificFeatures,
    DiagnosticEngine& diag,
    const AST::Package& pkg);

} // namespace ASTLoaderCJMP
} // namespace Cangjie

#endif
