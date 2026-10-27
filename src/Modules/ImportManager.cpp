// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements the ImportManager related classes.
 */

#include "cangjie/Modules/ImportManager.h"
#include "cangjie/Parse/ParseModifiersRules.h"
#include "cangjie/Parse/Parser.h"

#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ModulesDiag.h"

#include "cangjie/AST/Clone.h"
#include "cangjie/AST/Create.h"
#include "cangjie/AST/Match.h"
#include "cangjie/AST/Utils.h"
#include "cangjie/AST/Walker.h"
#include "cangjie/Basic/SourceManager.h"
#include "cangjie/ConditionalCompilation/ConditionalCompilation.h"
#include "cangjie/Frontend/CompilerInstance.h"
#include "cangjie/Modules/ASTSerialization.h"
#include "cangjie/Modules/ModulesUtils.h"
#include "cangjie/Utils/ProfileRecorder.h"

using namespace Cangjie;
using namespace AST;
using namespace Modules;

namespace {
std::string GetPackageNameFromMacroImport(const std::string& importStr)
{
    if (importStr.size() >= 2 && importStr.compare(importStr.size() - 2, 2, ".*") == 0) {
        return importStr.substr(0, importStr.size() - 2);
    }
    auto lastDot = importStr.rfind('.');
    if (lastDot != std::string::npos) {
        return importStr.substr(0, lastDot);
    }
    return importStr;
}

void AddImplicitImportAll(File& file, const std::string& fullPackageName)
{
    // Check whether the prefix paths are already cached.
    static FullPackageNameToPrefixPaths fullPackageNameToPrefixPaths;
    std::vector<std::string> curPrefixPaths{};
    if (fullPackageNameToPrefixPaths.find(fullPackageName) != fullPackageNameToPrefixPaths.end()) {
        curPrefixPaths = fullPackageNameToPrefixPaths.at(fullPackageName);
    } else {
        curPrefixPaths = Utils::SplitQualifiedName(fullPackageName);
        fullPackageNameToPrefixPaths.insert({fullPackageName, curPrefixPaths});
    }
    // Check whether the import is already added.
    for (auto it = file.imports.rbegin(); it != file.imports.rend(); ++it) {
        auto& import = *it;
        if (!import->TestAttr(Attribute::IMPLICIT_ADD)) {
            continue;
        }
        if (import->content.kind != ImportKind::IMPORT_ALL) {
            continue;
        }
        if (import->content.prefixPaths == fullPackageNameToPrefixPaths.at(fullPackageName)) {
            return; // The import is already added not add again.
        }
    }
    // Add the import if not added.
    file.imports.emplace_back(CreateImportSpec(fullPackageName, "*", "", fullPackageNameToPrefixPaths));
}

void AddImplicitImports(Package& pkg, const GlobalOptions& opts)
{
    for (auto& file : pkg.files) {
        CJC_NULLPTR_CHECK(file);
        if (opts.implicitPrelude) {
            AddImplicitImportAll(*file, CORE_PACKAGE_NAME);
        }
        if (opts.enableCompileTest) {
            AddImplicitImportAll(*file, "std.unittest");
            AddImplicitImportAll(*file, "std.unittest.testmacro");
        }
        if (opts.compileTestsOnly && ImportManager::IsTestPackage(pkg.fullPackageName)) {
            AddImplicitImportAll(*file, ImportManager::GetMainPartPkgNameForTestPkg(pkg.fullPackageName));
        }
    }
}

struct DependencyInfoItem {
    std::string package;
    std::string orPackage;
    bool isStd{false};
    std::set<Ptr<ImportSpec>, CmpNodeByPos> imports;
};

std::string Jsonfy(bool b)
{
    return b ? "true" : "false";
}

std::string Jsonfy(AccessLevel level)
{
    CJC_ASSERT(level != AccessLevel::PRIVATE);
    return level == AccessLevel::PUBLIC ? "public" : level == AccessLevel::PROTECTED ? "protected" : "internal";
}

std::string Jsonfy(const std::string& s)
{
    // Accroding to RFC 8259: All Unicode characters may be placed within the quotation marks,
    // except for the characters that MUST be escaped: quotation mark, reverse solidus,
    // and the control characters (U+0000 through U+001F)
    std::ostringstream out;
    for (char c : s) {
        if (c == '\\' || c == '"') {
            out << '\\' << c;
        } else if ('\x00' <= c && c <= '\x1f') {
            // According to RFC 8259: 4 hexadecimal digits are used to encode the character's code point.
            out << '\\' << 'u' << std::setfill('0') << std::setw(4) << std::hex << static_cast<int>(c);
        } else {
            out << c;
        }
    }
    return out.str();
}

std::string Jsonfy(const Position& pos)
{
    std::ostringstream out;
    out << "{"
        << "\"line\":" << pos.line << ","
        << "\"column\":" << pos.column << "}";
    return out.str();
}

std::string Jsonfy(const ImportSpec& import, bool exportCJO)
{
    CJC_NULLPTR_CHECK(import.curFile);
    std::ostringstream out;
    out << "{"
        << "\"file\":\"" << Jsonfy(exportCJO ? import.curFile->fileName : import.curFile->filePath) << "\","
        << "\"begin\":" << Jsonfy(import.begin) << ","
        << "\"end\":" << Jsonfy(import.end) << "}";
    return out.str();
}

std::string Jsonfy(const std::set<Ptr<ImportSpec>, CmpNodeByPos>& imports, bool exportCJO)
{
    if (imports.empty()) {
        return "[]";
    }
    std::ostringstream out;
    out << "[";
    auto iter = imports.cbegin();
    out << Jsonfy(**iter, exportCJO);
    ++iter;
    while (iter != imports.cend()) {
        out << "," << Jsonfy(**iter, exportCJO);
        ++iter;
    }
    out << "]";
    return out.str();
}

std::string Jsonfy(const DependencyInfoItem& info, bool exportCJO)
{
    std::ostringstream out;
    out << "{" << "\"package\":\"" << Jsonfy(info.package) << "\",";
    if (!info.orPackage.empty()) {
        out << "\"orPackage\":\"" << Jsonfy(info.orPackage) << "\",";
    }
    out << "\"isStd\":" << Jsonfy(info.isStd) << ",";
    out << "\"imports\":" << Jsonfy(info.imports, exportCJO) << "}";
    return out.str();
}

std::string Jsonfy(const std::map<std::string, DependencyInfoItem>& dependencies, bool exportCJO)
{
    if (dependencies.empty()) {
        return "[]";
    }
    std::ostringstream out;
    out << "[";
    auto iter = dependencies.cbegin();
    out << Jsonfy(iter->second, exportCJO);
    ++iter;
    while (iter != dependencies.cend()) {
        out << "," << Jsonfy(iter->second, exportCJO);
        ++iter;
    }
    out << "]";
    return out.str();
}

std::string Jsonfy(const std::set<std::string>& features)
{
    if (features.empty()) {
        return "[]";
    }
    std::ostringstream out;
    out << "[";
    auto iter = features.cbegin();
    out << "\"" << Jsonfy(*iter) << "\"";
    ++iter;
    while (iter != features.cend()) {
        out << "," << "\"" << Jsonfy(*iter) << "\"";
        ++iter;
    }
    out << "]";
    return out.str();
}

std::string Jsonfy(const std::map<std::string, std::set<std::string>>& dependencies)
{
    if (dependencies.empty()) {
        return "[]";
    }
    std::ostringstream out;
    out << "[";
    auto iter = dependencies.begin();
    while (iter != dependencies.end()) {
        out << "{\"" << iter->first << "\":";
        out << Jsonfy(iter->second) + "}";
        if (++iter != dependencies.end()) {
            out << ",";
        }
    }
    out << "]";
    return out.str();
}

void CollectStdDependency(std::map<std::string, std::set<std::string>>& stdDependencies, const std::string& stdpkg,
    const ImportManager& importMgr, CjoManager& cjoManager)
{
    if (stdDependencies.find(stdpkg) != stdDependencies.end()) {
        return;
    }
    // Only collect packages that ship as separately linkable std libs.
    // Private support packages such as `flatbuffers` (force-linked into std.ast,
    // cjo under third_party/flatbuffers/modules, no libcangjie-std-flatbuffers)
    // must not appear in std-dependencies or cjpm will emit a missing -l.
    if (STANDARD_LIBS.find(stdpkg) == STANDARD_LIBS.end()) {
        return;
    }
    std::string cjoPath = FileUtil::FindSerializationFile(
        FileUtil::ToPackageName(stdpkg), SERIALIZED_FILE_EXTENSION, importMgr.GetSearchPath());
    if (cjoPath.empty()) {
        importMgr.GetDiagnosticEngine().DiagnoseRefactor(DiagKindRefactor::package_search_error, DEFAULT_POSITION,
            stdpkg);
        return;
    }
    if (!cjoManager.LoadPackageHeader(stdpkg, cjoPath)) {
        return;
    }
    std::set<std::string> importInfo;
    Ptr<Package> stdPkgInfo = cjoManager.GetPackage(stdpkg);
    CJC_NULLPTR_CHECK(stdPkgInfo);
    for (auto& file : stdPkgInfo->files) {
        for (auto& import : file->imports) {
            auto imported = import->content.GetImportedPackageNameWithIsDecl();
            if (STANDARD_LIBS.find(imported) == STANDARD_LIBS.end()) {
                continue;
            }
            importInfo.emplace(imported);
        }
    }
    for (auto& importRes : importInfo) {
        CollectStdDependency(stdDependencies, importRes, importMgr, cjoManager);
    }
    stdDependencies.emplace(stdpkg, std::move(importInfo));
    return;
}

std::map<std::string, std::set<std::string>> GetStdDependency(
    const std::map<std::string, DependencyInfoItem>& dependencies, const Package& pkg, bool exportCJO,
    const ImportManager& importMgr, CjoManager& cjoManager)
{
    std::map<std::string, std::set<std::string>> stdDependencies{{"std.core", {}}};
    if (exportCJO) {
        for (const auto& stdpkg : pkg.GetAllDependentStdPkgs()) {
            CollectStdDependency(stdDependencies, stdpkg, importMgr, cjoManager);
        }
    } else {
        for (const auto& [_, dependInfo] : dependencies) {
            if (dependInfo.isStd) {
                CollectStdDependency(stdDependencies, dependInfo.package, importMgr, cjoManager);
            }
        }
    }
    return stdDependencies;
}

Range GetPackageNameRange(const CjoManager& cjoManager, const ImportSpec& import)
{
    auto& im = import.content;
    return import.TestAttr(Attribute::IMPLICIT_ADD)
        ? MakeRange(DEFAULT_POSITION, im.identifier)
        : (cjoManager.IsImportPackage(import)
                  ? (im.prefixPoses.empty() ? MakeRange(im.identifier)
                                            : MakeRange(im.prefixPoses.front(), im.identifier.End()))
                  : MakeRange(im.prefixPoses.front(), im.prefixPoses.back() + im.prefixPaths.back().size()));
}
} // namespace

void ImportManager::ExportAST(bool saveFileWithAbsPath, std::vector<uint8_t>& astData, const Package& pkg,
    const std::function<void(ASTWriter&)> additionalSerializations)
{
    ASTWriter writer(diag, GeneratePkgDepInfo(pkg),
        {
            .exportContent = importSrcCode,
            .exportForIncr = false,
            .exportForTest = opts.exportForTest,
            .needAbsPath = saveFileWithAbsPath,
            .compileCjd = opts.compileCjd,
        },
        *cjoManager, typeManager);
    if (opts.outputMode == GlobalOptions::OutputMode::CHIR) {
        writer.SetSerializingCommon();
    }
    auto realWriter = &writer;

    auto packageDecl = cjoManager->GetPackageDecl(pkg.fullPackageName);
    CJC_NULLPTR_CHECK(packageDecl);
    auto found = astWriters.find(&pkg);
    if (found != astWriters.end()) {
        realWriter = found->second;
    } else {
        writer.PreSaveFullExportDecls(*packageDecl->srcPackage);
    }
    realWriter->ExportAST(*packageDecl);
    realWriter->SaveOptions(opts.enableCompileDebug, opts.optimizationLevel);

    additionalSerializations(*realWriter);

    realWriter->AST2FB(astData, *packageDecl);
}

std::vector<uint8_t> ImportManager::ExportASTSignature(const Package& pkg)
{
    ASTWriter writer(diag, {},
        {
            .exportContent = true,
            .exportForIncr = true,
            .compileCjd = opts.compileCjd,
        },
        *cjoManager, typeManager);
    auto packageDecl = cjoManager->GetPackageDecl(pkg.fullPackageName);
    CJC_NULLPTR_CHECK(packageDecl);
    writer.PreSaveFullExportDecls(*packageDecl->srcPackage);
    writer.ExportAST(*packageDecl);
    writer.SaveOptions(opts.enableCompileDebug, opts.optimizationLevel);

    std::vector<uint8_t> astData;
    writer.AST2FB(astData, *packageDecl);
    return astData;
}

void ImportManager::ExportDeclsWithContent(bool saveFileWithAbsPath, Package& package)
{
    // NOTE: If 'importSrcCode' is disabled, we also do not need to export source code.
    auto writer = new ASTWriter(diag, GeneratePkgDepInfo(package),
        {
            .exportContent = importSrcCode,
            .exportForIncr = false,
            .exportForTest = opts.exportForTest,
            .needAbsPath = saveFileWithAbsPath,
            .compileCjd = opts.compileCjd,
        },
        *cjoManager, typeManager);
    if (opts.outputMode == GlobalOptions::OutputMode::CHIR) {
        writer->SetSerializingCommon();
    }
    writer->PreSaveFullExportDecls(package);
    writer->SaveOptions(opts.enableCompileDebug, opts.optimizationLevel);
    if (auto [it, success] = astWriters.emplace(&package, writer); !success) {
        delete it->second;
        astWriters[&package] = writer;
    }
}

std::unordered_set<std::string> ImportManager::LoadCachedTypeForPackage(
    const Package& pkg, const std::map<std::string, Ptr<Decl>>& mangledName2DeclMap)
{
    auto cachedCjo = opts.GenerateCachedPathName(pkg.fullPackageName, SERIALIZED_FILE_EXTENSION);
    return cjoManager->LoadCachedPackage(pkg, cachedCjo, mangledName2DeclMap);
}

bool ImportManager::HandleParsedPackage(
    const Package& package, const std::string& filePath, bool isUsedAsCommon, bool isRecursive)
{
    // `onlyUsedByMacro = false` means we have collected STD deps before, but `isRecursive`
    // may be different from previous scene, so we need handle for STD deps again.
    if (cjoManager->IsOnlyUsedByMacro(package.fullPackageName) &&
        (curPackage->isMacroPackage || (isUsedAsCommon && !package.isMacroPackage))) {
        // The current package has ever been macro-related, but now it's used in non-macro scene,
        // so we need collect STD deps for this package and its dependent packages.
        cjoManager->SetOnlyUsedByMacro(package.fullPackageName, false);
        HandleStdPackage(package.fullPackageName, filePath, isRecursive);
        for (auto depStdPkg : package.GetAllDependentStdPkgs()) {
            auto [it, succ] = cjoFilePaths.emplace(depStdPkg, "");
            if (succ) {
                it->second = FileUtil::FindSerializationFile(depStdPkg, SERIALIZED_FILE_EXTENSION, GetSearchPath());
            }
            HandleStdPackage(depStdPkg, it->second, true);
        }
        return ResolveImportedPackageHeaders(package, true);
    }
    return true;
}

void ImportManager::SaveDepPkgCjoPath(const std::string& fullPackageName, const std::string& cjoPath)
{
    cjoFilePaths.emplace(fullPackageName, cjoPath);
    auto cjdPath = cjoPath.substr(0, cjoPath.rfind(SERIALIZED_FILE_EXTENSION)) + CJ_D_FILE_EXTENSION;
    cjdFilePaths.emplace(fullPackageName, cjdPath);
}

void ImportManager::HandleAlreadyParsedPackage(const ParsedPackageContext& context, bool& success)
{
    CJC_ASSERT(!context.importPkg.TestAttr(Attribute::IMPORTED) ||
                context.importPkg.TestAttr(Attribute::TOOL_ADD) ||
                !context.cjoPath.empty());
    if (!context.importPkg.TestAttr(Attribute::IMPORTED) && context.curPkg.TestAttr(Attribute::IMPORTED)) {
        // Source package have same name with indirect dependent package. Was reported during loading package.
        success = false;
        diag.DiagnoseRefactor(DiagKindRefactor::module_same_name_with_indirect_dependent_pkg, DEFAULT_POSITION,
            context.fullPackageName, context.curPkg.fullPackageName);
    } else {
        success = HandleParsedPackage(context.importPkg, context.cjoPath,
                                    !context.isMacroRelated || context.isVisible, context.isRecursive) && success;
    }
}

bool ImportManager::ResolveImportedPackageForFile(File& file, bool isRecursive)
{
    bool success{true};
    auto curPkg = file.curPackage;
    CJC_NULLPTR_CHECK(curPkg);
    bool isMacroRelated = curPkg->isMacroPackage || IsMacroRelatedPackageName(curPkg->fullPackageName);
    auto relation = Modules::GetPackageRelation(curPackage->fullPackageName, curPkg->fullPackageName);
    for (auto& import : file.imports) {
        if (import->IsImportMulti() || !import->withImplicitExport) {
            continue;
        }
        auto isVisible = import->IsReExport() && Modules::IsVisible(*import, relation);
        auto [fullPackageName, cjoPath] = cjoManager->GetPackageCjo(*import);
        SaveDepPkgCjoPath(fullPackageName, cjoPath);
        // 1. Handle the package which has been parsed before.
        auto importPkg = cjoManager->GetPackage(fullPackageName);
        if (importPkg != nullptr) {
            ParsedPackageContext context{fullPackageName, cjoPath, *curPkg, *importPkg,
                                       isMacroRelated, isVisible, isRecursive};
            HandleAlreadyParsedPackage(context, success);
            continue;
        }
        auto isMainPartPkgForTestPkg = opts.compileTestsOnly && IsTestPackage(curPkg->fullPackageName) &&
            GetMainPartPkgNameForTestPkg(curPkg->fullPackageName) == fullPackageName;
        // 2. Check and try to load cjo data.
        if (!CheckCjoPathLegality(import, cjoPath, fullPackageName, isRecursive, isMainPartPkgForTestPkg) ||
            !cjoManager->LoadPackageHeader(fullPackageName, cjoPath)) {
            success = false;
            continue;
        }
        importPkg = cjoManager->GetPackage(fullPackageName);
        CJC_NULLPTR_CHECK(importPkg);
        // 3. Store loaded package node.
        // Do not need to load indirectly dependent macro package which is not re-exported and is macro related.
        // NOTE: since allowing macro package to re-export normal package,
        //       we cannot ignore macro package imported by normal package.
        if (importPkg->isMacroPackage && isRecursive && !isVisible && isMacroRelated) {
            // If it's the first time to load `fullPackageName` and current package is macro-related,
            // `fullPackageName` isn't being used now, we need to remove it and then re-import it when it's being used.
            cjoManager->RemovePackage(fullPackageName, importPkg);
            continue;
        }
        // Package are needed for CodeGen in the following scenarios:
        // a. If the package is not a macro dependency package (import by normal package or import by case c's package).
        // b. In the scenario of compiling a macro package.
        // c. If the package is not a macro package and non-internal reExported by a macro package. If the package is
        //    internal, it's only necessary if current package is its subpackage.
        auto macroReExportCommonPackage = isVisible && isMacroRelated && !importPkg->isMacroPackage;
        if ((!IsMacroRelatedPackageName(file.curPackage->fullPackageName) && !importPkg->isMacroPackage) ||
            curPackage->isMacroPackage || macroReExportCommonPackage) {
            // If package is needed for CodeGen, we also should collect the standard library dependencies.
            HandleStdPackage(fullPackageName, cjoPath, isRecursive);
            for (auto depStdPkg : importPkg->GetAllDependentStdPkgs()) {
                auto [it, succ] = cjoFilePaths.emplace(depStdPkg, "");
                if (succ) {
                    it->second = FileUtil::FindSerializationFile(depStdPkg, SERIALIZED_FILE_EXTENSION, GetSearchPath());
                }
                HandleStdPackage(depStdPkg, it->second, true);
            }
        } else {
            cjoManager->SetOnlyUsedByMacro(fullPackageName, true);
        }
        // 4. Resolve current package's dependent packages by DFS.
        success = ResolveImportedPackageHeaders(*importPkg, true) && success;
    }
    return success;
}

void ImportManager::SetImportedPackageFromASTNode(std::vector<OwnedPtr<AST::Package>>& pkgs)
{
    for (auto& pkg : pkgs) {
        cjoManager->AddImportedPackageFromASTNode(std::move(pkg));
    }
}

void ImportManager::HandleStdPackage(const std::string& fullPackageName, const std::string& cjoPath, bool isRecursive)
{
    auto type = isRecursive ? DepType::INDIRECT : DepType::DIRECT;
    auto typeWithFullPkgName = stdDepsMap.find(cjoPath);
    if (typeWithFullPkgName != stdDepsMap.end()) {
        if (typeWithFullPkgName->second.first != type) {
            typeWithFullPkgName->second.first = DepType::BOTH;
        }
        return;
    }
    if ((STANDARD_LIBS.find(fullPackageName) != STANDARD_LIBS.end()) && FileUtil::FileExist(cjoPath)) {
        stdDepsMap.emplace(cjoPath, std::make_pair(type, fullPackageName));
    }
}

bool ImportManager::CheckCjoPathLegality(const OwnedPtr<ImportSpec>& import, const std::string& cjoPath,
    const std::string& fullPackageName, bool isRecursive, bool isMainPartPkgForTestPkg)
{
    // Call guarantees the 'cjoPath' existing when it is not empty.
    if (cjoPath.empty()) {
        Range range =
            isRecursive ? MakeRange(import->begin, import->begin + 1) : GetPackageNameRange(*cjoManager, *import);
        CJC_ASSERT(!range.HasZero()); // Except for IMPLICIT_ADD, there should be no zero.

        DiagnosticBuilder builder = diag.DiagnoseRefactor(isMainPartPkgForTestPkg
                ? DiagKindRefactor::package_missed_cjo_main_pkg_part_for_test_pkg
                : DiagKindRefactor::package_search_error,
            range, fullPackageName);
        builder.AddHelp(DiagHelp(Modules::NO_CJO_HELP_INFO));
        return false;
    }
    return true;
}

bool ImportManager::ResolveImportedPackageHeaders(const Package& package, bool isRecursive)
{
    bool success{true};
    for (auto& file : package.files) {
        if (!ResolveImportedPackageForFile(*file, isRecursive)) {
            success = false;
        }
    }
    return success;
}

void ImportManager::UpdateMacroPackageUsage(const AST::Package& pkg)
{
    // Semantic rule 2: no early-return for macro packages. Record only the direct macro
    // dependency set here. macroImportStrings / macroImportPkgsFromMacroDeps are filled
    // solely by ProcessMacroImports after evaluating @When conditions — filling them
    // here (pre-evaluation) would pollute .macrocall with entries whose condition is
    // false for the current downstream target.
    std::unordered_set<std::string> visited;
    auto macroDeps = CollectTransitiveMacroDeps(pkg.fullPackageName, visited);
    for (const auto& macroDepPkgName : macroDeps) {
        auto pd = cjoManager->GetPackage(macroDepPkgName);
        if (!pd || !pd->isMacroPackage) {
            continue;
        }
        (void)directMacroDeps.emplace(pd);
    }
}

bool ImportManager::ResolveImportedPackages(const std::vector<Ptr<Package>>& packages)
{
    Utils::ProfileRecorder recorder("BuildIndex", "ResolveImportedPackages");
    bool success = true;
    Utils::ProfileRecorder::Start("ResolveImportedPackages", "ResolveImportedPackageHeaders");
    for (auto pkg : packages) {
        curPackage = pkg;
        // Files of common part need to be loaded in advance,
        // to be able to handle `import`s of common part.
        if (!cjoManager->LoadFilesOfCommonPart(pkg)) {
            return false;
        }
        success = ResolveImportedPackageHeaders(*curPackage, false) && success;
        curPackage->ClearAllDependentStdPkgs();
        for (auto [_, typeWithFullPkgName] : stdDepsMap) {
            curPackage->AddDependentStdPkg(typeWithFullPkgName.second);
        }
    }
    Utils::ProfileRecorder::Stop("ResolveImportedPackages", "ResolveImportedPackageHeaders");
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
    cjoManager->LoadPackageDeclsOnDemand(packages);
#endif
    Utils::ProfileRecorder::Start("ResolveImportedPackages", "AddPackageDeclMap");
    for (auto pkg : packages) {
        cjoManager->AddPackageDeclMap(pkg->fullPackageName);
    }
    Utils::ProfileRecorder::Stop("ResolveImportedPackages", "AddPackageDeclMap");
    return success;
}

void ImportManager::DeleteASTWriters() noexcept
{
    for (auto& [_, writer] : astWriters) {
        delete writer;
        writer = nullptr;
    }
    astWriters.clear();
}

void ImportManager::DeleteASTLoaders() noexcept
{
    if (cjoManager) {
        cjoManager->DeleteASTLoaders();
    }
}

void ImportManager::ResolveImports(const AST::Package& pkg)
{
    // NOTE: only update macro package usage for source package.
    UpdateMacroPackageUsage(pkg);
    // 1. Report errors for invalid imports.
    CheckImports(pkg);
    // 2. Report shadowing & collision warnings for source package.
    CheckRedefinition(pkg);
}

void ImportManager::CheckImports(const AST::Package& pkg)
{
    for (auto& file : pkg.files) {
        for (auto& import : file->imports) {
            if (import->IsImportMulti()) {
                continue;
            }
            auto fullPackageName = cjoManager->GetPackageNameByImport(*import);
            auto package = cjoManager->GetPackage(fullPackageName);
            if (fullPackageName.empty() || !package) {
                continue; // Failed to import the package.
            }
            auto range = GetPackageNameRange(*cjoManager, *import);
            if (fullPackageName != package->fullPackageName) {
                diag.DiagnoseRefactor(
                    DiagKindRefactor::package_import_inconsistent, range, package->fullPackageName, fullPackageName);
                continue;
            } else if (fullPackageName == pkg.fullPackageName) {
                diag.DiagnoseRefactor(DiagKindRefactor::package_import_itself_illegal, range, fullPackageName);
                continue;
            }
            if (import->TestAttr(Attribute::IMPLICIT_ADD)) {
                continue;
            }
            auto relation = GetPackageRelation(pkg.fullPackageName, fullPackageName);
            if ((package->accessible == AccessLevel::PROTECTED && relation == PackageRelation::NONE) ||
                (package->accessible == AccessLevel::INTERNAL && relation != PackageRelation::CHILD &&
                    !IsSuperPackage(pkg.fullPackageName, fullPackageName))) {
                // Report error when the package is not accessible.
                diag.DiagnoseRefactor(DiagKindRefactor::package_accessibility, range, fullPackageName,
                    GetAccessLevelStr(*package), RelationToString(relation), pkg.fullPackageName);
            } else if (import->IsReExport() && !pkg.isMacroPackage && package->isMacroPackage) {
                diag.DiagnoseRefactor(DiagKindRefactor::package_cannot_export_macro_package, range);
                continue;
            } else if (import->IsImportAll()) {
                continue;
            } else if (cjoManager->IsImportPackage(*import)) {
                if (import->IsReExport()) {
                    diag.DiagnoseRefactor(DiagKindRefactor::package_re_export_package_name, range, fullPackageName,
                        GetAccessLevelStr(*import));
                }
                continue;
            }
            // Report error when the imported name cannot be found.
            auto decls = cjoManager->GetPackageMembersByName(package->fullPackageName, import->content.identifier);
            Utils::EraseIf(decls, [relation](auto it) { return !Modules::IsVisible(*it, relation); });
            if (decls.empty()) {
                auto& im = import->content;
                auto nameRange = MakeRange(im.prefixPoses.front(), im.identifier.End());
                diag.DiagnoseRefactor(
                    DiagKindRefactor::package_decl_not_find_in_package, nameRange, im.identifier, fullPackageName);
            }
        }
    }
}

void ImportManager::ProcessMacroImports(AST::Package& pkg)
{
    // Macro import declarations are persisted to cjo as structured MacroImportList nodes
    // (with @When kept as Annotation condExpr AST). Here we iterate them directly over
    // every macro package in the dependency closure, evaluate each @When in the current
    // downstream target via the normal-import @When path (EvalMacroImportWhenCond), and
    // inject the survivors' symbols / record their importStr for .macrocall generation.
    //
    // Semantic rule 2: macro import takes effect for downstream packages regardless of
    // whether they are macro packages. Rule 1 (a package's own macro imports are not
    // imported for itself, and @When is not processed in the macro package) is upheld by
    // CollectTransitiveMacroDeps pre-seeding `visited` with pkgName itself, so the closure
    // contains only upstream dependencies and pkg's own macro imports are never fed back.

    // Clear the two global fields per downstream package: they are ImportManager-wide
    // (not per-package), and with @When the same macro package's imports evaluate
    // differently for different downstream targets, so not clearing would cross-pollute.
    macroImportStrings.clear();
    macroImportPkgsFromMacroDeps.clear();
    curPackage = &pkg;

    std::unordered_set<std::string> visited;
    auto macroDeps = CollectTransitiveMacroDeps(pkg.fullPackageName, visited);

    // Structured scheme: each macro package in the closure carries a macroImports
    // vector (structured nodes with @When kept as Annotation condExpr AST). Iterate
    // them directly — no token concatenation, no re-parse. Dedup of identical imports
    // is handled below at injection time (macroImportPkgsFromMacroDeps per-package
    // load set, macroImportStrings std::find for .macrocall dedup). There is no
    // dedicated macro-import conflict check: symbols are injected into the same
    // fileImportedDeclsMap as ordinary imports, so clashes surface at use time as
    // ordinary ambiguous-use diagnostics (lazy, file-scoped) — identical to how
    // ordinary import collisions are reported.
    bool anyMacroImport = false;
    for (const auto& macroDepPkgName : macroDeps) {
        auto pd = cjoManager->GetPackage(macroDepPkgName);
        if (pd && pd->isMacroPackage && !pd->macroImports.empty()) {
            anyMacroImport = true;
            break;
        }
    }
    if (!anyMacroImport) {
        return;
    }

    // Semantic rule 2 file-level visibility: macro import is equivalent to a downstream
    // ordinary private import, which is file-scoped (see AddImportedDeclsForSourcePackage:
    // non-reexport injects to fileImportedDeclsMap[fileHash]). Collect the set of
    // downstream files that actually import a macro package in the dependency closure —
    // the macro expansion consumers are a subset (calling a macro ⟹ importing the macro
    // package), so this never misses a real consumer. See design 4.5.4/4.5.5.
    std::unordered_set<std::string> macroDepSet(macroDeps.begin(), macroDeps.end());
    std::unordered_set<uint64_t> macroConsumerFileHashes;
    for (const auto& file : pkg.files) {
        for (const auto& import : file->imports) {
            auto fullPkgName = cjoManager->GetPackageNameByImport(*import);
            if (macroDepSet.find(fullPkgName) != macroDepSet.end()) {
                macroConsumerFileHashes.insert(file->fileHash);
                break; // 该文件命中即可，无需重复收集
            }
        }
    }

    for (const auto& macroDepPkgName : macroDeps) {
        auto pd = cjoManager->GetPackage(macroDepPkgName);
        if (!pd || !pd->isMacroPackage || pd->macroImports.empty()) {
            continue;
        }
        for (auto& stmt : pd->macroImports) {
            for (auto& spec : stmt->items) {
            // Evaluate @When in the downstream context via the normal-import path. False
            // conditions are erased from the node's annotations; EvalMacroImportWhenCond
            // returns whether the import survives.
            if (condComp == nullptr) {
                // Defensive: CompilerInstance should inject condComp before BuildIndex. If
                // it is missing, @When is not evaluated and all macro imports survive — warn
                // once per import so the misconfiguration is visible rather than silently
                // importing everything. (Only emitted in the abnormal, un-injected path.)
                diag.DiagnoseRefactor(DiagKindRefactor::macro_import_condcomp_missing,
                    spec->begin == DEFAULT_POSITION ? (pkg.files.empty() ? DEFAULT_POSITION : pkg.files[0]->begin)
                                                    : spec->begin,
                    pkg.fullPackageName);
            } else if (!condComp->EvalMacroImportWhenCond(*spec)) {
                continue; // @When false in this target → skip
            }
            std::string macroImportStr = spec->content.ToString();
            auto macroImportPkg = GetPackageNameFromMacroImport(macroImportStr);
            if (macroImportPkg.empty()) {
                continue;
            }

            bool pkgAlreadyLoaded = macroImportPkgsFromMacroDeps.find(macroImportPkg) !=
                                    macroImportPkgsFromMacroDeps.end();
            if (!pkgAlreadyLoaded) {
                macroImportPkgsFromMacroDeps.insert(macroImportPkg);

                auto existingPkg = cjoManager->GetPackage(macroImportPkg);
                if (existingPkg == nullptr) {
                    std::string cjoPath = FileUtil::FindSerializationFile(
                        macroImportPkg, SERIALIZED_FILE_EXTENSION, GetSearchPath());
                    if (!cjoPath.empty()) {
                        auto loadedPkg = LoadPackageFromCjo(macroImportPkg, cjoPath);
                        if (loadedPkg != nullptr) {
                            loadedPkg->EnableAttr(AST::Attribute::IMPORTED);
                            cjoManager->SetOnlyUsedByMacro(macroImportPkg, false);
                            SaveDepPkgCjoPath(macroImportPkg, cjoPath);
                            for (auto depStdPkg : loadedPkg->GetAllDependentStdPkgs()) {
                                if (cjoFilePaths.find(depStdPkg) == cjoFilePaths.end()) {
                                    std::string depCjoPath = FileUtil::FindSerializationFile(
                                        depStdPkg, SERIALIZED_FILE_EXTENSION, GetSearchPath());
                                    HandleStdPackage(depStdPkg, depCjoPath, true);
                                }
                            }
                        }
                    }
                } else {
                    existingPkg->EnableAttr(AST::Attribute::IMPORTED);
                    cjoManager->SetOnlyUsedByMacro(macroImportPkg, false);
                    auto members = cjoManager->GetPackageMembers(macroImportPkg);
                    if (members.empty()) {
                        cjoManager->LoadPackageDeclsAndRefs(macroImportPkg);
                        cjoManager->AddPackageDeclMap(macroImportPkg);
                    }
                }

                // Symbols added to fileImportedDeclsMap for the consumer files of the
                // current downstream package (including downstream macro packages, per
                // semantic rule 2). File-level injection upholds the private-import scoping.
                AddPackageSymbolsToImportMap(macroImportPkg, macroConsumerFileHashes);
            }

            // Record only survivors, for .macrocall generation. GenerateMacroCallFile emits
            // a bare `import xxx` (no @When, already evaluated).
            if (std::find(macroImportStrings.begin(), macroImportStrings.end(), macroImportStr) ==
                macroImportStrings.end()) {
                macroImportStrings.push_back(macroImportStr);
            }
            // No dedicated macro-import conflict check here. Symbols are injected into
            // fileImportedDeclsMap[fileHash] (same table as ordinary imports), so a clash
            // (same simple name from two macro packages pointing at different decls) shows
            // up at use time as an ordinary ambiguous-use diagnostic in Sema — same
            // lazy, file-scoped mechanism as ordinary import collisions.
        }
        }
    }
}

std::vector<std::string> ImportManager::CollectTransitiveMacroDeps(
    const std::string& pkgName, std::unordered_set<std::string>& visited) const
{
    // Pre-seed visited with pkgName itself to uphold rule 1 after the entry guard was
    // removed: without this, a declaration-chain cycle (A macro import B, B macro import A)
    // would let pkg re-enter the result via the cycle and be processed, polluting pkg's
    // own importedDeclsMap with its own macroImports. Pre-seeding blocks any path that
    // loops back to pkg.
    visited.insert(pkgName);

    std::vector<std::string> result;
    for (auto& pd : GetCurImportedPackages(pkgName)) {
        if (!pd->srcPackage || pd->srcPackage->TestAttr(Attribute::TOOL_ADD)) {
            continue;
        }
        const auto& depName = pd->srcPackage->fullPackageName;
        if (!visited.insert(depName).second) {
            continue; // avoid cycles in the dependency graph + block loops back to pkg
        }
        if (pd->srcPackage->isMacroPackage) {
            result.push_back(depName);
            // Rule 3.3 (lifter semantics): macro import declarations also form a
            // propagation chain. When a macro package M2 declares `macro import X`
            // and X is itself a macro package, X (and X's own macro-import chain)
            // is lifted to M2 and must propagate through M2 to downstream packages.
            // This covers the case where an outer macro's expansion result references
            // a macro in an indirectly-imported macro package, without M2 directly
            // invoking that macro (which would require a normal import).
            CollectMacroImportChain(*pd->srcPackage, visited, result);
        }
        // recurse into transitive dependencies (along normal import load chain)
        auto sub = CollectTransitiveMacroDeps(depName, visited);
        result.insert(result.end(), sub.begin(), sub.end());
    }
    return result;
}

void ImportManager::CollectMacroImportChain(const AST::Package& macroPkg,
    std::unordered_set<std::string>& visited, std::vector<std::string>& result) const
{
    // Walk the macroImport declaration chain of a macro package: for each declared
    // import, if the target package is itself a macro package, lift it (and recurse
    // into its chain) so its macroImports propagate downstream through this package.
    // Walk the cjo-persisted `macroImports` (structured MacroImportList nodes): this
    // runs at downstream compile time where the macro package was loaded from its cjo,
    // and macroImportPkgNames (the Parse-derived name list) is not persisted, so it is
    // empty on loaded packages and cannot be used here.
    auto targetName = [](const AST::MacroImportSpec& spec) {
        std::string name;
        for (const auto& p : spec.content.prefixPaths) {
            name += p + ".";
        }
        if (spec.content.kind == AST::ImportKind::IMPORT_ALL) {
            if (!name.empty()) {
                name.pop_back(); // "a.b.*" targets package "a.b"
            }
        } else {
            name += spec.content.identifier.Val();
        }
        return name;
    };
    for (const auto& stmt : macroPkg.macroImports) {
        for (const auto& spec : stmt->items) {
            auto targetPkgName = targetName(*spec);
            if (targetPkgName.empty() || targetPkgName == macroPkg.fullPackageName) {
                continue;
            }
            auto targetPkg = cjoManager->GetPackage(targetPkgName);
            if (targetPkg == nullptr) {
                // The chain target is not a direct import of the downstream package, so its
                // cjo may not have been loaded yet (GetPackage only looks up registered
                // packages). Load its header on demand from the import search paths — this
                // also registers it, letting ProcessMacroImports inject its macroImports.
                auto cjoPath = cjoManager->GetPackageCjoPath(targetPkgName);
                if (cjoPath.empty() || !cjoManager->LoadPackageHeader(targetPkgName, cjoPath)) {
                    continue;
                }
                targetPkg = cjoManager->GetPackage(targetPkgName);
            }
            if (!targetPkg || !targetPkg->isMacroPackage) {
                continue; // only macro packages carry a macro-import declaration chain
            }
            if (!visited.insert(targetPkgName).second) {
                continue; // avoid cycles in the declaration chain
            }
            result.push_back(targetPkgName);
            CollectMacroImportChain(*targetPkg, visited, result);
        }
    }
}

void ImportManager::CheckRedefinition(const AST::Package& pkg)
{
    std::unordered_map<std::string, Range> pkgImportNamePos;
    std::unordered_map<Ptr<File>, std::unordered_map<std::string, Range>> fileImportNamePos;
    for (auto& file : pkg.files) {
        for (auto& import : file->imports) {
            if (import->IsImportMulti() || import->IsImportAll()) {
                continue;
            }
            auto& posMap = import->IsReExport() ? pkgImportNamePos : fileImportNamePos[file.get()];
            auto& im = import->content;
            auto& name = im.kind == ImportKind::IMPORT_SINGLE ? im.identifier : im.aliasName;
            auto decls = GetImportedDeclsByName(*file, name);
            if (decls.empty()) {
                continue; // Was failed to import package member.
            }
            auto range = im.kind == ImportKind::IMPORT_SINGLE ? MakeRange(im.identifier) : MakeRange(im.aliasName);
            auto [it, success] = posMap.emplace(name, range);
            if (import->IsReExport() && success) {
                std::tie(it, success) = fileImportNamePos[file.get()].emplace(name, range);
            }
            auto& pkgMembers = cjoManager->GetPackageMembersByName(pkg.fullPackageName, name);
            std::vector<Ptr<Decl>> sourceDecls;
            std::copy_if(pkgMembers.cbegin(), pkgMembers.cend(), std::back_inserter(sourceDecls),
                [&pkg](auto it1) { return it1->curFile->curPackage == &pkg; });
            if (!IsAllFuncDecl(sourceDecls)) {
                WarnUselessImport(diag, range, *(*sourceDecls.begin()));
            } else if (!success) {
                WarnConflictImport(diag, name, range, it->second);
            }
        }
    }
}

std::string ImportManager::GeneratePkgDepInfo(const Package& pkg, bool exportCJO) const
{
    auto isStd = [](const std::string& fullPackageName) { return STANDARD_LIBS.count(fullPackageName) > 0; };
    std::map<std::string, DependencyInfoItem> dependencies;
    std::set<std::string> refSet;
    bool isProduct{true};
    for (auto& file : pkg.files) {
        for (auto& import : file->imports) {
            if (import->IsImportMulti() || import->TestAttr(Attribute::IMPLICIT_ADD)) {
                continue;
            }
            auto names = cjoManager->GetPossibleCjoNames(*import);
            CJC_ASSERT(!names.empty());
            std::string longName = FileUtil::ToPackageName(names.front());
            std::string shortName = FileUtil::ToPackageName(names.back());
            auto iter = dependencies.find(longName);
            if (iter == dependencies.cend()) {
                DependencyInfoItem dependInfo{"", "", false, {}};
                if (isStd(longName)) {
                    dependInfo.package = longName;
                    dependInfo.isStd = true;
                } else if (isStd(shortName)) {
                    dependInfo.package = shortName;
                    dependInfo.isStd = true;
                } else {
                    dependInfo.package = shortName;
                    dependInfo.orPackage = longName == shortName ? "" : longName;
                    dependInfo.isStd = isStd(shortName);
                }
                dependencies.emplace(longName, dependInfo);
            }
            dependencies.at(longName).imports.emplace(import.get());
        }
        if (file->feature) {
            for (auto& feature : file->feature->featuresSet->content) {
                refSet.insert(feature.ToString());
            }
            for (auto& anno : file->feature->annotations) {
                if (anno->kind == AnnotationKind::NON_PRODUCT) {
                    isProduct = false;
                }
            }
        }
    }
    std::ostringstream out;
    out << "{" << "\"package\":\"" << Jsonfy(pkg.fullPackageName) << "\","
        << "\"isMacro\":" << Jsonfy(pkg.isMacroPackage) << ","
        << "\"accessLevel\":\"" << Jsonfy(pkg.accessible) << "\","
        << "\"dependencies\":" << Jsonfy(dependencies, exportCJO) << ","
        << "\"std-dependencies\":" << Jsonfy(GetStdDependency(dependencies, pkg, exportCJO, *this, *cjoManager)) << ","
        << "\"features\":" << Jsonfy(refSet) << ",";
    // Macro import dependencies (structured scheme): for a macro package, report the package
    // names derived from its macro import declarations (Parse-derived, full set without
    // @When evaluation — @When is a downstream condition; the macro package may be depended
    // on by downstream packages with different targets, so cjpm cannot evaluate it here;
    // over-reporting a no-op package is safe, under-reporting deadlocks the build). cjpm
    // reads this field and adds these names to the `requires` of downstream packages that
    // depend on this macro package, so the dependency graph is correct on first build (no
    // cjo yet). The actual @When evaluation stays in ProcessMacroImports at downstream
    // compile time.
    if (pkg.isMacroPackage && !pkg.macroImportPkgNames.empty()) {
        std::set<std::string> macroImportPkgSet(pkg.macroImportPkgNames.begin(),
            pkg.macroImportPkgNames.end()); // dedup
        out << "\"macroImports\":" << Jsonfy(macroImportPkgSet) << ",";
    }
    out << "\"product\":" << Jsonfy(isProduct) << "}";
    return out.str();
}

std::vector<std::string> ImportManager::GeneratePkgDepInfoByCjo(const std::string& cjoPath)
{
    std::string packageDepInfo = cjoManager->GetPackageDepInfo(cjoPath);
    if (packageDepInfo.empty()) {
        return {};
    }
    // Note: wrap with std::vector to adapt current interface.
    return {packageDepInfo};
}

std::vector<std::string> ImportManager::GeneratePkgDepInfo(const std::vector<Ptr<Package>>& packages) const
{
    std::vector<std::string> res;
    for (auto& pkg : packages) {
        CJC_NULLPTR_CHECK(pkg);
        std::string depInfo = GeneratePkgDepInfo(*pkg, false);
        res.emplace_back(depInfo);
    }
    return res;
}

std::set<std::string> ImportManager::CollectDirectDepPkg(const Package& package) const
{
    std::set<std::string> depPkgs;
    for (auto& file : package.files) {
        for (auto& import : file->imports) {
            if (import->IsImportMulti()) {
                continue;
            }
            // Collect all possible package names.
            for (auto name : cjoManager->GetPossibleCjoNames(*import)) {
                depPkgs.emplace(name);
            }
        }
    }
    return depPkgs;
}

namespace {

void ValidateFileFeatureSpec(DiagnosticEngine& diag, const Package& pkg, std::unordered_map<std::string, bool>& refMap,
    Ptr<File>& refFile, bool& anno)
{
    size_t refSize = 0;
    std::unordered_map<std::string, Range> rangeMap;
    for (auto& file : pkg.files) {
        CJC_NULLPTR_CHECK(file);
        if (file->feature != nullptr) {
            for (auto& feature : file->feature->featuresSet->content) {
                std::string ftrStr = feature.ToString();
                auto prevPos = rangeMap.find(ftrStr);
                bool contains = prevPos != rangeMap.end();
                Range current = MakeRange(feature.begin, feature.end);
                if (contains) {
                    WarnRepeatedFeatureName(diag, ftrStr, current, prevPos->second);
                } else {
                    rangeMap.emplace(ftrStr, current);
                }
            }
            if (!file->feature->annotations.empty()) {
                anno = true;
            }
            if (!refFile || refSize < rangeMap.size()) {
                refSize = rangeMap.size();
                refFile = file;
            }
            rangeMap.clear();
        }
    }
    CJC_NULLPTR_CHECK(refFile.get());
    for (auto& ftr : refFile->feature->featuresSet->content) {
        refMap.emplace(ftr.ToString(), false);
    }
}

void CollectInvalidFeatureFiles(const Package& pkg, std::vector<Ptr<File>>& invalidFeatures,
    std::unordered_map<std::string, bool>& refMap, bool hasAnno)
{
    for (auto& file : pkg.files) {
        if (!file->feature || (hasAnno && file->feature->annotations.empty())) {
            invalidFeatures.emplace_back(file);
            continue;
        }
        bool hasInvalidName{false};
        for (auto& feature : file->feature->featuresSet->content) {
            std::string ftrStr = feature.ToString();
            auto pair = refMap.find(ftrStr);
            if (pair != refMap.end()) {
                pair->second = true;
            } else {
                hasInvalidName = true;
                invalidFeatures.emplace_back(file);
                break;
            }
        }
        if (!hasInvalidName) {
            for (auto& pair : refMap) {
                if (!pair.second) {
                    invalidFeatures.emplace_back(file);
                    break;
                }
            }
        }
        std::for_each(refMap.begin(), refMap.end(), [](auto& pair) { pair.second = false; });
    }
}

static void CheckPackageFeatureSpec(DiagnosticEngine& diag, const Package& pkg)
{
    std::unordered_map<std::string, bool> refMap;
    std::vector<Ptr<File>> invalidFeatures;
    Ptr<File> refFile;
    bool hasAnno{false};

    ValidateFileFeatureSpec(diag, pkg, refMap, refFile, hasAnno);
    CollectInvalidFeatureFiles(pkg, invalidFeatures, refMap, hasAnno);

    if (!invalidFeatures.empty()) {
        uint8_t counter = 0;
        for (auto& file : invalidFeatures) {
            if (counter > 1) {
                return;
            }
            if (!file->feature) {
                DiagForNullPackageFeature(diag, MakeRange(file->begin, file->end), refFile->feature);
            } else {
                auto& feature = file->feature;
                DiagForDifferentPackageFeatureConsistency(diag, feature, refFile->feature, hasAnno);
            }
            counter++;
        }
    }
}

static bool IsRootPackage(const PackageSpec& p)
{
    if (p.hasDoubleColon && p.prefixPaths.size() == 1) {
        return true;
    }
    return p.prefixPaths.empty();
}

static void CheckPackageSpecsIdentical(DiagnosticEngine& diag, const Package& pkg)
{
    // the map of the key is declared package name with it's modifier (default is 'public'),
    // and the value is a pair whose first element is package name
    // position and second indicates whether current package declaration is an implicit default package.
    std::map<std::pair<std::string, std::string>, std::pair<Position, bool>> packageNamePosMap;
    for (auto& file : pkg.files) {
        CJC_NULLPTR_CHECK(file);
        if ((file->package == nullptr || file->begin == file->end) &&
            !Utils::InKeys(std::make_pair(DEFAULT_PACKAGE_NAME, std::string("public")), packageNamePosMap)) {
            packageNamePosMap[std::make_pair(DEFAULT_PACKAGE_NAME, "public")] = {file->begin, false};
        } else if (file->package != nullptr) {
            if (IsRootPackage(*file->package) && !file->package->TestAttr(Attribute::PUBLIC)) {
                DiagRootPackageModifier(diag, *file->package);
            }
            auto fullPackageName = file->package->GetPackageName();
            auto keyPair = std::make_pair(std::move(fullPackageName), GetAccessLevelStr(*file->package));

            if (!Utils::InKeys(keyPair, packageNamePosMap)) {
                auto position = file->package->prefixPaths.empty() ? file->package->packageName.Begin()
                                                                   : file->package->prefixPoses.front();
                packageNamePosMap[keyPair] = {position, true};
            }
        }
    }
    if (packageNamePosMap.size() > 1) {
        DiagForDifferentPackageNames(diag, packageNamePosMap);
    }
}
} // namespace

bool ImportManager::BuildIndex(
    const std::string& cangjieModules, const GlobalOptions& globalOptions, std::vector<Ptr<Package>>& packages)
{
    Utils::ProfileRecorder recorder("ImportPackages", "BuildIndex");
    bool incrBuildIndex = HasBuildIndex();
    CJC_ASSERT(!packages.empty());
    if (!incrBuildIndex) {
        cjoManager->UpdateSearchPath(cangjieModules);
    } else {
        ClearCachesForRebuild();
    }
    Utils::ProfileRecorder::Start("BuildIndex", "AddImplicitImports");
    for (auto pkg : packages) {
        if (pkg->HasFtrDirective()) {
            CheckPackageFeatureSpec(diag, *pkg);
        }
        CheckPackageSpecsIdentical(diag, *pkg);
        if (opts.compileTestsOnly) {
            pkg->fullPackageName = pkg->fullPackageName + SourceManager::testPkgSuffix;
        }
        AddImplicitImports(*pkg, globalOptions);
        cjoManager->AddSourcePackage(*pkg);
    }
    Utils::ProfileRecorder::Stop("BuildIndex", "AddImplicitImports");
    ResolveImportedPackages(packages);
    ClearPackageCjoCache();

    Utils::ProfileRecorder::Start("BuildIndex", "AddDependencies");
    for (auto pkg : packages) {
        dependencyGraph->AddDependenciesForPackage(*pkg);
    }
    Utils::ProfileRecorder::Stop("BuildIndex", "AddDependencies");

    Utils::ProfileRecorder::Start("BuildIndex", "AddImportedDeclsForSourcePackage");
    for (auto pkg : packages) {
        AddImportedDeclsForSourcePackage(*pkg);
    }
    Utils::ProfileRecorder::Stop("BuildIndex", "AddImportedDeclsForSourcePackage");

    Utils::ProfileRecorder::Start("BuildIndex", "ProcessMacroImports");
    for (auto pkg : packages) {
        ProcessMacroImports(*pkg);
    }
    Utils::ProfileRecorder::Stop("BuildIndex", "ProcessMacroImports");

    Utils::ProfileRecorder::Start("BuildIndex", "ResolveImports");
    for (auto pkg : packages) {
        ResolveImports(*pkg);
    }
    for (auto& pd : GetAllImportedPackages()) {
        if (pd->srcPackage->hasSourceImportedDecl) {
            packages.push_back(pd->srcPackage);
        }
    }
    Utils::ProfileRecorder::Stop("BuildIndex", "ResolveImports");
    curPackage = packages.front();
    // Set BuildIndex flag
    cjoManager->SetHasBuildIndex(true);
    return true;
}

Ptr<Package> ImportManager::LoadPackageFromCjo(const std::string& fullPkgName, const std::string& cjoPath) const
{
    if (!cjoManager->LoadPackageHeader(fullPkgName, cjoPath)) {
        return nullptr;
    }

    std::function<bool(const Package&)> loadDependentPkgHeader = [&loadDependentPkgHeader, this](const Package& pkg) {
        for (auto& file : std::as_const(pkg.files)) {
            for (auto& import : std::as_const(file->imports)) {
                if (import->IsImportMulti()) {
                    continue;
                }
                auto [fullDepPkgName, cjoPath1] = cjoManager->GetPackageCjo(*import);
                if (auto depPkg = cjoManager->GetPackage(fullDepPkgName); depPkg) {
                    continue;
                }
                if (!cjoManager->LoadPackageHeader(fullDepPkgName, cjoPath1)) {
                    return false;
                }
                auto depPkg = cjoManager->GetPackage(fullDepPkgName);
                CJC_NULLPTR_CHECK(depPkg);
                if (!loadDependentPkgHeader(*depPkg)) {
                    return false;
                }
            }
        }
        return true;
    };

    auto package = cjoManager->GetPackage(fullPkgName);
    CJC_NULLPTR_CHECK(package);
    if (!loadDependentPkgHeader(*package)) {
        return nullptr;
    }

    cjoManager->LoadPackageDeclsOnDemand({package}, true);
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
    cjoManager->ClearVisitedPkgs();
#endif
    cjoManager->AddPackageDeclMap(fullPkgName);
    return package;
}

// Use for macro debug: update the debug file info
void ImportManager::UpdateFileNodeImportInfo(Package& package, const File& file, OwnedPtr<File>& newFile)
{
    CJC_ASSERT(newFile);
    newFile->curPackage = &package;
    // Replace the current file.
    CJC_ASSERT(file.fileHash != newFile->fileHash);
    // Copy the compiler added implicitly imports.
    for (auto& import : file.imports) {
        if (import->TestAttr(Attribute::IMPLICIT_ADD)) {
            (void)newFile->imports.emplace_back(ASTCloner::Clone(import.get()));
        }
    }
    // Remove stale importSpec->packageName entries before resolving new imports; old ImportSpec pointers
    // may be reused after the file is replaced, causing GetPackageNameByImport to return wrong package names.
    cjoManager->RemoveImportedPackageNames(file.imports);
    for (auto& import : newFile->imports) {
        if (import->IsImportMulti()) {
            continue;
        }
        auto [_, cjoPath] = cjoManager->GetPackageCjo(*import);
        if (cjoPath.empty()) {
            InternalError("Failed to resolve imports for macro file");
        }
        if (!import->IsImportAll() && !cjoManager->IsImportPackage(*import)) {
            import->content.isDecl = true;
        }
    }
    // MacroExpansion will never change import declarations, just copy file import map for new file.
    // [Fix] Preserve old hash entry, since macro-expanded nodes may still reference the original file (curFile)
    auto found = fileImportedDeclsMap.find(file.fileHash);
    if (found != fileImportedDeclsMap.end()) {
        // Copy rather than move, preserving the old entry for macro-expanded nodes still referencing the old file
        (void)fileImportedDeclsMap.emplace(newFile->fileHash, found->second);
        // Do not erase old entry: fileImportedDeclsMap.erase(file.fileHash);
    }
    auto index = file.indexOfPackage;
    newFile->indexOfPackage = index;
    RemoveDeclsImportedByImports(package.fullPackageName, file.imports);
    package.files[index] = std::move(newFile);
    // Update declsImportedByNodeMap from newFile's imports (stale entries for the old file were removed above).
    AddImportedDeclsForFile(package, *package.files[index]);
}

void ImportManager::SetPackageBchirCache(const std::string& fullPackageName, const std::vector<uint8_t>& bchirData)
{
    if (fullPackageName.empty() || bchirData.empty()) {
        return;
    }
    bchirFileCacheMap[fullPackageName] = bchirData;
}

const std::vector<uint8_t>* ImportManager::GetBchirCache(const std::string& fullPackageName)
{
    auto it = bchirFileCacheMap.find(fullPackageName);
    if (it != bchirFileCacheMap.end()) {
        return &(it->second);
    }
    return nullptr;
}

Ptr<PackageDecl> ImportManager::GetPackageDecl(const std::string& fullPackageName) const
{
    return cjoManager->GetPackageDecl(fullPackageName);
}

Ptr<Package> ImportManager::GetPackage(const std::string& fullPackageName) const
{
    return cjoManager->GetPackage(fullPackageName);
}

void ImportManager::AddUsedMacroDecls(const Ptr<const AST::File> file, Ptr<const AST::Decl> decl)
{
    auto& usedDeclMap = fileUsedMacroDeclsMap[file->indexOfPackage];

    auto& declSet = usedDeclMap[decl->fullPackageName];
    declSet.emplace(decl);
}

std::map<std::string, std::set<Ptr<const AST::Decl>, AST::CmpNodeByPos>>& ImportManager::GetUsedMacroDecls(
    const AST::File& file)
{
    return fileUsedMacroDeclsMap[file.indexOfPackage];
}

void ImportManager::RemoveDeclsImportedByImports(
    const std::string& fullPackageName, const std::vector<OwnedPtr<ImportSpec>>& imports)
{
    if (imports.empty()) {
        return;
    }
    auto found = declsImportedByNodeMap.find(fullPackageName);
    if (found == declsImportedByNodeMap.end()) {
        return;
    }
    std::unordered_set<Ptr<const ImportSpec>> staleImports(imports.begin(), imports.end());
    for (auto& [decl, importSpecs] : found->second) {
        (void)decl;
        importSpecs.erase(std::remove_if(importSpecs.begin(), importSpecs.end(),
                              [&staleImports](Ptr<const ImportSpec> import) { return staleImports.count(import) > 0; }),
            importSpecs.end());
    }
}

void ImportManager::AddImportedDeclsForSourcePackage(const AST::Package& pkg)
{
    for (auto& file : pkg.files) {
        AddImportedDeclsForFile(pkg, *file);
    }
}

void ImportManager::AddImportedDeclsForFile(const Package& pkg, const File& file)
{
    for (auto& import : file.imports) {
        auto& declMap = import->IsReExport() ? importedDeclsMap : fileImportedDeclsMap[file.fileHash];
        CJC_NULLPTR_CHECK(import);
        if (import->IsImportMulti()) {
            continue;
        }
        auto fullPackageName = cjoManager->GetPackageNameByImport(*import);
        auto pkgDecl = cjoManager->GetPackageDecl(fullPackageName);
        if (fullPackageName.empty() || !pkgDecl) {
            continue; // Load package failed.
        }
        auto relation = GetPackageRelation(pkg.fullPackageName, fullPackageName);
        auto importLevel = GetAccessLevel(*import);
        if (import->content.kind == ImportKind::IMPORT_ALL) {
            auto members = cjoManager->GetPackageMembers(fullPackageName);
            for (auto& member : std::as_const(members)) {
                auto& targetMap = declMap[member.first];
                auto visibleDecls = GetVisibleDeclToMap(member.second, importLevel, relation);
                std::for_each(visibleDecls.cbegin(), visibleDecls.cend(), [this, &import, &pkg](auto it) {
                    declsImportedByNodeMap[pkg.fullPackageName][it].emplace_back(import.get());
                });
                targetMap.merge(visibleDecls);
                std::for_each(member.second.begin(), member.second.end(), [this, member](auto decl) {
                    if (decl->identifier != member.first) {
                        declToTypeAlias[decl].emplace(decl->identifier);
                    }
                });
            }
            continue;
        }
        if (cjoManager->IsImportPackage(*import)) {
            auto name = import->content.kind == ImportKind::IMPORT_SINGLE
                ? Utils::SplitQualifiedName(fullPackageName, true).back()
                : import->content.aliasName.Val();
            declMap[name].emplace(pkgDecl);
            declsImportedByNodeMap[pkg.fullPackageName][pkgDecl].emplace_back(import.get());
        } else {
            auto members = cjoManager->GetPackageMembersByName(fullPackageName, import->content.identifier);
            const auto& name = import->content.kind == ImportKind::IMPORT_SINGLE ? import->content.identifier.Val()
                                                                                 : import->content.aliasName.Val();
            auto& targetMap = declMap[name];
            auto visibleDecls = GetVisibleDeclToMap(members, importLevel, relation);
            for (auto member : members) {
                if (auto tad = DynamicCast<TypeAliasDecl>(member); tad && tad->type) {
                    auto originDecl = tad->type->GetTarget();
                    declToTypeAlias[originDecl].emplace(name);
                } else if (member->identifier != name) {
                    declToTypeAlias[member].emplace(name);
                    declToTypeAlias[member].emplace(member->identifier);
                }
            }
            std::for_each(visibleDecls.cbegin(), visibleDecls.cend(), [this, &import, &pkg](auto it) {
                declsImportedByNodeMap[pkg.fullPackageName][it].emplace_back(import.get());
            });
            targetMap.merge(visibleDecls);
            import->content.isDecl = true;
        }
    }
}

std::vector<Ptr<Decl>> ImportManager::GetImportedDeclsByName(const File& file, const std::string& name) const
{
    OrderedDeclSet ret;
    auto found = importedDeclsMap.find(name);
    if (found != importedDeclsMap.cend()) {
        ret.insert(found->second.cbegin(), found->second.cend());
    }
    auto fileIter = fileImportedDeclsMap.find(file.fileHash);
    if (fileIter != fileImportedDeclsMap.cend()) {
        auto iter = fileIter->second.find(name);
        if (iter != fileIter->second.cend()) {
            ret.insert(iter->second.cbegin(), iter->second.cend());
        }
    }
    return Utils::SetToVec<Ptr<Decl>>(ret);
}

std::vector<std::pair<std::string, std::vector<Ptr<Decl>>>> ImportManager::GetImportedDecls(const File& file) const
{
    // pre-allocate space for result
    size_t totalSize = importedDeclsMap.size();
    auto iter = fileImportedDeclsMap.find(file.fileHash);
    if (iter != fileImportedDeclsMap.cend()) {
        totalSize += iter->second.size();
    }
    std::vector<std::pair<std::string, std::vector<Ptr<Decl>>>> res;
    res.reserve(totalSize);
    // construct result directly, avoid temporary variables
    for (const auto& [name, declSet] : importedDeclsMap) {
        std::vector<Ptr<Decl>> decls(declSet.cbegin(), declSet.cend());
        res.emplace_back(name, std::move(decls));
    }

    if (iter != fileImportedDeclsMap.cend()) {
        for (const auto& [name, declSet] : iter->second) {
            std::vector<Ptr<Decl>> decls(declSet.cbegin(), declSet.cend());
            res.emplace_back(name, std::move(decls));
        }
    }
    return res;
}

std::vector<Ptr<Decl>> ImportManager::GetAllImportedDecls(const File& file) const
{
    OrderedDeclSet declSet;
    for (const auto& [_, decls] : importedDeclsMap) {
        declSet.insert(decls.cbegin(), decls.cend());
    }
    auto iter = fileImportedDeclsMap.find(file.fileHash);
    if (iter != fileImportedDeclsMap.cend()) {
        for (const auto& [_, decls] : iter->second) {
            declSet.insert(decls.cbegin(), decls.cend());
        }
    }
    std::vector<Ptr<Decl>> res;
    res.reserve(declSet.size());
    res.assign(declSet.cbegin(), declSet.cend());
    return res;
}

const OrderedDeclSet& ImportManager::GetPackageMembersByName(const Package& package, const std::string& name) const
{
    return cjoManager->GetPackageMembersByName(package.fullPackageName, name);
}

// For LSP
std::map<std::string, AST::OrderedDeclSet> ImportManager::GetPackageMembers(
    const std::string& srcFullPackageName, const std::string& targetFullPackageName) const
{
    if (srcFullPackageName == targetFullPackageName) {
        return {};
    }
    auto relation = Modules::GetPackageRelation(srcFullPackageName, targetFullPackageName);
    auto members = cjoManager->GetPackageMembers(targetFullPackageName);
    std::map<std::string, AST::OrderedDeclSet> res;
    for (auto& [name, decls] : members) {
        AST::OrderedDeclSet visibleDecls;
        for (auto it : decls) {
            if (Modules::IsVisible(*it, relation)) {
                visibleDecls.emplace(it.get());
            }
        }
        if (!visibleDecls.empty()) {
            res[name] = std::move(visibleDecls);
        }
    }
    return res;
}

std::pair<Ptr<PackageDecl>, bool> ImportManager::GetImportedPackageDecl(
    Ptr<const Node> node, const std::string& packageName) const
{
    CJC_NULLPTR_CHECK(node->curFile);
    auto decls = GetImportedDeclsByName(*node->curFile, packageName);
    Ptr<PackageDecl> ret = nullptr;
    for (auto decl : decls) {
        if (auto pd = DynamicCast<PackageDecl>(decl)) {
            if (ret == nullptr) {
                ret = pd;
            } else {
                return {ret, true};
            }
        }
    }
    return {ret, false};
}

std::vector<Ptr<PackageDecl>> ImportManager::GetAllImportedPackages(bool includeMacroPkg) const
{
    return cjoManager->GetAllPackageDecls(includeMacroPkg);
}

const std::vector<Ptr<PackageDecl>>& ImportManager::GetCurImportedPackages(const std::string& fullPackageName) const
{
    return dependencyGraph->GetDirectDependencyPackageDecls(fullPackageName);
}

const std::vector<Ptr<PackageDecl>>& ImportManager::GetAllDependentPackageDecls(
    const std::string& fullPackageName, bool includeMacroPkg) const
{
    return dependencyGraph->GetAllDependencyPackageDecls(fullPackageName, includeMacroPkg);
}

const std::set<std::string>& ImportManager::GetAllDependentPackageNames(const std::string& fullPackageName) const
{
    // NOTE: exclude packages which is only used by macro expansion.
    return dependencyGraph->GetAllDependencyPackageNames(fullPackageName, false);
}

Ptr<Decl> ImportManager::GetImportedDecl(const std::string& fullPackageName, const std::string& name) const
{
    auto& decls = cjoManager->GetPackageMembersByName(fullPackageName, name);
    if (decls.empty()) {
        auto decl = cjoManager->GetImplicitPackageMembersByName(fullPackageName, name);
        return decl;
    }
    return *decls.begin();
}

void ImportManager::SetPackageCjoCache(const std::string& fullPackageName, const std::vector<uint8_t>& cjoData,
    CjoManager::CjoChangeState changeState) const
{
    cjoManager->SetPackageCjoCache(fullPackageName, cjoData, changeState);
}

void ImportManager::ClearPackageCjoCache() const
{
    cjoManager->ClearCjoCache();
}

bool ImportManager::IsMacroRelatedPackageName(const std::string& fullPackageName) const
{
    return cjoManager->IsMacroRelatedPackageName(fullPackageName);
}

std::string ImportManager::GetMainPartPkgNameForTestPkg(const std::string& testPkgName)
{
    CJC_ASSERT(IsTestPackage(testPkgName));
    return testPkgName.substr(0, testPkgName.length() - SourceManager::testPkgSuffix.length());
}

bool ImportManager::IsTestPackage(const std::string& pkgName)
{
    return pkgName.length() >= SourceManager::testPkgSuffix.length() &&
        pkgName.compare(pkgName.length() - SourceManager::testPkgSuffix.length(), SourceManager::testPkgSuffix.length(),
            SourceManager::testPkgSuffix) == 0;
}

bool ImportManager::IsTypeAccessible(const File& file, const Type& type) const
{
    std::function<Ptr<Decl>(Ptr<Decl>)> getRealTarget = [&getRealTarget](Ptr<Decl> decl) -> Ptr<Decl> {
        auto target = decl;
        if (auto tad = DynamicCast<TypeAliasDecl>(target);
            tad && tad->type && !tad->TestAttr(Attribute::IN_REFERENCE_CYCLE)) {
            auto realTarget = tad->type->GetTarget();
            target = realTarget ? getRealTarget(realTarget) : target;
        }
        return target;
    };
    // NOTE: For compatibility reason, extension declaration's visibility does not depend on alias declarations.
    auto decl = getRealTarget(type.GetTarget());
    if (!decl) {
        // Builtin type and primitive type is always accessible.
        return true;
    }
    return IsDeclAccessible(file, *decl);
}

bool ImportManager::IsTyAccessible(const File& file, const Ty& ty) const
{
    Ptr<Decl> decl = nullptr;
    if (auto sTy = DynamicCast<StructTy>(&ty)) {
        decl = sTy->declPtr;
    } else if (auto cTy = DynamicCast<ClassTy>(&ty)) {
        decl = cTy->declPtr;
    } else if (auto iTy = DynamicCast<InterfaceTy>(&ty)) {
        decl = iTy->declPtr;
    } else if (auto eTy = DynamicCast<EnumTy>(&ty)) {
        decl = eTy->declPtr;
    }
    if (decl) {
        return IsDeclAccessible(file, *decl);
    }
    return true;
}

bool ImportManager::IsDeclAccessible(const File& file, Decl& decl) const
{
    auto ret = decl.fullPackageName == file.curPackage->fullPackageName;
    auto relation = Modules::GetPackageRelation(file.curPackage->fullPackageName, decl.fullPackageName);
    if (!Utils::In(Ptr(&decl), GetImportedDeclsByName(file, decl.identifier))) {
        // Process 'import pkg1.pkg2; pkg2.Decl.extendMember', If 'Decl' is imported, it must visible in cur package.
        auto pkgNames = Utils::SplitQualifiedName(decl.fullPackageName);
        for (auto it = pkgNames.crbegin(); it != pkgNames.crend(); ++it) {
            ret = ret || (!GetImportedDeclsByName(file, *it).empty() && Modules::IsVisible(decl, relation));
        }
        // Process import alias decl or alias-import.
        // If we have 'type TA = A; extend TA {...}' in pkg1, and other pkg import 'pkg1.TA', 'extend A' is accessable.
        auto found = declToTypeAlias.find(&decl);
        if (found != declToTypeAlias.end()) {
            for (auto& tadId : found->second) {
                ret = ret || !GetImportedDeclsByName(file, tadId).empty();
            }
        }
    } else {
        ret = true;
    }
    return ret;
}
namespace {
inline void AddNoteForExtendExportDiag(const Ptr<DiagnosticBuilder> builder, const std::string note)
{
    if (builder) {
        builder->AddNote(note);
    }
}

inline void AddNoteForExtendExportDiag(const Ptr<DiagnosticBuilder> builder, const Range range, const std::string note)
{
    if (builder) {
        builder->AddNote(range, "to use the following extension, import '" + note + "'");
    }
}
} // namespace
bool ImportManager::IsExtendAllUpperBoundsImported(
    const ExtendDecl& ed, const File& file, const Ptr<DiagnosticBuilder> builder) const
{
    if (!ed.generic) {
        return true;
    }
    std::set<std::string> upperboundsNotImported;
    bool areAllUpperBoundsImported = true;
    for (auto& gc : ed.generic->genericConstraints) {
        for (auto& ub : gc->upperBounds) {
            CJC_NULLPTR_CHECK(ub);
            if (!IsTypeAccessible(file, *ub)) {
                areAllUpperBoundsImported = false;
                upperboundsNotImported.emplace(ub->GetTy()->String());
                break;
            }
        }
    }
    for (auto& up : upperboundsNotImported) {
        AddNoteForExtendExportDiag(builder, MakeRange(ed.begin, ed.end), up);
    }
    return areAllUpperBoundsImported;
}

bool ImportManager::IsExtendAccessible(
    const File& file, const ExtendDecl& ed, const Ptr<DiagnosticBuilder> builder) const
{
    if (!file.curPackage || ed.fullPackageName == file.curPackage->fullPackageName) {
        return true;
    }
    auto curPkgName = file.curPackage->fullPackageName;

    if (opts.compileTestsOnly && ed.TestAttr(Attribute::FOR_TEST) && IsTestPackage(curPkgName) &&
        GetMainPartPkgNameForTestPkg(curPkgName) == ed.fullPackageName) {
        return true;
    }
    bool areAllUpperBoundsImported = IsExtendAllUpperBoundsImported(ed, file, builder);
    bool hasAnyInterfacesImported = false;
    bool isExtendedTypeAccessible = false;
    auto extendedDecl = Ty::GetDeclPtrOfTy<InheritableDecl>(ed.GetTy());
    bool isInSamePkg =
        extendedDecl ? ed.fullPackageName == extendedDecl->fullPackageName : ed.fullPackageName == "std.core";
    if (!isInSamePkg) {
        for (auto& inhert : ed.inheritedTypes) {
            if (inhert && IsTypeAccessible(file, *inhert)) {
                hasAnyInterfacesImported = true;
            }
        }
        isExtendedTypeAccessible = IsTypeAccessible(file, *ed.extendedType);
    } else {
        hasAnyInterfacesImported = true;
        if (extendedDecl) {
            auto relation = GetPackageRelation(file.curPackage->fullPackageName, extendedDecl->fullPackageName);
            isExtendedTypeAccessible = IsVisible(*extendedDecl, relation);
        } else {
            isExtendedTypeAccessible = true;
        }
    }
    if (!hasAnyInterfacesImported) {
        if (builder) {
            builder->AddNote(MakeRange(ed.begin, ed.end),
                "to use the following extension, you must import at least one of its inherited interfaces");
        }
    }
    // For direct extension, all upperbound (if any) need to be imported.
    // For interface extension, all upperbound (if any) and at lest one interface need to be imported.
    if (!isExtendedTypeAccessible) {
        AddNoteForExtendExportDiag(builder, MakeRange(ed.begin, ed.end), ed.extendedType->GetTy()->String());
    }
    bool isExtendImported = areAllUpperBoundsImported && hasAnyInterfacesImported && isExtendedTypeAccessible;
    return ed.IsExportedDecl() && isExtendImported;
}

const Ptr<Type> ImportManager::FindImplmentInterface(const File& file, const Decl& member, const Ptr<Type>& it) const
{
    auto targetDecl = Ty::GetDeclPtrOfTy<InheritableDecl>(it->GetTy());
    if (targetDecl == nullptr) {
        return nullptr;
    }
    for (auto& super : targetDecl->inheritedTypes) {
        if (auto implInterface = FindImplmentInterface(file, member, super)) {
            return implInterface;
        }
    }
    for (auto& m : targetDecl->GetMemberDecls()) {
        bool isImpl = typeManager.PairIsOverrideOrImpl(member, *m);
        if (isImpl) {
            return it;
        }
    }
    return nullptr;
}

bool ImportManager::IsExtendMemberImported(
    const Ptr<ExtendDecl> extend, const File& file, const Decl& member, const Ptr<DiagnosticBuilder> builder) const
{
    if (extend->inheritedTypes.empty()) {
        return true;
    }
    auto extendedDecl = Ty::GetDeclPtrOfTy<InheritableDecl>(extend->GetTy());
    bool isInSamePkg =
        extendedDecl ? extend->fullPackageName == extendedDecl->fullPackageName : extend->fullPackageName == "std.core";
    if (isInSamePkg) {
        if (!IsTypeAccessible(file, *extend->extendedType)) {
            AddNoteForExtendExportDiag(builder, MakeRange(extend->begin, extend->end), extend->GetTy()->String());
            return false;
        }
        return true;
    }
    // Check whether the interface to which the member belongs is imported.
    for (auto& super : extend->inheritedTypes) {
        if (auto implInterface = FindImplmentInterface(file, member, super)) {
            if (!IsTypeAccessible(file, *implInterface)) {
                AddNoteForExtendExportDiag(
                    builder, MakeRange(extend->begin, extend->end), implInterface->GetTy()->String());
                return false;
            }
            return true;
        }
    }
    return false;
}

bool ImportManager::IsExtendMemberAccessible(
    const File& file, const Decl& member, Ty& baseTy, const Ptr<DiagnosticBuilder> builder) const
{
    Ptr<ExtendDecl> extend = typeManager.GetExtendDeclByMember(member, baseTy);
    if (extend == nullptr || file.curPackage == nullptr) {
        return true;
    }

    auto curPkgName = file.curPackage->fullPackageName;
    if (opts.compileTestsOnly && !member.TestAttr(Attribute::PRIVATE) && extend->TestAttr(Attribute::FOR_TEST) &&
        IsTestPackage(curPkgName) && GetMainPartPkgNameForTestPkg(curPkgName) == member.fullPackageName) {
        return true;
    }
    // Access to extended members must meet the following four requirements:
    // 1. extension is exported.
    // 2. extension is imported. `IsExtendAccessible` checks 1 and 2.
    // 3. extension member is exported. `IsExportedDecl` checks 3.
    // 4. extension member is imported.
    if (file.curPackage && extend->fullPackageName == file.curPackage->fullPackageName) {
        return true;
    }
    if (!IsExtendAccessible(file, *extend, builder)) {
        return false;
    }
    if (!member.IsExportedDecl()) {
        AddNoteForExtendExportDiag(builder,
            "cannot access this member because it is not declared in any interface implemented by the extension");
        return false;
    }

    return IsExtendMemberImported(extend, file, member, builder);
}

ImportManager::~ImportManager()
{
    DeleteASTWriters();
    DeleteASTLoaders();
    curPackage = nullptr;
}

bool ImportManager::HasBuildIndex() const
{
    return cjoManager->HasBuildIndex();
}

void ImportManager::ClearCachesForRebuild()
{
    // clear the cache of the previous compilation in lsp.
    cjoManager->ClearForReBuildIndex();
    typeManager.Clear();
    typeManager.ClearMapCache();
    cjoFilePaths.clear();
    cjdFilePaths.clear();
    stdDepsMap.clear();
    // Create dependency info after load all packages.
    dependencyGraph->Clear();
    // Clear for AddImportedDeclsForSourcePackage.
    importedDeclsMap.clear();
    fileImportedDeclsMap.clear();
    declsImportedByNodeMap.clear();
    declToTypeAlias.clear();
    // clear for ResolveImports.
    directMacroDeps.clear();
}

bool ImportManager::AnalyzeDepStdPkgsOfBC(const std::string& fullPackageName)
{
    auto existingPkg = cjoManager->GetPackage(fullPackageName);
    if (existingPkg || (cjoFilePaths.find(fullPackageName) != cjoFilePaths.end())) {
        return true;
    }

    std::string cjoPath = FileUtil::FindSerializationFile(fullPackageName, SERIALIZED_FILE_EXTENSION, GetSearchPath());
    if (cjoPath.empty() || !cjoManager->LoadPackageHeader(fullPackageName, cjoPath)) {
        diag.DiagnoseRefactor(DiagKindRefactor::bc_cjo_error, DEFAULT_POSITION, fullPackageName);
        return false;
    }

    auto package = cjoManager->GetPackage(fullPackageName);
    CJC_NULLPTR_CHECK(package);
    for (const auto& depStdPkg : package->GetAllDependentStdPkgs()) {
        CJC_ASSERT(STANDARD_LIBS.find(depStdPkg) != STANDARD_LIBS.end());
        if (cjoFilePaths.find(depStdPkg) == cjoFilePaths.end()) {
            std::string depCjoPath = FileUtil::FindSerializationFile(
                depStdPkg, SERIALIZED_FILE_EXTENSION, GetSearchPath());
            // Stdlib deps in separately-handled .bc files are regarded as indirect.
            HandleStdPackage(depStdPkg, depCjoPath, true);
        }
    }
    return true;
}

void ImportManager::AddPackageSymbolsToImportMap(const std::string& fullPackageName,
    const std::unordered_set<uint64_t>& consumerFileHashes)
{
    auto members = cjoManager->GetPackageMembers(fullPackageName);

    if (members.empty() || consumerFileHashes.empty()) {
        return;
    }

    auto importLevel = AccessLevel::PUBLIC;
    auto relation = Modules::PackageRelation::NONE;

    // Inject at file level (fileImportedDeclsMap[fileHash][name]) to uphold semantic
    // rule 2: macro import is equivalent to a downstream ordinary private import, which
    // is file-scoped (see AddImportedDeclsForSourcePackage: non-reexport injects to
    // fileImportedDeclsMap[fileHash]). Only the files in consumerFileHashes gain
    // visibility. Consumers (GetImportedDeclsByName) already union importedDeclsMap and
    // fileImportedDeclsMap[file.fileHash], so no query-side change is needed.
    for (const auto& [name, declSet] : members) {
        auto visibleDecls = Modules::GetVisibleDeclToMap(declSet, importLevel, relation);
        for (auto fileHash : consumerFileHashes) {
            auto& targetMap = fileImportedDeclsMap[fileHash][name];
            for (auto decl : visibleDecls) {
                targetMap.insert(decl);
                // declsImportedByNodeMap tracks "which decls are imported" for unused-
                // import warnings etc.; it carries no file-level visibility, keep its
                // package-level key (design 4.5.4).
                if (curPackage != nullptr) {
                    declsImportedByNodeMap[curPackage->fullPackageName][decl];
                }
            }
        }
    }
}
