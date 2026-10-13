// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 * This file implements CJMP (Common JavaScript/Multi-platform) related loading functions.
 */

#include "ASTLoaderCJMP.h"

#include "cangjie/AST/Node.h"
#include "cangjie/AST/Utils.h"
#include "cangjie/Basic/DiagnosticEngine.h"

#include "ASTLoaderImpl.h"
#include "cangjie/Basic/Position.h"
#include "cangjie/Modules/ASTSerialization.h"
#include "cangjie/Option/Option.h"
#include "flatbuffers/CjoFormat_generated.h"

#include <optional>

namespace Cangjie {

namespace ASTLoaderCJMP {

OwnedPtr<AST::FeaturesDirective> LoadFeaturesDirective(const PackageFormat::FeaturesDirective* raw)
{
    auto ftrDirective = MakeOwned<AST::FeaturesDirective>();
    ftrDirective->featuresSet = MakeOwned<AST::FeaturesSet>();

    for (uoffset_t i = 0; i < raw->featuresSet()->features()->size(); i++) {
        auto rawFeature = raw->featuresSet()->features()->Get(i);
        auto feature = AST::FeatureId();
        for (uoffset_t j = 0; j < rawFeature->identifiers()->size(); j++) {
            auto identifier = rawFeature->identifiers()->Get(j);
            feature.identifiers.push_back(SrcIdentifier(identifier->str()));
        }

        ftrDirective->featuresSet->content.push_back(feature);
    }

    return ftrDirective;
}

void ReportPackageMismatch(
    DiagnosticEngine& diag, AST::Package& node, std::string& expectedPackageName, std::string& actualPackageName)
{
    Range range = MakeRange(node.GetBegin(), node.GetEnd());
    if (range.HasZero()) {
        CJC_ASSERT_WITH_MSG(!node.files.empty(), "There are no files in the current package");
        auto& file = *node.files[0];
        range = MakeRange(file.GetBegin(), file.GetEnd());
    }

    diag.DiagnoseRefactor(
        DiagKindRefactor::module_common_cjo_wrong_package, range, actualPackageName, expectedPackageName);
}

std::set<std::string> CollectFeaturesFromPackage(AST::Package& pkg)
{
    if (pkg.files.empty()) {
        return {};
    }
    return (*pkg.files.begin())->GetFeatures();
}

namespace {
// Positions of the current package refer to its files by 'file' index plus 1, and positions of other packages
// refer to the imported packages by 'pkgId' index plus 1, see 'ASTLoaderImpl::LoadPos'.
bool IsValidPosition(const PackageFormat::Position* pos, uoffset_t nFiles, uoffset_t nImports)
{
    if (pos == nullptr) {
        return true;
    }
    if (pos->pkgId() == INVALID_FORMAT_INDEX) {
        return pos->file() == INVALID_FORMAT_INDEX || pos->file() <= nFiles;
    }
    return pos->pkgId() <= nImports;
}

template <typename T>
std::optional<std::string> FindInvalidPosition(
    const flatbuffers::Vector<flatbuffers::Offset<T>>& nodes, const std::string& kind, uoffset_t nFiles,
    uoffset_t nImports)
{
    for (uoffset_t i = 0; i < nodes.size(); i++) {
        auto node = nodes.Get(i);
        if (!IsValidPosition(node->begin(), nFiles, nImports) || !IsValidPosition(node->end(), nFiles, nImports)) {
            return kind + " " + std::to_string(i + 1) + " refers to an unlisted file or package";
        }
    }
    return std::nullopt;
}
} // namespace

std::optional<std::string> FindCommonPartCjoDefect(const PackageFormat::Package& package)
{
    // FlatBuffers verifier accepts absent fields, so every field used when preloading and loading the common part
    // is checked here explicitly. An empty vector is valid, an absent one is not.
    const std::vector<std::pair<const void*, std::string>> requiredFields = {
        {package.allFiles(), "allFiles"},
        {package.allFileImports(), "allFileImports"},
        {package.allFileInfo(), "allFileInfo"},
        {package.allTypes(), "allTypes"},
        {package.allDecls(), "allDecls"},
        {package.allExprs(), "allExprs"},
        {package.imports(), "imports"},
        {package.options(), "options"},
    };
    for (auto& [field, name] : requiredFields) {
        if (field == nullptr) {
            return "the field '" + name + "' is missing";
        }
    }
    auto nFiles = package.allFiles()->size();
    if (package.allFileImports()->size() != nFiles || package.allFileInfo()->size() != nFiles) {
        return "file imports or CJMP data do not match the file list";
    }
    auto nImports = package.imports()->size();
    for (uoffset_t i = 0; i < nFiles; i++) {
        auto importSpecs = package.allFileImports()->Get(i)->importSpecs();
        auto fileInfo = package.allFileInfo()->Get(i);
        if (importSpecs == nullptr || fileInfo->begin() == nullptr || fileInfo->end() == nullptr) {
            return "the import list or position of file " + std::to_string(i + 1) + " is missing";
        }
        for (uoffset_t j = 0; j < importSpecs->size(); j++) {
            auto importSpec = importSpecs->Get(j);
            if (importSpec->begin() == nullptr || importSpec->end() == nullptr) {
                return "the position of an import of file " + std::to_string(i + 1) + " is missing";
            }
        }
    }
    if (auto defect = FindInvalidPosition(*package.allDecls(), "declaration", nFiles, nImports)) {
        return defect;
    }
    return FindInvalidPosition(*package.allExprs(), "expression", nFiles, nImports);
}

void ReportUnusableCommonPartCjo(DiagnosticEngine& diag, const std::string& cjoPath, const std::string& packageName,
    const PackageFormat::Package& package, const std::optional<std::string>& defect)
{
    auto builder = diag.DiagnoseRefactor(
        DiagKindRefactor::module_common_cjo_not_common_part, DEFAULT_POSITION, cjoPath, packageName);
    // Only a common part compilation writes the CJMP file info, so a cjo with it is a damaged common part.
    if (package.allFileInfo() != nullptr) {
        builder.AddNote("its common part information is incomplete or corrupted:\n         " +
            defect.value_or("unknown"));
    } else {
        builder.AddNote("this cjo was not produced by a common part compilation\n"
                        "         with '--output-type=chir'");
        builder.AddNote("a specific part compilation writes '" + packageName + ".cjo' to its\n"
                        "         output directory and may have overwritten the common part cjo");
    }
    builder.AddHelp(DiagHelp("recompile the common part with '--output-type=chir'\n"
                             "         and keep its '.cjo' and '.chir' together in a separate\n"
                             "         directory, e.g. with '--output-dir'"));
}

void ValidateCommonSpecificFeatureSetsRelations(
    const AST::File& commonFile,
    const std::set<std::string>& specificFeatures,
    DiagnosticEngine& diag,
    const AST::Package& pkg)
{
    auto parentFeatures = commonFile.GetFeatures();
    bool isParentSubsetOfChild = std::includes(
        specificFeatures.begin(), specificFeatures.end(),
        parentFeatures.begin(), parentFeatures.end());
    if (!isParentSubsetOfChild) {
        std::vector<std::string> difference;
        std::set_difference(
            parentFeatures.begin(), parentFeatures.end(),
            specificFeatures.begin(), specificFeatures.end(),
            std::back_inserter(difference)
        );
        std::stringstream ss;
        ss << "Extra feature from common part file: ";
        for (const auto& feature : difference) {
            ss << "'" << feature << "' ";
        }
        auto diagBuilder = diag.DiagnoseRefactor(
            DiagKindRefactor::feature_is_not_subset_of_child_set,
            commonFile.GetBegin(), commonFile.GetFullPackageName(), commonFile.filePath
        );
        diagBuilder.AddNote(ss.str());
        Ptr<AST::File> childFile = *pkg.files.begin();
        if (childFile->feature) {
            diagBuilder.AddNote(*childFile->feature, "Conflicting specific feature set:");
        }
    }
}

/**
 * @brief Convert optimization level enum to string for diagnostics
 * @param level The optimization level to convert
 * @return String representation of the optimization level
 */
std::string OptimizationLevelToString(GlobalOptions::OptimizationLevel level)
{
    switch (level) {
        case GlobalOptions::OptimizationLevel::O0:
            return "O0";
        case GlobalOptions::OptimizationLevel::O1:
            return "O1";
        case GlobalOptions::OptimizationLevel::O2:
            return "O2";
        case GlobalOptions::OptimizationLevel::O3:
            return "O3";
        case GlobalOptions::OptimizationLevel::Os:
            return "Os";
        case GlobalOptions::OptimizationLevel::Oz:
            return "Oz";
        default:
            return "(unsupported)";
    }
}

} // namespace ASTLoaderCJMP

OwnedPtr<AST::File> ASTLoader::ASTLoaderImpl::PreloadCommonFile(uoffset_t indexOfFile)
{
    CJC_NULLPTR_CHECK(package->allFileImports());
    uoffset_t i = indexOfFile;
    auto&& importSpecs = LoadImportSpecs(package->allFileImports()->Get(i));
    CJC_NULLPTR_CHECK(package->allFileInfo());

    auto fileInfo = package->allFileInfo()->Get(i);
    auto tmpFilePath = package->allFiles()->Get(i)->str();
    const bool isCJMPFile = true;
    auto tmpFileId = sourceManager.AddSource(tmpFilePath, "", package->fullPkgName()->str(), isCJMPFile);
    CJC_ASSERT(tmpFileId == fileInfo->fileID());
    allFileIds[i] = tmpFileId;

    auto file = CreateFileNode(*curPackage, fileInfo->fileID(), std::move(importSpecs));
    if (fileInfo->feature()) {
        file->feature = ASTLoaderCJMP::LoadFeaturesDirective(fileInfo->feature());
    }

    file->EnableAttr(AST::Attribute::FROM_COMMON_PART);
    file->EnableAttr(AST::Attribute::COMMON);
    file->isCommon = true;
    file->begin = LoadPos(fileInfo->begin());
    file->end = LoadPos(fileInfo->end());
    return file;
}

/**
 * @brief Validate that compilation options match between common cjo and current compile
 * @return true if options match, false if there is a mismatch
 */
bool ASTLoader::ASTLoaderImpl::ValidateOptions()
{
    // Presence of the options is guaranteed by 'FindCommonPartCjoDefect'.
    auto options = package->options();
    CJC_NULLPTR_CHECK(options);

    bool failed = false;
    if (options->debug() != this->opts.enableCompileDebug) {
        std::string before = options->debug() ? "enabled" : "disabled";
        std::string after = opts.enableCompileDebug ? "enabled" : "disabled";
        diag.DiagnoseRefactor(DiagKindRefactor::module_common_cjo_debug_mismatch, DEFAULT_POSITION, before, after);
        failed = true;
    }

    auto cjoOptLevel = LoadOptimizationLevel(*options);
    if (opts.optimizationLevel != cjoOptLevel) {
        std::string before = ASTLoaderCJMP::OptimizationLevelToString(cjoOptLevel);
        std::string after = ASTLoaderCJMP::OptimizationLevelToString(opts.optimizationLevel);
        diag.DiagnoseRefactor(DiagKindRefactor::module_common_cjo_opt_mismatch, DEFAULT_POSITION, before, after);
        failed = true;
    }

    return !failed;
}

bool ASTLoader::ASTLoaderImpl::PreloadCommonPartOfPackage(AST::Package& pkg)
{
    // The loader stays rejected unless all checks pass and the files are preloaded, so that its decls are never
    // loaded with incomplete file mapping, see 'CanLoadDecls'.
    commonPartState = CommonPartState::REJECTED;
    if (!PreloadCommonPartFiles(pkg)) {
        return false;
    }
    commonPartState = CommonPartState::PRELOADED;
    return true;
}

bool ASTLoader::ASTLoaderImpl::PreloadCommonPartFiles(AST::Package& pkg)
{
    if (!VerifyForData("ast")) {
        return false;
    }

    package = PackageFormat::GetPackage(data.data());
    CJC_NULLPTR_CHECK(package);
    if (!CheckCjoVersion()) {
        return false;
    }
    if (package->fullPkgName() == nullptr) {
        ASTLoaderCJMP::ReportUnusableCommonPartCjo(
            diag, cjoPath, pkg.fullPackageName, *package, "the field 'fullPkgName' is missing");
        return false;
    }

    curPackage = &pkg;

    auto deserializedPackageName = package->fullPkgName()->str();
    if (pkg.fullPackageName != deserializedPackageName) {
        ASTLoaderCJMP::ReportPackageMismatch(diag, pkg, pkg.fullPackageName, deserializedPackageName);
        return false;
    }
    // Reject a cjo which is not a common part or has incomplete data before using it, e.g. a specific part cjo
    // which has overwritten the common part one in the same output directory: it does not match the common part chir.
    if (auto defect = ASTLoaderCJMP::FindCommonPartCjoDefect(*package)) {
        ASTLoaderCJMP::ReportUnusableCommonPartCjo(diag, cjoPath, deserializedPackageName, *package, defect);
        return false;
    }
    auto specificFeatures = ASTLoaderCJMP::CollectFeaturesFromPackage(pkg);

    allTypes.resize(package->allTypes()->size(), nullptr);
    auto fileSize = package->allFiles()->size();
    allFileIds.resize(fileSize);

    for (uoffset_t i = 0; i < fileSize; i++) {
        auto file = PreloadCommonFile(i);

        ASTLoaderCJMP::ValidateCommonSpecificFeatureSetsRelations(*file, specificFeatures, diag, pkg);

        pkg.files.emplace_back(std::move(file));
    }

    if (!ValidateOptions()) {
        // options mismatch must abort compilation
        // otherwise the difference in desugaring/chir generation
        // may lead to a crash in subsequent compilation stages
        return false;
    }

    AddCurFile(pkg);
    auto imports = package->imports();
    uoffset_t nImports = imports->size();
    for (uoffset_t i = 0; i < nImports; i++) {
        std::string importItem = imports->Get(i)->str();
        importedFullPackageNames.emplace_back(importItem);
    }

    return true;
}

} // namespace Cangjie
