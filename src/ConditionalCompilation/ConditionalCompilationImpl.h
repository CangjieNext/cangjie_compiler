// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares class ConditionalCompilationImpl.
 */

#ifndef CANGJIE_CONDITIONALCOMPILATION_CONDITIONALCOMPILATIONIMPL_H
#define CANGJIE_CONDITIONALCOMPILATION_CONDITIONALCOMPILATIONIMPL_H

#include <regex>
#include <optional>

#include "cangjie/AST/Match.h"
#include "cangjie/AST/Walker.h"
#include "cangjie/Basic/DiagnosticEngine.h"
#include "cangjie/Basic/Utils.h"
#include "cangjie/Basic/Version.h"
#include "cangjie/ConditionalCompilation/ConditionalCompilation.h"
#include "cangjie/Frontend/CompilerInstance.h"
#include "cangjie/Option/Option.h"
#include "cangjie/Utils/FileUtil.h"

namespace Cangjie::AST {

/**
 * @brief Version information structure for cjc_version condition compilation.
 * 
 * Supports four version formats:
 * 1. LTS version: x.0.z (x, z: 0-99)
 * 2. STS version: x.y.z (y: 1-99, x, z: 0-99)
 * 3. Iteration version: x.y.z-alpha/beta.xy (x: iteration number 0-99, y: sub-version 0-10)
 * 4. Nightly version: x.y.z-YYYYMMDDHHMMSS
 */
struct VersionInfo {
    uint32_t major = 0;
    uint32_t minor = 0;
    uint32_t patch = 0;

    enum class PreReleaseType {
        NONE,
        ALPHA,
        BETA,
        NIGHTLY
    } preReleaseType = PreReleaseType::NONE;

    uint32_t preReleaseNum = 0;
    uint32_t preReleaseSubNum = 0;
    uint64_t nightlyDateTime = 0;

    bool isValid = false;
};


class ConditionalCompilationImpl {
public:
    explicit ConditionalCompilationImpl(CompilerInstance* c);
    void HandleConditionalCompilation(const Package& root);
    void HandleFileConditionalCompilation(File& file);

private:
    inline std::string GetBackendType() const
    {
        if (backendType == Triple::BackendType::CJNATIVE) {
            return "cjnative";
        } else {
            return Triple::BackendToString(backendType);
        }
    }
    inline std::string GetArchType() const
    {
        return triple.ArchToString();
    }
    inline std::string GetEnv() const
    {
        return triple.EnvironmentToSimpleString();
    }
    std::string GetOSType() const;
    std::string GetCJCVersion() const;
    inline std::string GetDebug() const
    {
        return std::to_string(static_cast<int>(debug));
    }
    inline std::string GetTest() const
    {
        return std::to_string(static_cast<int>(test));
    }
    std::optional<std::string> GetUserDefinedInfoByName(const std::string& name) const;
    inline auto GetPassedValues() const
    {
        return passedCondition;
    }
    std::optional<std::string> GetRelatedInfo(const std::string& target) const;

    CompilerInstance* ci{nullptr};
    Triple::BackendType backendType;
    Triple::Info triple;
    VersionInfo cjcVersionInfo;
    bool debug;
    bool test;
    std::unordered_map<std::string, std::string> passedCondition;

    bool EvalConditionExpr(const Expr& condition);

    bool ConditionCheck(const std::string& conditionStr, const Position& begin, const std::string& right);

    bool Eval(const BinaryExpr& expr, const std::string& left, const std::string& right) const;
    bool EvalVersion(const BinaryExpr& expr, const VersionInfo& left, const VersionInfo& right) const;

    bool EvalBinaryExpr(const BinaryExpr& be);
    bool EvalParenExpr(const ParenExpr& pe);
    bool EvalUnaryExpr(const UnaryExpr& ue) const;
    bool EvalRefExpr(const RefExpr& re) const;

    bool EvalLogicBinaryExpr(const BinaryExpr& be);
    bool EvalJudgeBinaryExpr(const BinaryExpr& be);

    static VersionInfo ParseVersion(const std::string& version);
    static int CompareVersion(const VersionInfo& left, const VersionInfo& right);
    static bool IsValidVersionNumber(uint32_t num);

    template <typename T> bool EvalNodeCondition(Ptr<T> node);
};
} // namespace Cangjie::AST
#endif