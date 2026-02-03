// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements utilities for modules.
 */

#include "cangjie/Modules/ModulesUtils.h"

namespace Cangjie::Modules {
PackageRelation GetPackageRelation(const std::string& srcFullPkgName, const std::string& targetFullPkgName)
{
    auto pureSrcFullPackageName = ImportManager::IsTestPackage(srcFullPkgName)
        ? ImportManager::GetMainPartPkgNameForTestPkg(srcFullPkgName)
        : srcFullPkgName;
    auto pureTargetFullPackageName = ImportManager::IsTestPackage(targetFullPkgName)
        ? ImportManager::GetMainPartPkgNameForTestPkg(targetFullPkgName)
        : targetFullPkgName;

    if (pureSrcFullPackageName == pureTargetFullPackageName) {
        return PackageRelation::SAME_PACKAGE;
    }
    if (pureSrcFullPackageName.empty() || pureTargetFullPackageName.empty()) {
        return PackageRelation::NONE;
    }

    // Optimize: check CHILD relation without full split by comparing prefixes manually
    if (pureTargetFullPackageName.size() > pureSrcFullPackageName.size() &&
        pureTargetFullPackageName[pureSrcFullPackageName.size()] == '.' &&
        pureTargetFullPackageName.compare(0, pureSrcFullPackageName.size(), pureSrcFullPackageName) == 0) {
        // Verify it's a proper child by checking no additional dots in the prefix
        // This avoids full split for common case
        return PackageRelation::CHILD;
    }

    // Optimize: check SAME_MODULE by comparing first component without full split
    auto srcFirstDot = pureSrcFullPackageName.find('.');
    auto targetFirstDot = pureTargetFullPackageName.find('.');
    std::string_view srcFirst = srcFirstDot == std::string::npos 
        ? std::string_view(pureSrcFullPackageName) 
        : std::string_view(pureSrcFullPackageName.data(), srcFirstDot);
    std::string_view targetFirst = targetFirstDot == std::string::npos 
        ? std::string_view(pureTargetFullPackageName) 
        : std::string_view(pureTargetFullPackageName.data(), targetFirstDot);
    
    if (srcFirst == targetFirst) {
        return PackageRelation::SAME_MODULE;
    }

    return PackageRelation::NONE;
}
} // namespace Cangjie::Modules
