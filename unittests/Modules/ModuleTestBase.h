// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_UNITTESTS_MODULES_MODULE_TEST_BASE_H
#define CANGJIE_UNITTESTS_MODULES_MODULE_TEST_BASE_H

#include "gtest/gtest.h"

#include <cstdlib>

#include "TestCompilerInstance.h"
#include "cangjie/Utils/FileUtil.h"

class ModuleTestBase : public testing::Test {
protected:
    void SetUp() override
    {
        SetupTempDirectory();
        SetupCompilerOptions();
        CompileCoreMock();
        CompileAllFiles();
    }

    void SetupTempDirectory()
    {
#ifdef _WIN32
        std::string command;
        int err = 0;
        if (Cangjie::FileUtil::FileExist("testTempFiles")) {
            std::string command = "rmdir /s /q testTempFiles";
            int err = system(command.c_str());
            std::cout << err;
        }
        command = "mkdir testTempFiles";
        err = system(command.c_str());
        ASSERT_EQ(0, err);
        srcPath = projectPath + "\\unittests\\Modules\\CangjieFiles\\";
        packagePath = packagePath + "\\";
#else
        std::string command;
        int err = 0;
        if (Cangjie::FileUtil::FileExist("testTempFiles")) {
            command = "rm -rf testTempFiles";
            err = system(command.c_str());
            ASSERT_EQ(0, err);
        }
        command = "mkdir -p testTempFiles";
        err = system(command.c_str());
        ASSERT_EQ(0, err);
        srcPath = projectPath + "/unittests/Modules/CangjieFiles/";
        packagePath = packagePath + "/";
#endif
    }

    void SetupCompilerOptions()
    {
#ifdef __x86_64__
        invocation.globalOptions.target.arch = Cangjie::Triple::ArchType::X86_64;
#else
        invocation.globalOptions.target.arch = Cangjie::Triple::ArchType::AARCH64;
#endif
#ifdef _WIN32
        invocation.globalOptions.target.os = Cangjie::Triple::OSType::WINDOWS;
#elif defined(__unix__)
        invocation.globalOptions.target.os = Cangjie::Triple::OSType::LINUX;
#endif
        invocation.globalOptions.outputMode = Cangjie::GlobalOptions::OutputMode::STATIC_LIB;
        invocation.globalOptions.disableReflection = true;
        invocation.globalOptions.importPaths = {packagePath};
        invocation.globalOptions.compilationCachedPath = ".";
    }

    void CompileCoreMock()
    {
        auto coreFile = srcPath + "coremock.cj";
        std::string failedReason;
        auto content = Cangjie::FileUtil::ReadFileContent(coreFile, failedReason);
        if (!content.has_value()) {
            diag.DiagnoseRefactor(
                Cangjie::DiagKindRefactor::module_read_file_to_buffer_failed, DEFAULT_POSITION, coreFile, failedReason);
        }
        instance = std::make_unique<Cangjie::TestCompilerInstance>(invocation, diag);
        CreateModulesDirectory();
        instance->code = std::move(content.value());
        instance->invocation.globalOptions.implicitPrelude = false;
        instance->Compile(Cangjie::CompileStage::DESUGAR_AFTER_SEMA);
        std::vector<uint8_t> astData;
        instance->importManager->ExportAST(false, astData, *instance->GetSourcePackages()[0]);
        allAstData["std.core"] = astData;
        AfterCompileCoreMock(astData);
        instance->invocation.globalOptions.implicitPrelude = true;
    }

    void CreateModulesDirectory()
    {
        auto modulesName = Cangjie::FileUtil::JoinPath(instance->cangjieHome, "modules");
        auto libPathName = invocation.globalOptions.GetCangjieLibTargetPathName();
        cangjieModulesPath = Cangjie::FileUtil::JoinPath(modulesName, libPathName);
        if (!Cangjie::FileUtil::FileExist(cangjieModulesPath)) {
            Cangjie::FileUtil::CreateDirs(Cangjie::FileUtil::JoinPath(cangjieModulesPath, ""));
        }
    }

    void CompileAllFiles()
    {
        for (unsigned int i = 0; i < fileNames.size(); i++) {
            CompileSingleFile(fileNames[i]);
        }
        AfterCompileAllFiles();
    }

    void CompileSingleFile(const std::string& fileName)
    {
        auto srcFile = srcPath + fileName + ".cj";
        std::string failedReason;
        auto content = Cangjie::FileUtil::ReadFileContent(srcFile, failedReason);
        if (!content.has_value()) {
            diag.DiagnoseRefactor(
                Cangjie::DiagKindRefactor::module_read_file_to_buffer_failed, DEFAULT_POSITION, srcFile, failedReason);
        }
        instance = std::make_unique<Cangjie::TestCompilerInstance>(invocation, diag);
        BeforeCompileSingleFile();
        instance->code = std::move(content.value());
        instance->Compile();
        std::vector<uint8_t> astData;
        instance->importManager->ExportAST(false, astData, *instance->GetSourcePackages()[0]);
        allAstData[fileName] = astData;
        AfterCompileSingleFile(fileName, astData);
        diag.Reset();
    }

    virtual void AfterCompileCoreMock(const std::vector<uint8_t>& astData) {}
    virtual void BeforeCompileSingleFile() {}
    virtual void AfterCompileSingleFile(const std::string& fileName, const std::vector<uint8_t>& astData) {}
    virtual void AfterCompileAllFiles() {}

#ifdef PROJECT_SOURCE_DIR
    std::string projectPath = PROJECT_SOURCE_DIR;
#else
    std::string projectPath = "..";
#endif
    std::vector<std::string> fileNames{
        "vardecl", "funcdecl", "math", "recorddecl", "interfacedecl", "classdecl", "extenddecl"};
    std::string packagePath = "testTempFiles";
    std::string srcPath;
    std::string cangjieModulesPath;
    Cangjie::DiagnosticEngine diag;
    Cangjie::CompilerInvocation invocation;
    std::unique_ptr<Cangjie::TestCompilerInstance> instance;
    std::unordered_map<std::string, std::vector<uint8_t>> allAstData;
};

#endif // CANGJIE_UNITTESTS_MODULES_MODULE_TEST_BASE_H
