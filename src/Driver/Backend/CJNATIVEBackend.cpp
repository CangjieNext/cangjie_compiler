// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements the cjnative backend class.
 */

#include "cangjie/Driver/Backend/CJNATIVEBackend.h"

#include <fstream>
#include <set>
#include <unordered_set>

#include "Job.h"
#include "cangjie/Driver/TempFileManager.h"
#include "cangjie/Driver/ToolOptions.h"
#include "cangjie/Mangle/BaseMangler.h"
#include "cangjie/Mangle/MangleUtils.h"
#include "cangjie/Utils/Utils.h"
#include "Toolchains/CJNATIVE/Linux_CJNATIVE.h"
#include "Toolchains/CJNATIVE/Darwin_CJNATIVE.h"
#include "Toolchains/CJNATIVE/IOS_CJNATIVE.h"
#include "Toolchains/CJNATIVE/Android_CJNATIVE.h"
#include "Toolchains/CJNATIVE/MinGW_CJNATIVE.h"
#include "Toolchains/CJNATIVE/Ohos_CJNATIVE.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

using namespace Cangjie;
using namespace Cangjie::Triple;

namespace {
std::set<std::string> ReadSymbolLikeListFile(const std::string& filePath)
{
    std::set<std::string> result;
    std::ifstream input(filePath);
    if (!input.is_open()) {
        return result;
    }
    std::string line;
    while (std::getline(input, line)) {
        auto commentPos = line.find('#');
        if (commentPos != std::string::npos) {
            line.erase(commentPos);
        }
        Utils::TrimString(line);
        if (!line.empty()) {
            result.emplace(line);
        }
    }
    return result;
}

void AppendIfNotEmpty(std::set<std::string>& values, const std::string& value)
{
    if (!value.empty()) {
        values.emplace(value);
    }
}

std::set<std::string> GetTempFileIdentifiers(const TempFileInfo& info)
{
    std::set<std::string> ids;
    AppendIfNotEmpty(ids, info.fileName);
    AppendIfNotEmpty(ids, info.filePath);
    AppendIfNotEmpty(ids, info.rawPath);
    if (!info.filePath.empty()) {
        AppendIfNotEmpty(ids, FileUtil::GetFileName(info.filePath));
        AppendIfNotEmpty(ids, FileUtil::GetFileNameWithoutExtension(info.filePath));
    }
    if (!info.rawPath.empty()) {
        AppendIfNotEmpty(ids, FileUtil::GetFileName(info.rawPath));
        AppendIfNotEmpty(ids, FileUtil::GetFileNameWithoutExtension(info.rawPath));
    }
    return ids;
}

bool IsRootListMatch(const TempFileInfo& info, const std::set<std::string>& rootEntries)
{
    auto ids = GetTempFileIdentifiers(info);
    for (const auto& id : ids) {
        if (rootEntries.find(id) != rootEntries.end()) {
            return true;
        }
    }
    return false;
}

std::vector<const TempFileInfo*> MatchRootBitCodeFiles(
    const std::vector<TempFileInfo>& bitCodeFiles, const std::set<std::string>& rootEntries, std::set<std::string>& unmatched)
{
    std::vector<const TempFileInfo*> roots;
    unmatched = rootEntries;
    for (const auto& file : bitCodeFiles) {
        auto ids = GetTempFileIdentifiers(file);
        bool matched = false;
        for (const auto& id : ids) {
            if (unmatched.erase(id) != 0U) {
                matched = true;
            }
        }
        if (matched || IsRootListMatch(file, rootEntries)) {
            roots.emplace_back(&file);
        }
    }
    return roots;
}

bool ShouldKeepRootGlobalValue(const llvm::GlobalValue& GV)
{
    if (GV.getName().empty() || GV.getName().startswith("llvm.")) {
        return false;
    }
    return !GV.isDeclarationForLinker();
}

bool CollectDefinedSymbolsFromBitCode(const std::string& bitcodePath, std::set<std::string>& exportedSymbols)
{
    llvm::LLVMContext context;
    llvm::SMDiagnostic err;
    auto module = llvm::parseIRFile(bitcodePath, err, context);
    if (!module) {
        std::string errMsg;
        llvm::raw_string_ostream os(errMsg);
        err.print("cjc", os);
        Errorln("Failed to parse root LTO bitcode file: " + bitcodePath + "\n" + os.str());
        return false;
    }

    for (const auto& F : module->functions()) {
        if (ShouldKeepRootGlobalValue(F)) {
            exportedSymbols.emplace(F.getName().str());
        }
    }
    for (const auto& GV : module->globals()) {
        if (ShouldKeepRootGlobalValue(GV)) {
            exportedSymbols.emplace(GV.getName().str());
        }
    }
    for (const auto& GA : module->aliases()) {
        if (ShouldKeepRootGlobalValue(GA)) {
            exportedSymbols.emplace(GA.getName().str());
        }
    }
    for (const auto& GIF : module->ifuncs()) {
        if (ShouldKeepRootGlobalValue(GIF)) {
            exportedSymbols.emplace(GIF.getName().str());
        }
    }
    return true;
}

std::string GetExistingBitCodePathForRoot(const TempFileInfo& info)
{
    if (!info.rawPath.empty() && FileUtil::FileExist(info.rawPath)) {
        return info.rawPath;
    }
    if (!info.filePath.empty() && FileUtil::FileExist(info.filePath)) {
        return info.filePath;
    }
    if (!info.filePath.empty()) {
        constexpr const char* optBcSuffix = ".opt.bc";
        if (llvm::StringRef(info.filePath).endswith(optBcSuffix)) {
            auto candidate =
                info.filePath.substr(0, info.filePath.size() - llvm::StringRef(optBcSuffix).size()) + ".bc";
            if (FileUtil::FileExist(candidate)) {
                return candidate;
            }
        }
    }
    return "";
}
} // namespace

bool CJNATIVEBackend::GenerateToolChain()
{
    std::string errFieldStr;
    switch (driverOptions.target.os) {
        case OSType::LINUX:
            if (driverOptions.target.env == Environment::OHOS) {
                TC = std::make_unique<Ohos_CJNATIVE>(driver, driverOptions, backendCmds);
                return true;
            }
            if (driverOptions.target.env == Environment::ANDROID) {
                TC = std::make_unique<Android_CJNATIVE>(driver, driverOptions, backendCmds);
                return true;
            }
            if (driverOptions.target.env == Environment::GNU ||
                driverOptions.target.env == Environment::NOT_AVAILABLE) {
                TC = std::make_unique<Linux_CJNATIVE>(driver, driverOptions, backendCmds);
                return true;
            }
            errFieldStr = driverOptions.target.EnvironmentToString();
            Errorf("Unsupported Environment Type: %s\n", errFieldStr.c_str());
            break;
        case OSType::WINDOWS:
            if (driverOptions.target.env == Triple::Environment::GNU) {
                TC = std::make_unique<MinGW_CJNATIVE>(driver, driverOptions, backendCmds);
                return true;
            }
            errFieldStr = driverOptions.target.EnvironmentToString();
            Errorf("Unsupported Environment Type: %s\n", errFieldStr.c_str());
            break;
        case OSType::DARWIN:
            TC = std::make_unique<Darwin_CJNATIVE>(driver, driverOptions, backendCmds);
            return true;
        case OSType::IOS:
            TC = std::make_unique<IOS_CJNATIVE>(driver, driverOptions, backendCmds);
            return true;
        case OSType::UNKNOWN:
        default:
            errFieldStr = driverOptions.target.OSToString();
            Errorf("Unsupported OS Type: %s\n", errFieldStr.c_str());
            break;
    }
    return false;
}

bool CJNATIVEBackend::PrepareDependencyPath()
{
    std::vector<std::string> cjnativeBinSearchPaths;
    // search in CANGJIE_HOME if it is available
    if (driverOptions.environment.cangjieHome.has_value()) {
        cjnativeBinSearchPaths.emplace_back(
            FileUtil::JoinPath(driverOptions.environment.cangjieHome.value(), "third_party/llvm/bin"));
    }
    // relative path to the current executing cjc
    cjnativeBinSearchPaths.emplace_back(FileUtil::JoinPath(driver.cangjieHome, "third_party/llvm/bin"));
    // They don't have to be in the same location.
    optPath = FileUtil::FindProgramByName(g_toolList.at(ToolID::OPT).name, cjnativeBinSearchPaths);
    if (optPath.empty()) {
        Errorln("not found `opt` in the Cangjie installation, " + CANGJIE_HOME);
        return false;
    }
    llvmLinkPath = FileUtil::FindProgramByName(g_toolList.at(ToolID::LLVM_LINK).name, cjnativeBinSearchPaths);
    if (llvmLinkPath.empty()) {
        Errorln("not found `llvm-link` in the Cangjie installation, " + CANGJIE_HOME);
        return false;
    }
    llcPath = FileUtil::FindProgramByName(g_toolList.at(ToolID::LLC).name, cjnativeBinSearchPaths);
    if (llcPath.empty()) {
        Errorln("not found `llc` in the Cangjie installation, " + CANGJIE_HOME);
        return false;
    }
    llvmArPath = FileUtil::FindProgramByName(g_toolList.at(ToolID::LLVM_AR).name, cjnativeBinSearchPaths);
    if (llvmArPath.empty()) {
        Errorln("not found `llvm-ar` in the Cangjie installation, " + CANGJIE_HOME);
        return false;
    }
    return TC->PrepareDependencyPath();
}

std::vector<TempFileInfo> CJNATIVEBackend::GetFrontendOutputs()
{
    // Treating input .bc files as input makes passing .bc files possible. The following two commands
    // should be identical, except that the first one requires two extra jobs for backend compilation.
    // 1) cjc main.cj pkg1.bc
    // 2) cjc main.cj pkg1.o
    std::vector<TempFileInfo> bitCodeFiles;
    for (const auto& frontendOutput : driverOptions.frontendOutputFiles) {
        bitCodeFiles.emplace_back(frontendOutput);
    }
    for (const auto& bcFile : driverOptions.bcInputFiles) {
        auto filename = FileUtil::GetFileNameWithoutExtension(bcFile);
        bitCodeFiles.emplace_back(TempFileInfo{filename, bcFile, bcFile, false, true});
    }
    return bitCodeFiles;
}

void CJNATIVEBackend::GenerateCacheCopyTool(const std::vector<TempFileInfo>& files, const std::string& extension)
{
    ToolBatch batch{};
    for (auto& file : files) {
        if (file.isForeignInput) {
            continue;
        }
        std::string srcFile = file.filePath;
        std::string destFile =
            driverOptions.GetHashedObjFileName(FileUtil::GetFileNameWithoutExtension(srcFile)) + extension;
        auto tool =
            std::make_unique<Tool>("CacheCopy", ToolType::INTERNAL_IMPLEMENTED, driverOptions.environment.allVariables);
        tool->AppendArg(srcFile, destFile);
        batch.emplace_back(std::move(tool));
    }
    backendCmds.emplace_back(std::move(batch));
}

/**
 * Generate backend tool and linker.
 */
bool CJNATIVEBackend::ProcessGeneration()
{
    CJC_ASSERT_WITH_MSG(
        !(driverOptions.frontendOutputFiles.empty() && driverOptions.inputObjs.empty()), "non-compiled file found!");
    std::vector<TempFileInfo> bitCodeFiles = GetFrontendOutputs();
    return driverOptions.incrementalCompileNoChange ? ProcessGenerationOfIncrementalNoChangeCompile(bitCodeFiles)
                                                    : ProcessGenerationOfNormalCompile(bitCodeFiles);
}

bool CJNATIVEBackend::ProcessGenerationOfNormalCompile(const std::vector<TempFileInfo>& bitCodeFiles)
{
    if (driverOptions.IsLTOEnabled() && driverOptions.enIncrementalCompilation) {
        GenerateCacheCopyTool(bitCodeFiles, ".bc");
    }

    if (driverOptions.IsLTOEnabled()) {
        std::vector<TempFileInfo> frontendOutputFiles;
        std::vector<TempFileInfo> preprocessedFiles;
        for (auto& fileInfo : bitCodeFiles) {
            if (fileInfo.isFrontendOutput) {
                frontendOutputFiles.emplace_back(fileInfo);
            } else {
                preprocessedFiles.emplace_back(fileInfo);
            }
        }
        auto processedFrontendFiles = GeneratePreprocessTools(frontendOutputFiles);
        preprocessedFiles.insert(preprocessedFiles.end(), processedFrontendFiles.begin(), processedFrontendFiles.end());
        // When compiling a static library in LTO mode, the compilation process stops at the opt stage.
        if (driverOptions.outputMode == GlobalOptions::OutputMode::STATIC_LIB) {
            if (driverOptions.HasLTOFinalOutput()) {
                return ProcessGenerationOfFinalStaticLTO(preprocessedFiles);
            }
            return true;
        }
        // In LTO mode, compilation is not performed using llc.
        return TC->ProcessGeneration(preprocessedFiles);
    }

    auto preprocessedFiles = GeneratePreprocessTools(bitCodeFiles);
    if (driverOptions.saveTemps) {
        (void)GenerateCompileTool(preprocessedFiles, true);
    }
    auto objFiles = GenerateCompileTool(preprocessedFiles);
    // copy each obj file from temporary directory to cache directory in normal compile case
    ToolBatch batch{};
    for (auto& objFile : objFiles) {
        std::string srcFile = objFile.filePath;
        std::string destFile =
            driverOptions.GetHashedObjFileName(FileUtil::GetFileNameWithoutExtension(srcFile)) + ".o";
        if (driverOptions.aggressiveParallelCompile.value_or(1) > 1) {
            // the format of parallel compile objFile is: number-pkgName.o
            auto isParallelCompileObjFile = [srcFile] {
                auto fileName = FileUtil::GetFileNameWithoutExtension(srcFile);
                auto extension = FileUtil::GetFileExtension(srcFile);
                auto posOfHyphen = fileName.find("-");
                if (posOfHyphen == std::string::npos) {
                    return false;
                }
                auto parallelId = fileName.substr(0, posOfHyphen);
                // if the parallelId is number and extension is 'o', then it is a parallelCompileObjFile.
                return extension == "o" && parallelId.find_first_not_of("0123456789") == std::string::npos;
            };
            if (isParallelCompileObjFile()) {
                continue;
            }
        }
        auto tool =
            std::make_unique<Tool>("CacheCopy", ToolType::INTERNAL_IMPLEMENTED, driverOptions.environment.allVariables);
        tool->AppendArg(srcFile);
        tool->AppendArg(destFile);
        batch.emplace_back(std::move(tool));
    }
    backendCmds.emplace_back(std::move(batch));
    return TC->ProcessGeneration(objFiles);
}

bool CJNATIVEBackend::ProcessGenerationOfIncrementalNoChangeCompile(const std::vector<TempFileInfo>& bitCodeFiles)
{
    std::vector<TempFileInfo> tempBitCodeFiles = bitCodeFiles;

    if (driverOptions.IsLTOEnabled()) {
        std::vector<TempFileInfo> preprocessorInputs;
        for (auto& file : tempBitCodeFiles) {
            auto tempFile = file;
            if (!file.isForeignInput) {
                std::string filePath =
                    driverOptions.GetHashedObjFileName(FileUtil::GetFileNameWithoutExtension(file.fileName)) + ".bc";
                tempFile.filePath = filePath;
                tempFile.rawPath = filePath;
            }
            preprocessorInputs.emplace_back(tempFile);
        }
        auto preprocessedFiles = GeneratePreprocessTools(preprocessorInputs);
        if (driverOptions.outputMode == GlobalOptions::OutputMode::STATIC_LIB) {
            if (driverOptions.HasLTOFinalOutput()) {
                return ProcessGenerationOfFinalStaticLTO(preprocessedFiles);
            }
            return true;
        }
        return TC->ProcessGeneration(preprocessedFiles);
    }

    // 1. check if all `.o` file exist, if any of them does not exist, then run back to
    // `ProcessGenerationOfNormalCompile`.
    // 2. if all the `.o` files exist, Update the file path in the `bitCodeFiles` from referencing `.bc` file to the
    // corresponding `.o` file, and then trigger the ld.
    for (auto& file : tempBitCodeFiles) {
        auto objFile = driverOptions.GetHashedObjFileName(file.fileName) + ".o";
        if (!FileUtil::FileExist(objFile)) {
            Errorf("The cache directory is incomplete.\n");
            return false;
        }
        file.filePath = driverOptions.GetHashedObjFileName(file.fileName) + ".o";
    }
    return TC->ProcessGeneration(tempBitCodeFiles);
}

void CJNATIVEBackend::PreprocessOfNewPassManager(Tool& tool)
{
    std::string passesCollector = "-passes=";
    std::vector<std::string> passItems;
    {
        using namespace ToolOptions;
        SetFuncType setOptimizationLevelHandler = [&passItems](
                                                      const std::string& option) { passItems.emplace_back(option); };
        SetOptions(setOptimizationLevelHandler, driverOptions, OPT::SetOptimizationLevelOptions);
        // remove the initial hyphen in the options.
        SetFuncType setOptionHandler = [&passItems](
                                           const std::string& option) { passItems.emplace_back(option.substr(1)); };
        SetOptions(setOptionHandler, driverOptions, OPT::SetNewPassManagerOptions);
    }

    for (auto& it : passItems) {
        passesCollector += it;
        if (&it != &passItems.back()) {
            passesCollector += ",";
        }
    }
    tool.AppendArg(passesCollector);
}

std::vector<TempFileInfo> CJNATIVEBackend::GeneratePreprocessTools(const std::vector<TempFileInfo>& bitCodeFiles)
{
    std::vector<TempFileInfo> outputFiles;
    ToolBatch batch{};
    for (const auto& bitCodeFile : bitCodeFiles) {
        // 'opt' can only process one file in one execution, for each bitCodeFile, generate one 'opt' command for it.
        std::unique_ptr<Tool> tool = GenerateCJNativeBaseTool(optPath);
        // set input
        tool->AppendArg(bitCodeFile.filePath);

        // set options
        // handle the new pass manager of 'opt'
        PreprocessOfNewPassManager(*tool);
        {
            using namespace ToolOptions;
            SetFuncType setOptionHandler = [&tool](const std::string& option) { tool->AppendArg(option); };
            std::vector<ToolOptionType> setOptionsPass = {
                OPT::SetOptions,                // Comment ensure vector members are arranged vertically.
                OPT::SetVerifyOptions,          //
                OPT::SetTripleOptions,          //
                OPT::SetCodeObfuscationOptions, //
                OPT::SetLTOOptions,             //
                OPT::SetPgoOptions,             //
                OPT::SetTransparentOptions      // The transparent options must after other options.
            };
            SetOptions(setOptionHandler, driverOptions, setOptionsPass);
        }

        // set output
        // When compiling a static library in LTO mode
        // the optimized bc file generated by the opt phase is used as the output file.
        TempFileKind kind = driverOptions.IsLTOEnabled() &&
                driverOptions.outputMode == GlobalOptions::OutputMode::STATIC_LIB &&
                !driverOptions.HasLTOFinalOutput()
            ? TempFileKind::O_OPT_BC
            : TempFileKind::T_OPT_BC;
        TempFileInfo optBcFileInfo = TempFileManager::Instance().CreateNewFileInfo(bitCodeFile, kind);
        tool->AppendArg("-o", optBcFileInfo.filePath);
        outputFiles.emplace_back(optBcFileInfo);
        batch.emplace_back(std::move(tool));
    }
    backendCmds.emplace_back(std::move(batch));
    return outputFiles;
}

std::vector<TempFileInfo> CJNATIVEBackend::GenerateCompileTool(
    const std::vector<TempFileInfo>& bitCodeFiles, bool emitAssembly)
{
    std::vector<TempFileInfo> outputFiles;
    ToolBatch batch{};
    for (const auto& bitCodeFile : bitCodeFiles) {
        // 'llc' can only process one file in one execution, for each bitCodeFile,
        // generate one 'llc' command for it, just like 'opt'.
        std::unique_ptr<Tool> tool = GenerateCJNativeBaseTool(llcPath);

        // set input
        tool->AppendArg(bitCodeFile.filePath);

        // set options
        {
            using namespace ToolOptions;
            SetFuncType setOptionHandler = [&tool](const std::string& option) { tool->AppendArg(option); };
            std::vector<ToolOptionType> setOptionsPass = {
                LLC::SetOptions,                  // Comment ensure vector members are arranged vertically.
                LLC::SetTripleOptions,             //
                LLC::SetOptimizationLevelOptions, //
                LLC::SetTransparentOptions,       // The transparent options must after other options.
            };
            SetOptions(setOptionHandler, driverOptions, setOptionsPass);
        }

        // set output
        tool->AppendArg(emitAssembly ? "--filetype=asm" : "--filetype=obj");
        auto fileKind = emitAssembly ? TempFileKind::T_ASM : TempFileKind::T_OBJ;
        TempFileInfo fileInfo = TempFileManager::Instance().CreateNewFileInfo(bitCodeFile, fileKind);
        tool->AppendArg("-o", fileInfo.filePath);
        outputFiles.emplace_back(fileInfo);
        batch.emplace_back(std::move(tool));
    }
    backendCmds.emplace_back(std::move(batch));
    return outputFiles;
}

std::unique_ptr<Tool> CJNATIVEBackend::GenerateCJNativeBaseTool(const std::string& toolPath)
{
    auto tool = std::make_unique<Tool>(toolPath, ToolType::BACKEND, driverOptions.environment.allVariables);
    tool->SetLdLibraryPath(FileUtil::JoinPath(FileUtil::GetDirPath(toolPath), "../lib"));
    return tool;
}

bool CJNATIVEBackend::AppendBuiltinLTOBitcodes(std::vector<TempFileInfo>& bitCodeFiles) const
{
    std::unordered_set<std::string> existingPaths;
    for (const auto& file : bitCodeFiles) {
        existingPaths.emplace(file.filePath);
    }

    auto appendDependencies = [&bitCodeFiles, &existingPaths](const std::unordered_set<std::string>& deps) {
        for (const auto& cjoFileName : deps) {
            auto bcFile = FileUtil::ConvertFilenameToLtoLibCangjieFormat(cjoFileName);
            if (!FileUtil::FileExist(bcFile) || existingPaths.find(bcFile) != existingPaths.end()) {
                continue;
            }
            existingPaths.emplace(bcFile);
            bitCodeFiles.emplace_back(
                TempFileInfo{FileUtil::GetFileNameWithoutExtension(bcFile), bcFile, bcFile, false, true});
        }
    };

    appendDependencies(driverOptions.directBuiltinDependencies);
    appendDependencies(driverOptions.indirectBuiltinDependencies);
    return true;
}

bool CJNATIVEBackend::BuildLTOExportList(
    const std::vector<TempFileInfo>& bitCodeFiles, std::optional<std::string>& exportListPath)
{
    bool useExportList = !driverOptions.ltoExportListFile.empty();
    bool useRootBcList = !useExportList && !driverOptions.ltoRootBcListFile.empty();
    bool useCompileAsExeKeepSet =
        !useExportList && !useRootBcList && driverOptions.IsCompileAsExeEnabled();
    if (!useExportList && !useRootBcList && !useCompileAsExeKeepSet) {
        exportListPath = std::nullopt;
        return true;
    }

    std::set<std::string> exportedSymbols;
    if (useExportList) {
        std::ifstream input(driverOptions.ltoExportListFile);
        if (!input.is_open()) {
            Errorln("Failed to open lto export list file: " + driverOptions.ltoExportListFile);
            return false;
        }
        std::string line;
        while (std::getline(input, line)) {
            auto commentPos = line.find('#');
            if (commentPos != std::string::npos) {
                line.erase(commentPos);
            }
            Utils::TrimString(line);
            if (!line.empty()) {
                exportedSymbols.emplace(line);
            }
        }
    }

    std::vector<const TempFileInfo*> rootBitCodeFiles;
    if (useRootBcList) {
        auto rootEntries = ReadSymbolLikeListFile(driverOptions.ltoRootBcListFile);
        if (rootEntries.empty()) {
            Errorln("Failed to determine root bitcode entries from --lto-root-bc-list.");
            return false;
        }
        std::set<std::string> unmatched;
        rootBitCodeFiles = MatchRootBitCodeFiles(bitCodeFiles, rootEntries, unmatched);
        if (!unmatched.empty()) {
            Errorln("Some root bitcode entries from --lto-root-bc-list do not match final LTO inputs.");
            return false;
        }
        if (rootBitCodeFiles.empty()) {
            Errorln("No root bitcode files matched --lto-root-bc-list.");
            return false;
        }
        for (const auto* file : rootBitCodeFiles) {
            auto rootBitCodePath = GetExistingBitCodePathForRoot(*file);
            if (rootBitCodePath.empty()) {
                Errorln("Failed to locate an existing root LTO bitcode file for --lto-root-bc-list.");
                return false;
            }
            if (!CollectDefinedSymbolsFromBitCode(rootBitCodePath, exportedSymbols)) {
                return false;
            }
        }
    }

    std::set<std::string> packageInitSymbols;
    auto appendPackageInit = [&packageInitSymbols](const TempFileInfo& file) {
        if (file.fileName.empty()) {
            return;
        }
        const auto& pkgName = file.fileName;
        packageInitSymbols.emplace(
            MANGLE_CANGJIE_PREFIX + MANGLE_GLOBAL_PACKAGE_INIT_PREFIX + MangleUtils::GetOptPkgName(pkgName) +
            SPECIAL_NAME_FOR_INIT_FUNCTION + MANGLE_FUNC_PARAM_TYPE_PREFIX + MANGLE_VOID_TY_SUFFIX);
    };
    if (useRootBcList) {
        for (const auto* file : rootBitCodeFiles) {
            appendPackageInit(*file);
        }
    } else {
        for (const auto& file : bitCodeFiles) {
            appendPackageInit(file);
        }
    }
    if (packageInitSymbols.empty()) {
        Errorln("Failed to determine package init symbols for final static LTO output.");
        return false;
    }
    exportedSymbols.insert(packageInitSymbols.begin(), packageInitSymbols.end());

    auto exportListFile = FileUtil::JoinPath(
        TempFileManager::Instance().GetTempFolder(), "cangjie-lto-export-" + Utils::GenerateRandomHexString() + ".txt");
    std::ofstream output(exportListFile, std::ios::trunc);
    if (!output.is_open()) {
        Errorln("Failed to create generated lto export list file: " + exportListFile);
        return false;
    }
    for (const auto& symbol : exportedSymbols) {
        output << symbol << '\n';
    }
    exportListPath = exportListFile;
    return true;
}

std::string CJNATIVEBackend::BuildFinalLTOPassPipeline(bool enableInternalize, bool enableRootCJMetadataPrune) const
{
    std::vector<std::string> passItems;
    {
        using namespace ToolOptions;
        SetFuncType setOptimizationLevelHandler = [&passItems](const std::string& option) {
            passItems.emplace_back(option);
        };
        SetOptions(setOptimizationLevelHandler, driverOptions, OPT::SetOptimizationLevelOptions);
        SetFuncType setOptionHandler = [&passItems](const std::string& option) {
            passItems.emplace_back(option.substr(1));
        };
        SetOptions(setOptionHandler, driverOptions, OPT::SetNewPassManagerOptions);
    }

    std::string passesCollector = "-passes=";
    bool needComma = false;
    auto appendPass = [&passesCollector, &needComma](const std::string& passName) {
        if (needComma) {
            passesCollector += ",";
        }
        passesCollector += passName;
        needComma = true;
    };
    if (enableInternalize) {
        appendPass("internalize");
    }
    for (const auto& passItem : passItems) {
        appendPass(passItem);
    }
    appendPass("globaldce");
    if (enableRootCJMetadataPrune) {
        appendPass("cj-root-reflection-prune");
        appendPass("cj-root-metadata-mark");
    }
    return passesCollector;
}

bool CJNATIVEBackend::ProcessGenerationOfFinalStaticLTO(const std::vector<TempFileInfo>& bitCodeFiles)
{
    std::vector<TempFileInfo> finalBitCodeFiles = bitCodeFiles;
    if (!AppendBuiltinLTOBitcodes(finalBitCodeFiles) || finalBitCodeFiles.empty()) {
        return false;
    }

    const auto baseFileName =
        finalBitCodeFiles.front().fileName.empty() ? "lto_final" : finalBitCodeFiles.front().fileName;
    TempFileInfo mergeBaseInfo{baseFileName + "_whole", "", "", true, false};
    TempFileInfo mergedBcInfo = TempFileManager::Instance().CreateNewFileInfo(mergeBaseInfo, TempFileKind::T_OPT_BC);
    {
        auto tool = GenerateCJNativeBaseTool(llvmLinkPath);
        for (const auto& file : finalBitCodeFiles) {
            tool->AppendArg(file.filePath);
        }
        tool->AppendArg("-o", mergedBcInfo.filePath);
        backendCmds.emplace_back(MakeSingleToolBatch({std::move(tool)}));
    }

    std::optional<std::string> exportListPath;
    if (!BuildLTOExportList(finalBitCodeFiles, exportListPath)) {
        return false;
    }
    const bool enableInternalize = exportListPath.has_value();
    const bool enableRootCJMetadataPrune =
        driverOptions.enableLTORootCJMetadataPrune && exportListPath.has_value();

    TempFileInfo optimizedBaseInfo{baseFileName + "_final", "", "", true, false};
    TempFileInfo optimizedBcInfo =
        TempFileManager::Instance().CreateNewFileInfo(optimizedBaseInfo, TempFileKind::T_OPT_BC);
    {
        auto tool = GenerateCJNativeBaseTool(optPath);
        tool->AppendArg(mergedBcInfo.filePath);
        tool->AppendArg(BuildFinalLTOPassPipeline(enableInternalize, enableRootCJMetadataPrune));
        {
            using namespace ToolOptions;
            SetFuncType setOptionHandler = [&tool](const std::string& option) { tool->AppendArg(option); };
            std::vector<ToolOptionType> setOptionsPass = {
                OPT::SetOptions,
                OPT::SetVerifyOptions,
                OPT::SetTripleOptions,
                OPT::SetCodeObfuscationOptions,
                OPT::SetPgoOptions,
                OPT::SetTransparentOptions,
            };
            SetOptions(setOptionHandler, driverOptions, setOptionsPass);
        }
        if (enableInternalize) {
            tool->AppendArg("--internalize-public-api-file=" + exportListPath.value());
        }
        if (enableRootCJMetadataPrune) {
            tool->AppendArg("--cj-root-bc-file=" + driverOptions.ltoRootBcListFile);
            tool->AppendArg("--cj-root-symbol-file=" + exportListPath.value());
        }
        tool->AppendArg("-o", optimizedBcInfo.filePath);
        backendCmds.emplace_back(MakeSingleToolBatch({std::move(tool)}));
    }

    TempFileInfo llcOutputInfo = driverOptions.EmitLTOFinalObject()
        ? TempFileManager::Instance().CreateNewFileInfo(
              TempFileInfo{baseFileName, "", "", true, false}, TempFileKind::O_OBJ)
        : TempFileManager::Instance().CreateNewFileInfo(
              TempFileInfo{baseFileName + "_final", "", "", true, false}, TempFileKind::T_OBJ);
    {
        auto tool = GenerateCJNativeBaseTool(llcPath);
        tool->AppendArg(optimizedBcInfo.filePath);
        {
            using namespace ToolOptions;
            SetFuncType setOptionHandler = [&tool](const std::string& option) { tool->AppendArg(option); };
            std::vector<ToolOptionType> setOptionsPass = {
                LLC::SetOptions,
                LLC::SetTripleOptions,
                LLC::SetOptimizationLevelOptions,
                LLC::SetTransparentOptions,
            };
            SetOptions(setOptionHandler, driverOptions, setOptionsPass);
        }
        tool->AppendArg("--filetype=obj");
        tool->AppendArg("-o", llcOutputInfo.filePath);
        backendCmds.emplace_back(MakeSingleToolBatch({std::move(tool)}));
    }

    if (driverOptions.EmitLTOFinalObject()) {
        if (!driverOptions.inputObjs.empty()) {
            Errorln("--lto-final-output=object does not support extra object or archive inputs.");
            return false;
        }
        return true;
    }

    TempFileInfo finalArchiveInfo = TempFileManager::Instance().CreateNewFileInfo(
        TempFileInfo{baseFileName, "", "", true, false}, TempFileKind::O_STATICLIB);
    auto tool = GenerateCJNativeBaseTool(llvmArPath);
    tool->AppendArg("cr");
    (void)FileUtil::Remove(finalArchiveInfo.filePath.c_str());
    tool->AppendArg(finalArchiveInfo.filePath);
    tool->AppendArg(llcOutputInfo.filePath);
    for (const auto& inputObj : driverOptions.inputObjs) {
        tool->AppendArg(inputObj);
    }
    backendCmds.emplace_back(MakeSingleToolBatch({std::move(tool)}));
    return true;
}
