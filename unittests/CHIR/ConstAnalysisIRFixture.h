// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * Shared gtest fixture for ConstAnalysis IR-level unit tests.
 */

#ifndef CANGJIE_UNITTESTS_CHIR_CONSTANALYSISIRFIXTURE_H
#define CANGJIE_UNITTESTS_CHIR_CONSTANALYSISIRFIXTURE_H

#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>

#include "cangjie/Basic/DiagnosticEngine.h"
#include "cangjie/CHIR/Analysis/ConstAnalysis.h"
#include "cangjie/CHIR/Analysis/ConstAnalysisWrapper.h"
#include "cangjie/CHIR/IR/AttributeInfo.h"
#include "cangjie/CHIR/IR/CHIRBuilder.h"
#include "cangjie/CHIR/IR/DebugLocation.h"
#include "cangjie/CHIR/IR/Expression/Expression.h"
#include "cangjie/CHIR/IR/Expression/Terminator.h"
#include "cangjie/CHIR/IR/IntrinsicKind.h"
#include "cangjie/CHIR/IR/Package.h"
#include "cangjie/CHIR/IR/Type/CustomTypeDef.h"
#include "cangjie/CHIR/IR/Value/LiteralValue.h"
#include "cangjie/Utils/ConstantsUtils.h"

using namespace Cangjie;
using namespace Cangjie::CHIR;

class ConstAnalysisIRFixture : public ::testing::Test {
protected:
    void SetUp() override
    {
        nameMap = std::make_unique<std::unordered_map<unsigned int, std::string>>();
        cctx = std::make_unique<CHIRContext>(nameMap.get());
        builder = std::make_unique<CHIRBuilder>(*cctx);
        package = builder->CreatePackage("default");
        // GetStringTy() looks up std.core::String in the current package.
        auto* strDef = builder->CreateStruct(INVALID_LOCATION, "String", "String", "std.core", false);
        auto* strTy = builder->GetType<StructType>(strDef);
        strDef->SetType(*strTy);
        diag = std::make_unique<DiagnosticEngine>();
        wrapper = std::make_unique<ConstAnalysisWrapper>(*builder);
        funcCounter = 0;
    }

    Function* NewFunc()
    {
        auto name = "cf" + std::to_string(funcCounter++);
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

    Block* NewBlock()
    {
        auto* b = builder->CreateBlock(curFunc->GetBody());
        return b;
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

    Value* LitRune(char32_t v)
    {
        auto* c = builder->CreateConstantExpression<RuneLiteral>(builder->GetRuneTy(), curBlock, v);
        curBlock->AppendExpression(c);
        return c->GetResult();
    }

    Value* LitStr(const std::string& v)
    {
        auto* c = builder->CreateConstantExpression<StringLiteral>(builder->GetStringTy(), curBlock, v);
        curBlock->AppendExpression(c);
        return c->GetResult();
    }

    void FinishWithExit()
    {
        curBlock->AppendExpression(builder->CreateTerminator<Exit>(curBlock));
    }

    std::unique_ptr<Results<ConstDomain>> Analyse(Function* func)
    {
        return wrapper->RunOnFunc(func, false, *diag);
    }

    // Re-run transfer after SetToStable so Raise*/GenerateTypeRangePrompt execute.
    // Expressions that may diagnose need a non-zero DebugLocation (DiagnoseRefactor ICEs on zero range).
    std::unique_ptr<Results<ConstDomain>> AnalyseWithDiagnostics(Function* func)
    {
        static const DebugLocation kLoc("ut.cj", 1, {1, 1}, {1, 10});
        for (auto* bb : func->GetBody()->GetBlocks()) {
            for (auto* e : bb->GetExpressions()) {
                if (e->GetDebugLocation().GetBeginPos().line == 0) {
                    e->SetDebugLocation(kLoc);
                }
            }
        }
        auto results = wrapper->RunOnFunc(func, false, *diag);
        if (results) {
            results->VisitWith(
                [](const ConstDomain&, Expression*, size_t) {}, [](const ConstDomain&, Expression*, size_t) {},
                [](const ConstDomain&, Expression*, std::optional<Block*>) {});
        }
        return results;
    }

    std::unique_ptr<Results<ConstPoolDomain>> AnalysePool(Function* func)
    {
        static const DebugLocation kLoc("ut.cj", 1, {1, 1}, {1, 10});
        for (auto* bb : func->GetBody()->GetBlocks()) {
            for (auto* e : bb->GetExpressions()) {
                if (e->GetDebugLocation().GetBeginPos().line == 0) {
                    e->SetDebugLocation(kLoc);
                }
            }
        }
        auto results = wrapper->RunOnFuncWithPool(func, false, *diag);
        if (results) {
            results->VisitWith(
                [](const ConstPoolDomain&, Expression*, size_t) {}, [](const ConstPoolDomain&, Expression*, size_t) {},
                [](const ConstPoolDomain&, Expression*, std::optional<Block*>) {});
        }
        return results;
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
