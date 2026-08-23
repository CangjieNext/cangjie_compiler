// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * Shared gtest fixture for ConstAnalysis LLT-migrated unit tests.
 */

#ifndef CANGJIE_UNITTESTS_CHIR_CONSTANALYSISFIXTURE_H
#define CANGJIE_UNITTESTS_CHIR_CONSTANALYSISFIXTURE_H

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>

#include "cangjie/Basic/DiagnosticEngine.h"
#include "cangjie/CHIR/Analysis/ConstAnalysisWrapper.h"
#include "cangjie/CHIR/IR/CHIRBuilder.h"
#include "cangjie/CHIR/IR/Expression/Expression.h"
#include "cangjie/CHIR/IR/Expression/Terminator.h"
#include "cangjie/CHIR/IR/Package.h"
#include "cangjie/CHIR/IR/Value/LiteralValue.h"
#include "cangjie/Utils/ConstantsUtils.h"

using namespace Cangjie;
using namespace Cangjie::CHIR;

class ConstAnalysisFixture : public ::testing::Test {
protected:
    void SetUp() override
    {
        nameMap = std::make_unique<std::unordered_map<unsigned int, std::string>>();
        cctx = std::make_unique<CHIRContext>(nameMap.get());
        builder = std::make_unique<CHIRBuilder>(*cctx);
        package = builder->CreatePackage("default");
        diag = std::make_unique<DiagnosticEngine>();
        wrapper = std::make_unique<ConstAnalysisWrapper>(*builder);
        funcCounter = 0;
    }

    Type* Ty(const char* name)
    {
        std::string n(name);
        if (n == "Int8") {
            return builder->GetInt8Ty();
        }
        if (n == "Int16") {
            return builder->GetInt16Ty();
        }
        if (n == "Int32") {
            return builder->GetInt32Ty();
        }
        if (n == "Int64") {
            return builder->GetInt64Ty();
        }
        if (n == "UInt8") {
            return builder->GetUInt8Ty();
        }
        if (n == "UInt16") {
            return builder->GetUInt16Ty();
        }
        if (n == "UInt32") {
            return builder->GetUInt32Ty();
        }
        if (n == "UInt64") {
            return builder->GetUInt64Ty();
        }
        if (n == "Float16") {
            return builder->GetFloat16Ty();
        }
        if (n == "Float32") {
            return builder->GetFloat32Ty();
        }
        if (n == "Float64") {
            return builder->GetFloat64Ty();
        }
        if (n == "Bool") {
            return builder->GetBoolTy();
        }
        return builder->GetUnitTy();
    }

    Function* NewFunc()
    {
        auto name = "f" + std::to_string(funcCounter++);
        auto* funcTy = builder->GetType<FuncType>(std::vector<Type*>{}, builder->GetUnitTy());
        auto* func = builder->CreateFunction(funcTy, name, name, "", "default");
        auto* body = builder->CreateBlockGroup(*func);
        func->InitBody(*body);
        auto* block = builder->CreateBlock(body);
        body->SetEntryBlock(block);
        curFunc = func;
        curBlock = block;
        return func;
    }

    Value* LitBool(bool v)
    {
        auto* c = builder->CreateConstantExpression<BoolLiteral>(builder->GetBoolTy(), curBlock, v);
        curBlock->AppendExpression(c);
        return c->GetResult();
    }

    Value* LitInt(Type* ty, uint64_t v)
    {
        auto* c = builder->CreateConstantExpression<IntLiteral>(ty, curBlock, v);
        curBlock->AppendExpression(c);
        return c->GetResult();
    }

    Value* LitFloat(Type* ty, double v)
    {
        auto* c = builder->CreateConstantExpression<FloatLiteral>(ty, curBlock, v);
        curBlock->AppendExpression(c);
        return c->GetResult();
    }

    Value* Lit(const char* tyName, const char* val)
    {
        std::string ty(tyName);
        std::string v(val);
        if (ty == "Bool") {
            return LitBool(v == "true");
        }
        if (ty == "Float32" || ty == "Float16") {
            // Match frontend literal lowering: round through float width first.
            return LitFloat(Ty(tyName), static_cast<double>(static_cast<float>(std::stod(v))));
        }
        if (ty.rfind("Float", 0) == 0) {
            return LitFloat(Ty(tyName), std::stod(v));
        }
        if (ty.rfind('U', 0) == 0) {
            return LitInt(Ty(tyName), std::stoull(v));
        }
        return LitInt(Ty(tyName), static_cast<uint64_t>(std::stoll(v)));
    }

    void FinishFunc()
    {
        curBlock->AppendExpression(builder->CreateTerminator<Exit>(curBlock));
    }

    std::vector<std::string> Analyse(Function* func)
    {
        auto results = wrapper->RunOnFunc(func, false, *diag);
        std::vector<std::string> got;
        results->VisitWith(
            [](const ConstDomain&, Expression*, size_t) {},
            [&got](const ConstDomain& state, Expression* expr, size_t) {
                if (expr == nullptr || expr->GetResult() == nullptr) {
                    return;
                }
                auto kind = expr->GetExprKindName();
                if (kind == "Constant") {
                    return;
                }
                if (auto* abs = state.CheckAbstractValue(expr->GetResult())) {
                    got.emplace_back(std::string(kind) + "=" + abs->ToString());
                }
            },
            [](const ConstDomain&, Expression*, std::optional<Block*>) {});
        return got;
    }

    void ExpectSorted(std::vector<std::string> got, std::vector<std::string> exp)
    {
        std::sort(got.begin(), got.end());
        std::sort(exp.begin(), exp.end());
        EXPECT_EQ(got, exp);
    }

    std::unique_ptr<std::unordered_map<unsigned int, std::string>> nameMap;
    std::unique_ptr<CHIRContext> cctx;
    std::unique_ptr<CHIRBuilder> builder;
    Package* package{nullptr};
    std::unique_ptr<DiagnosticEngine> diag;
    std::unique_ptr<ConstAnalysisWrapper> wrapper;
    Function* curFunc{nullptr};
    Block* curBlock{nullptr};
    int funcCounter{0};
};


#endif
