// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares the MinGW_CJNATIVE class.
 */

#ifndef CANGJIE_DRIVER_TOOLCHAIN_MINGW_CJNATIVE_H
#define CANGJIE_DRIVER_TOOLCHAIN_MINGW_CJNATIVE_H

#include "cangjie/Driver/Backend/Backend.h"
#include "cangjie/Driver/Driver.h"
#include "Toolchains/Gnu.h"
#include "cangjie/Driver/Toolchains/ToolChain.h"

namespace Cangjie {
class MinGW_CJNATIVE : public Gnu {
public:
    MinGW_CJNATIVE(const Cangjie::Driver& driver, const DriverOptions& driverOptions,
        std::vector<ToolBatch>& backendCmds)
        : Gnu(driver, driverOptions, backendCmds)
    {
        mingwLibPath = FileUtil::JoinPath(driver.cangjieHome, "third_party/mingw/lib/");
    };
    ~MinGW_CJNATIVE() override = default;

protected:
    std::string GetSharedLibraryExtension() const override
    {
        return ".dll";
    }

    void InitializeLibraryPaths() override;

    void InitializeMinGWSysroot();

    void AddCRuntimeLibraryPaths() override;
    bool PrepareDependencyPath() override;
    // PrepareDependencyPath() always resolves 'ldPath' to the bundled LLD. Note that this is LLD's
    // MinGW driver, which rejects ELF-only options such as '--no-dependent-libraries'.
    bool IsUsingLLDLinker() const override
    {
        return true;
    }
    // COFF: the dependency is a '/DEFAULTLIB:' directive in '.drectve', which lld's COFF port
    // resolves in LinkerDriver::findLib(). '/nodefaultlib' in its argument-less form makes that
    // function refuse every default library, which is the suppression we need.
    //
    // The MinGW driver has no option of its own for this, but it forwards the value of '-Xlink='
    // verbatim to the COFF driver, so the switch is reachable from here.
    //
    // This override is not optional: Gnu, our base class, answers '--no-dependent-libraries', and
    // because IsUsingLLDLinker() is true here we would inherit it and hand an ELF-only option to
    // the MinGW driver, turning '--disable-auto-link' into a link failure.
    //
    // The granularity is coarser than the ELF switch: it also drops '/DEFAULTLIB:' directives
    // contributed by third-party objects rather than only the ones '--link-name' recorded. That is
    // the closest COFF equivalent available, and it is still the right kind of behaviour -- it acts
    // at link time on the consumer, which is what '--disable-auto-link' is defined to do.
    std::vector<std::string> GetDisableAutoLinkArgs() const override
    {
        return {"-Xlink=/nodefaultlib"};
    }

    // Gather library paths from LIBRARY_PATH and compiler guesses.
    void AddSystemLibraryPaths() override;

    std::vector<std::string> ComputeLibPaths() const override;

    std::string GenerateGCCLibPath(const std::pair<std::string, std::string>& gccCrtFilePair) const override;

    void GenerateArchiveTool(const std::vector<TempFileInfo>& objFiles) override;

    void HandleLibrarySearchPaths(Tool& tool, const std::string& cangjieLibPath) override;

    std::pair<std::string, std::string> GetGccCrtFilePair() const override
    {
        return gccExecCrtFilePair;
    }

    std::string GetCjldScript(const std::unique_ptr<Tool>& tool) const;
    void GenerateLinkingTool(const std::vector<TempFileInfo>& objFiles, const std::string& gccLibPath,
        const std::pair<std::string, std::string>& gccCrtFilePair) override;

    void GenerateLinkOptions(Tool& tool) override;

    std::string FindCangjieMinGWToolPath(const std::string toolName) const;

private:
    const std::vector<std::string> MINGW_CJNATIVE_LINK_OPTIONS = {
#define CJNATIVE_WINDOWS_BASIC_OPTIONS(OPTION) (OPTION),
#include "Toolchains/BackendOptions.inc"
#undef CJNATIVE_WINDOWS_BASIC_OPTIONS
    };
    std::string sysroot;
    std::string mingwLibPath;
};
} // namespace Cangjie

#endif // CANGJIE_DRIVER_TOOLCHAIN_MINGW_CJNATIVE_H
