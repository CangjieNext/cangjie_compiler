// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * Migrated from LLT CHIROpt/ConstAnalysis cases that used removed debugCHIROpt.
 */

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

namespace {

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

} // namespace

TEST_F(ConstAnalysisFixture, activeStateNode_multi_for_in_cj)
{
    // from LLT ConstAnalysis/activeStateNode/multi_for_in.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "1");
        auto* rhs = Lit("Int64", "1");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::ADD, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int64", "2");
        auto* rhs = Lit("Int64", "3");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::GT, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Add=2", "GT=false"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_add_float_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/add_float.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Float64", "9e7");
        auto* rhs = Lit("Float64", "8e3");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Float64"), BinaryExprKind::ADD, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Float32", "1111.00");
        auto* rhs = Lit("Float32", "-2222.111");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Float32"), BinaryExprKind::ADD, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Add=90008000.000000", "Add=-1111.111084"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_add_int_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/add_int.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "9000");
        auto* rhs = Lit("Int64", "7000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::ADD, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int32", "1111");
        auto* rhs = Lit("Int32", "2222");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int32"), BinaryExprKind::ADD, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int16", "1111");
        auto* rhs = Lit("Int16", "30000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int16"), BinaryExprKind::ADD, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int8", "100");
        auto* rhs = Lit("Int8", "3");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int8"), BinaryExprKind::ADD, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Add=103", "Add=31111", "Add=3333", "Add=16000"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_add_int_saturating_1_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/add_int_saturating_1.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "9223372036854775806");
        auto* rhs = Lit("Int64", "10");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::ADD, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int32", "2147483646");
        auto* rhs = Lit("Int32", "12");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int32"), BinaryExprKind::ADD, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int16", "32767");
        auto* rhs = Lit("Int16", "3");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int16"), BinaryExprKind::ADD, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int8", "127");
        auto* rhs = Lit("Int8", "4");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int8"), BinaryExprKind::ADD, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Add=127", "Add=32767", "Add=2147483647", "Add=9223372036854775807"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_add_int_saturating_2_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/add_int_saturating_2.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "-9223372036854775800");
        auto* rhs = Lit("Int64", "-10");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::ADD, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int32", "-2147483644");
        auto* rhs = Lit("Int32", "-12");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int32"), BinaryExprKind::ADD, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int16", "-32767");
        auto* rhs = Lit("Int16", "-3");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int16"), BinaryExprKind::ADD, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int8", "-127");
        auto* rhs = Lit("Int8", "-4");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int8"), BinaryExprKind::ADD, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Add=-128", "Add=-32768", "Add=-2147483648", "Add=-9223372036854775808"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_add_int_wrapping_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/add_int_wrapping.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "9223372036854775800");
        auto* rhs = Lit("Int64", "10");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::ADD, lhs, rhs, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int32", "2147483645");
        auto* rhs = Lit("Int32", "17");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int32"), BinaryExprKind::ADD, lhs, rhs, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int16", "32767");
        auto* rhs = Lit("Int16", "17");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int16"), BinaryExprKind::ADD, lhs, rhs, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int8", "127");
        auto* rhs = Lit("Int8", "3");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int8"), BinaryExprKind::ADD, lhs, rhs, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Add=-126", "Add=-32752", "Add=-2147483634", "Add=-9223372036854775806"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_add_uint_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/add_uint.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("UInt64", "9000");
        auto* rhs = Lit("UInt64", "7000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt64"), BinaryExprKind::ADD, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("UInt32", "1111");
        auto* rhs = Lit("UInt32", "2222");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt32"), BinaryExprKind::ADD, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("UInt16", "1111");
        auto* rhs = Lit("UInt16", "60000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt16"), BinaryExprKind::ADD, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("UInt8", "200");
        auto* rhs = Lit("UInt8", "3");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt8"), BinaryExprKind::ADD, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Add=203", "Add=61111", "Add=3333", "Add=16000"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_add_uint_saturating_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/add_uint_saturating.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("UInt64", "18446744073709551614");
        auto* rhs = Lit("UInt64", "2");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt64"), BinaryExprKind::ADD, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("UInt32", "4294967294");
        auto* rhs = Lit("UInt32", "7");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt32"), BinaryExprKind::ADD, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("UInt16", "65534");
        auto* rhs = Lit("UInt16", "17");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt16"), BinaryExprKind::ADD, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("UInt8", "254");
        auto* rhs = Lit("UInt8", "3");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt8"), BinaryExprKind::ADD, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Add=255", "Add=65535", "Add=4294967295", "Add=18446744073709551615"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_add_uint_wrapping_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/add_uint_wrapping.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("UInt64", "18446744073709551614");
        auto* rhs = Lit("UInt64", "10");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt64"), BinaryExprKind::ADD, lhs, rhs, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("UInt32", "4294967294");
        auto* rhs = Lit("UInt32", "7");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt32"), BinaryExprKind::ADD, lhs, rhs, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("UInt16", "65534");
        auto* rhs = Lit("UInt16", "17");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt16"), BinaryExprKind::ADD, lhs, rhs, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("UInt8", "254");
        auto* rhs = Lit("UInt8", "3");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt8"), BinaryExprKind::ADD, lhs, rhs, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Add=1", "Add=15", "Add=5", "Add=8"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_div_float_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/div_float.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Float64", "6.6");
        auto* rhs = Lit("Float64", "1.1");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Float64"), BinaryExprKind::DIV, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Float32", "1111.00");
        auto* rhs = Lit("Float32", "-2222.000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Float32"), BinaryExprKind::DIV, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Div=-0.500000", "Div=6.000000"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_div_int_1_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/div_int_1.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "7");
        auto* rhs = Lit("Int64", "3");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::DIV, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int32", "-7");
        auto* rhs = Lit("Int32", "3");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int32"), BinaryExprKind::DIV, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Div=-2", "Div=2"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_div_int_2_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/div_int_2.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int16", "7");
        auto* rhs = Lit("Int16", "-3");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int16"), BinaryExprKind::DIV, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int8", "-7");
        auto* rhs = Lit("Int8", "-3");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int8"), BinaryExprKind::DIV, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Div=2", "Div=-2"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_div_int_saturating_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/div_int_saturating.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "-9223372036854775808");
        auto* rhs = Lit("Int64", "-1");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::DIV, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int32", "-2147483648");
        auto* rhs = Lit("Int32", "-1");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int32"), BinaryExprKind::DIV, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int16", "-32768");
        auto* rhs = Lit("Int16", "-1");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int16"), BinaryExprKind::DIV, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int8", "-128");
        auto* rhs = Lit("Int8", "-1");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int8"), BinaryExprKind::DIV, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Div=127", "Div=32767", "Div=2147483647", "Div=9223372036854775807"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_div_int_wrapping_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/div_int_wrapping.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "-9223372036854775808");
        auto* rhs = Lit("Int64", "-1");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::DIV, lhs, rhs, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int32", "-2147483648");
        auto* rhs = Lit("Int32", "-1");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int32"), BinaryExprKind::DIV, lhs, rhs, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int16", "-32768");
        auto* rhs = Lit("Int16", "-1");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int16"), BinaryExprKind::DIV, lhs, rhs, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int8", "-128");
        auto* rhs = Lit("Int8", "-1");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int8"), BinaryExprKind::DIV, lhs, rhs, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Div=-128", "Div=-32768", "Div=-2147483648", "Div=-9223372036854775808"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_div_trivial_1_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/div_trivial_1.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "0");
        auto* rhs = Lit("Int64", "999");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::DIV, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Div=0"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_div_uint_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/div_uint.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("UInt64", "9000");
        auto* rhs = Lit("UInt64", "1000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt64"), BinaryExprKind::DIV, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("UInt32", "11120");
        auto* rhs = Lit("UInt32", "20");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt32"), BinaryExprKind::DIV, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("UInt16", "121");
        auto* rhs = Lit("UInt16", "11");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt16"), BinaryExprKind::DIV, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("UInt8", "12");
        auto* rhs = Lit("UInt8", "3");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt8"), BinaryExprKind::DIV, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Div=4", "Div=11", "Div=556", "Div=9"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_exp_int_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/exp_int.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "7");
        auto* rhs = Lit("UInt64", "2");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::EXP, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Exp=49"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_exp_int_saturating_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/exp_int_saturating.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "92233720368547758");
        auto* rhs = Lit("UInt64", "2");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::EXP, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int64", "-92233720368547757");
        auto* rhs = Lit("UInt64", "2");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::EXP, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int64", "9223372036854");
        auto* rhs = Lit("UInt64", "2038495435");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::EXP, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int64", "-9223372036854");
        auto* rhs = Lit("UInt64", "2038495437");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::EXP, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int64", "121");
        auto* rhs = Lit("UInt64", "18446744073709551525");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::EXP, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int64", "-121");
        auto* rhs = Lit("UInt64", "18446744073709551525");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::EXP, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Exp=9223372036854775807", "Exp=9223372036854775807", "Exp=9223372036854775807", "Exp=-9223372036854775808", "Exp=9223372036854775807", "Exp=-9223372036854775808"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_exp_int_wrapping_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/exp_int_wrapping.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "92233720368547758");
        auto* rhs = Lit("UInt64", "2");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::EXP, lhs, rhs, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int64", "-92233720368547757");
        auto* rhs = Lit("UInt64", "2");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::EXP, lhs, rhs, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int64", "9223372036854");
        auto* rhs = Lit("UInt64", "2038495435");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::EXP, lhs, rhs, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int64", "-9223372036854");
        auto* rhs = Lit("UInt64", "2038495437");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::EXP, lhs, rhs, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int64", "121");
        auto* rhs = Lit("UInt64", "18446744073709551525");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::EXP, lhs, rhs, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int64", "-121");
        auto* rhs = Lit("UInt64", "18446744073709551525");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::EXP, lhs, rhs, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Exp=-3881194953108489660", "Exp=-4065662393845585175", "Exp=0", "Exp=0", "Exp=-8386622389663533095", "Exp=8386622389663533095"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_exp_trivial_1_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/exp_trivial_1.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "0");
        auto* rhs = Lit("UInt64", "999");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::EXP, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Exp=0"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_exp_trivial_2_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/exp_trivial_2.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "1");
        auto* rhs = Lit("UInt64", "999");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::EXP, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Exp=1"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_exp_trivial_3_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/exp_trivial_3.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "999");
        auto* rhs = Lit("UInt64", "0");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::EXP, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Exp=1"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_exp_trivial_4_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/exp_trivial_4.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "0");
        auto* rhs = Lit("UInt64", "0");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::EXP, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Exp=1"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_mod_int_1_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/mod_int_1.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "7");
        auto* rhs = Lit("Int64", "3");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::MOD, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int32", "-7");
        auto* rhs = Lit("Int32", "3");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int32"), BinaryExprKind::MOD, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Mod=-1", "Mod=1"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_mod_int_2_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/mod_int_2.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int16", "7");
        auto* rhs = Lit("Int16", "-3");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int16"), BinaryExprKind::MOD, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int8", "-7");
        auto* rhs = Lit("Int8", "-3");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int8"), BinaryExprKind::MOD, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Mod=-1", "Mod=1"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_mod_trivial_1_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/mod_trivial_1.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "999");
        auto* rhs = Lit("Int64", "1");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::MOD, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Mod=0"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_mod_trivial_2_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/mod_trivial_2.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "0");
        auto* rhs = Lit("Int64", "999");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::MOD, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Mod=0"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_mod_uint_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/mod_uint.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("UInt64", "7");
        auto* rhs = Lit("UInt64", "6");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt64"), BinaryExprKind::MOD, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("UInt32", "17");
        auto* rhs = Lit("UInt32", "9");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt32"), BinaryExprKind::MOD, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("UInt16", "121");
        auto* rhs = Lit("UInt16", "11");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt16"), BinaryExprKind::MOD, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("UInt8", "12");
        auto* rhs = Lit("UInt8", "10");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt8"), BinaryExprKind::MOD, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Mod=2", "Mod=0", "Mod=8", "Mod=1"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_mul_float_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/mul_float.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Float64", "9e7");
        auto* rhs = Lit("Float64", "8e3");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Float64"), BinaryExprKind::MUL, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Float32", "1111.00");
        auto* rhs = Lit("Float32", "-2222.111");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Float32"), BinaryExprKind::MUL, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Mul=-2468765.500000", "Mul=720000000000.000000"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_mul_int_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/mul_int.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "9000");
        auto* rhs = Lit("Int64", "7000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::MUL, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int32", "-11110");
        auto* rhs = Lit("Int32", "22");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int32"), BinaryExprKind::MUL, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int16", "-111");
        auto* rhs = Lit("Int16", "-60");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int16"), BinaryExprKind::MUL, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int8", "30");
        auto* rhs = Lit("Int8", "3");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int8"), BinaryExprKind::MUL, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Mul=90", "Mul=6660", "Mul=-244420", "Mul=63000000"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_mul_int_saturating_1_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/mul_int_saturating_1.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "1044674407370955161");
        auto* rhs = Lit("Int64", "11");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::MUL, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int32", "2000000000");
        auto* rhs = Lit("Int32", "7");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int32"), BinaryExprKind::MUL, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int16", "6003");
        auto* rhs = Lit("Int16", "17");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int16"), BinaryExprKind::MUL, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int8", "90");
        auto* rhs = Lit("Int8", "3");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int8"), BinaryExprKind::MUL, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Mul=127", "Mul=32767", "Mul=2147483647", "Mul=9223372036854775807"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_mul_int_saturating_2_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/mul_int_saturating_2.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "1044674407370955161");
        auto* rhs = Lit("Int64", "-11");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::MUL, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int32", "2000000000");
        auto* rhs = Lit("Int32", "-7");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int32"), BinaryExprKind::MUL, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int16", "6003");
        auto* rhs = Lit("Int16", "-17");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int16"), BinaryExprKind::MUL, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int8", "90");
        auto* rhs = Lit("Int8", "-3");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int8"), BinaryExprKind::MUL, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Mul=-128", "Mul=-32768", "Mul=-2147483648", "Mul=-9223372036854775808"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_mul_int_wrapping_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/mul_int_wrapping.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "1044674407370955161");
        auto* rhs = Lit("Int64", "-11");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::MUL, lhs, rhs, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int32", "2000000000");
        auto* rhs = Lit("Int32", "-7");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int32"), BinaryExprKind::MUL, lhs, rhs, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int16", "6003");
        auto* rhs = Lit("Int16", "-17");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int16"), BinaryExprKind::MUL, lhs, rhs, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int8", "90");
        auto* rhs = Lit("Int8", "-3");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int8"), BinaryExprKind::MUL, lhs, rhs, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Mul=-14", "Mul=29021", "Mul=-1115098112", "Mul=6955325592629044845"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_mul_trivial_1_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/mul_trivial_1.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "0");
        auto* rhs = Lit("Int64", "999");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::MUL, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Mul=0"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_mul_trivial_2_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/mul_trivial_2.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "999");
        auto* rhs = Lit("Int64", "0");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::MUL, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Mul=0"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_mul_uint_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/mul_uint.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("UInt64", "9000");
        auto* rhs = Lit("UInt64", "7000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt64"), BinaryExprKind::MUL, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("UInt32", "11110");
        auto* rhs = Lit("UInt32", "22");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt32"), BinaryExprKind::MUL, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("UInt16", "111");
        auto* rhs = Lit("UInt16", "60");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt16"), BinaryExprKind::MUL, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("UInt8", "30");
        auto* rhs = Lit("UInt8", "3");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt8"), BinaryExprKind::MUL, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Mul=90", "Mul=6660", "Mul=244420", "Mul=63000000"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_mul_uint_saturating_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/mul_uint_saturating.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("UInt64", "10446744073709551614");
        auto* rhs = Lit("UInt64", "11");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt64"), BinaryExprKind::MUL, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("UInt32", "2000000000");
        auto* rhs = Lit("UInt32", "7");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt32"), BinaryExprKind::MUL, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("UInt16", "60034");
        auto* rhs = Lit("UInt16", "17");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt16"), BinaryExprKind::MUL, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("UInt8", "90");
        auto* rhs = Lit("UInt8", "3");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt8"), BinaryExprKind::MUL, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Mul=255", "Mul=65535", "Mul=4294967295", "Mul=18446744073709551615"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_mul_uint_wrapping_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/mul_uint_wrapping.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("UInt64", "10446744073709551614");
        auto* rhs = Lit("UInt64", "11");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt64"), BinaryExprKind::MUL, lhs, rhs, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("UInt32", "2000000000");
        auto* rhs = Lit("UInt32", "7");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt32"), BinaryExprKind::MUL, lhs, rhs, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("UInt16", "60034");
        auto* rhs = Lit("UInt16", "17");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt16"), BinaryExprKind::MUL, lhs, rhs, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("UInt8", "90");
        auto* rhs = Lit("UInt8", "3");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt8"), BinaryExprKind::MUL, lhs, rhs, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Mul=14", "Mul=37538", "Mul=1115098112", "Mul=4233720368547758058"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_neg_float_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/neg_float.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* operand = Lit("Float64", "9000.002");
        auto* expr = builder->CreateExpression<UnaryExpression>(
            Ty("Float64"), UnaryExprKind::NEG, operand, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* operand = Lit("Float32", "-1111e3");
        auto* expr = builder->CreateExpression<UnaryExpression>(
            Ty("Float32"), UnaryExprKind::NEG, operand, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Neg=1111000.000000", "Neg=-9000.002000"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_neg_int_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/neg_int.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* operand = Lit("Int64", "9000");
        auto* expr = builder->CreateExpression<UnaryExpression>(
            Ty("Int64"), UnaryExprKind::NEG, operand, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* operand = Lit("Int32", "1111");
        auto* expr = builder->CreateExpression<UnaryExpression>(
            Ty("Int32"), UnaryExprKind::NEG, operand, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* operand = Lit("Int16", "-30000");
        auto* expr = builder->CreateExpression<UnaryExpression>(
            Ty("Int16"), UnaryExprKind::NEG, operand, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* operand = Lit("Int8", "3");
        auto* expr = builder->CreateExpression<UnaryExpression>(
            Ty("Int8"), UnaryExprKind::NEG, operand, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Neg=-3", "Neg=30000", "Neg=-1111", "Neg=-9000"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_neg_int_saturating_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/neg_int_saturating.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* operand = Lit("Int64", "-9223372036854775808");
        auto* expr = builder->CreateExpression<UnaryExpression>(
            Ty("Int64"), UnaryExprKind::NEG, operand, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* operand = Lit("Int32", "-2147483648");
        auto* expr = builder->CreateExpression<UnaryExpression>(
            Ty("Int32"), UnaryExprKind::NEG, operand, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* operand = Lit("Int16", "-32768");
        auto* expr = builder->CreateExpression<UnaryExpression>(
            Ty("Int16"), UnaryExprKind::NEG, operand, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* operand = Lit("Int8", "-128");
        auto* expr = builder->CreateExpression<UnaryExpression>(
            Ty("Int8"), UnaryExprKind::NEG, operand, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Neg=127", "Neg=32767", "Neg=2147483647", "Neg=9223372036854775807"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_neg_int_wrapping_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/neg_int_wrapping.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* operand = Lit("Int64", "-9223372036854775808");
        auto* expr = builder->CreateExpression<UnaryExpression>(
            Ty("Int64"), UnaryExprKind::NEG, operand, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* operand = Lit("Int32", "-2147483648");
        auto* expr = builder->CreateExpression<UnaryExpression>(
            Ty("Int32"), UnaryExprKind::NEG, operand, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* operand = Lit("Int16", "-32768");
        auto* expr = builder->CreateExpression<UnaryExpression>(
            Ty("Int16"), UnaryExprKind::NEG, operand, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* operand = Lit("Int8", "-128");
        auto* expr = builder->CreateExpression<UnaryExpression>(
            Ty("Int8"), UnaryExprKind::NEG, operand, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Neg=-128", "Neg=-32768", "Neg=-2147483648", "Neg=-9223372036854775808"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_sub_float_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/sub_float.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Float64", "9e7");
        auto* rhs = Lit("Float64", "8e3");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Float64"), BinaryExprKind::SUB, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Float64", "8e3");
        auto* rhs = Lit("Float64", "9e7");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Float64"), BinaryExprKind::SUB, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Float32", "1111.00");
        auto* rhs = Lit("Float32", "-2222.111");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Float32"), BinaryExprKind::SUB, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Float32", "-2222.111");
        auto* rhs = Lit("Float32", "1111.00");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Float32"), BinaryExprKind::SUB, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Sub=3333.111084", "Sub=-3333.111084", "Sub=89992000.000000", "Sub=-89992000.000000"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_sub_int_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/sub_int.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "9000");
        auto* rhs = Lit("Int64", "7000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::SUB, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int32", "2222");
        auto* rhs = Lit("Int32", "1111");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int32"), BinaryExprKind::SUB, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int16", "11111");
        auto* rhs = Lit("Int16", "6000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int16"), BinaryExprKind::SUB, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int8", "100");
        auto* rhs = Lit("Int8", "3");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int8"), BinaryExprKind::SUB, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Sub=97", "Sub=5111", "Sub=1111", "Sub=2000"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_sub_int_saturating_1_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/sub_int_saturating_1.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "9000");
        auto* rhs = Lit("Int64", "-9223372036854775800");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::SUB, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int32", "1111");
        auto* rhs = Lit("Int32", "-2147483647");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int32"), BinaryExprKind::SUB, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int16", "100");
        auto* rhs = Lit("Int16", "-32767");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int16"), BinaryExprKind::SUB, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int8", "100");
        auto* rhs = Lit("Int8", "-127");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int8"), BinaryExprKind::SUB, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Sub=127", "Sub=32767", "Sub=2147483647", "Sub=9223372036854775807"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_sub_int_saturating_2_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/sub_int_saturating_2.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "-9000");
        auto* rhs = Lit("Int64", "9223372036854775800");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::SUB, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int32", "-1111");
        auto* rhs = Lit("Int32", "2147483647");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int32"), BinaryExprKind::SUB, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int16", "-100");
        auto* rhs = Lit("Int16", "32767");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int16"), BinaryExprKind::SUB, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int8", "-100");
        auto* rhs = Lit("Int8", "127");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int8"), BinaryExprKind::SUB, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Sub=-128", "Sub=-32768", "Sub=-2147483648", "Sub=-9223372036854775808"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_sub_int_wrapping_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/sub_int_wrapping.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "9000");
        auto* rhs = Lit("Int64", "-9223372036854775800");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::SUB, lhs, rhs, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int32", "1111");
        auto* rhs = Lit("Int32", "-2147483647");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int32"), BinaryExprKind::SUB, lhs, rhs, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int16", "100");
        auto* rhs = Lit("Int16", "-32767");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int16"), BinaryExprKind::SUB, lhs, rhs, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("Int8", "100");
        auto* rhs = Lit("Int8", "-127");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int8"), BinaryExprKind::SUB, lhs, rhs, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Sub=-29", "Sub=-32669", "Sub=-2147482538", "Sub=-9223372036854766816"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_sub_uint_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/sub_uint.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("UInt64", "9000");
        auto* rhs = Lit("UInt64", "7000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt64"), BinaryExprKind::SUB, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("UInt32", "2222");
        auto* rhs = Lit("UInt32", "1111");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt32"), BinaryExprKind::SUB, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("UInt16", "11111");
        auto* rhs = Lit("UInt16", "6000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt16"), BinaryExprKind::SUB, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("UInt8", "200");
        auto* rhs = Lit("UInt8", "3");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt8"), BinaryExprKind::SUB, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Sub=197", "Sub=5111", "Sub=1111", "Sub=2000"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_sub_uint_saturating_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/sub_uint_saturating.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("UInt64", "9000");
        auto* rhs = Lit("UInt64", "10000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt64"), BinaryExprKind::SUB, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("UInt32", "1111");
        auto* rhs = Lit("UInt32", "2222");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt32"), BinaryExprKind::SUB, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("UInt16", "11111");
        auto* rhs = Lit("UInt16", "60000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt16"), BinaryExprKind::SUB, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("UInt8", "200");
        auto* rhs = Lit("UInt8", "240");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt8"), BinaryExprKind::SUB, lhs, rhs, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Sub=0", "Sub=0", "Sub=0", "Sub=0"});
}

TEST_F(ConstAnalysisFixture, arithmeticOp_sub_uint_wrapping_cj)
{
    // from LLT ConstAnalysis/arithmeticOp/sub_uint_wrapping.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("UInt64", "30");
        auto* rhs = Lit("UInt64", "18446744073709551610");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt64"), BinaryExprKind::SUB, lhs, rhs, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("UInt32", "10");
        auto* rhs = Lit("UInt32", "4294967293");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt32"), BinaryExprKind::SUB, lhs, rhs, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("UInt16", "17");
        auto* rhs = Lit("UInt16", "65530");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt16"), BinaryExprKind::SUB, lhs, rhs, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* lhs = Lit("UInt8", "3");
        auto* rhs = Lit("UInt8", "255");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt8"), BinaryExprKind::SUB, lhs, rhs, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Sub=4", "Sub=23", "Sub=13", "Sub=36"});
}

TEST_F(ConstAnalysisFixture, bitwiseOp_bitand_int64_cj)
{
    // from LLT ConstAnalysis/bitwiseOp/bitand_int64.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "-10");
        auto* rhs = Lit("Int64", "15");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::BITAND, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"BitAnd=6"});
}

TEST_F(ConstAnalysisFixture, bitwiseOp_bitand_uint64_cj)
{
    // from LLT ConstAnalysis/bitwiseOp/bitand_uint64.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("UInt64", "10");
        auto* rhs = Lit("UInt64", "15");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt64"), BinaryExprKind::BITAND, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"BitAnd=10"});
}

TEST_F(ConstAnalysisFixture, bitwiseOp_bitor_int64_cj)
{
    // from LLT ConstAnalysis/bitwiseOp/bitor_int64.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "-10");
        auto* rhs = Lit("Int64", "-15");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::BITOR, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"BitOr=-9"});
}

TEST_F(ConstAnalysisFixture, bitwiseOp_bitor_uint64_cj)
{
    // from LLT ConstAnalysis/bitwiseOp/bitor_uint64.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("UInt64", "10");
        auto* rhs = Lit("UInt64", "15");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt64"), BinaryExprKind::BITOR, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"BitOr=15"});
}

TEST_F(ConstAnalysisFixture, bitwiseOp_bitxor_int64_cj)
{
    // from LLT ConstAnalysis/bitwiseOp/bitxor_int64.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "10");
        auto* rhs = Lit("Int64", "-15");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::BITXOR, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"BitXor=-5"});
}

TEST_F(ConstAnalysisFixture, bitwiseOp_bitxor_uint64_cj)
{
    // from LLT ConstAnalysis/bitwiseOp/bitxor_uint64.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("UInt64", "10");
        auto* rhs = Lit("UInt64", "15");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt64"), BinaryExprKind::BITXOR, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"BitXor=5"});
}

TEST_F(ConstAnalysisFixture, bitwiseOp_lshift_int64_1_cj)
{
    // from LLT ConstAnalysis/bitwiseOp/lshift_int64_1.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "-30");
        auto* rhs = Lit("Int64", "2");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::LSHIFT, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"LShift=-120"});
}

TEST_F(ConstAnalysisFixture, bitwiseOp_lshift_int64_2_cj)
{
    // from LLT ConstAnalysis/bitwiseOp/lshift_int64_2.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "30");
        auto* rhs = Lit("Int64", "2");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::LSHIFT, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"LShift=120"});
}

TEST_F(ConstAnalysisFixture, bitwiseOp_lshift_uint64_1_cj)
{
    // from LLT ConstAnalysis/bitwiseOp/lshift_uint64_1.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("UInt64", "10");
        auto* rhs = Lit("Int64", "1");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt64"), BinaryExprKind::LSHIFT, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"LShift=20"});
}

TEST_F(ConstAnalysisFixture, bitwiseOp_lshift_uint64_2_cj)
{
    // from LLT ConstAnalysis/bitwiseOp/lshift_uint64_2.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("UInt64", "30");
        auto* rhs = Lit("Int64", "3");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt64"), BinaryExprKind::LSHIFT, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"LShift=240"});
}

TEST_F(ConstAnalysisFixture, bitwiseOp_not_int64_cj)
{
    // from LLT ConstAnalysis/bitwiseOp/not_int64.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* operand = Lit("Int64", "10");
        auto* expr = builder->CreateExpression<UnaryExpression>(
            Ty("Int64"), UnaryExprKind::BITNOT, operand, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"BitNot=-11"});
}

TEST_F(ConstAnalysisFixture, bitwiseOp_not_uint64_cj)
{
    // from LLT ConstAnalysis/bitwiseOp/not_uint64.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* operand = Lit("UInt64", "10");
        auto* expr = builder->CreateExpression<UnaryExpression>(
            Ty("UInt64"), UnaryExprKind::BITNOT, operand, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"BitNot=18446744073709551605"});
}

TEST_F(ConstAnalysisFixture, bitwiseOp_rshift_int64_1_cj)
{
    // from LLT ConstAnalysis/bitwiseOp/rshift_int64_1.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "81");
        auto* rhs = Lit("Int64", "2");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::RSHIFT, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"RShift=20"});
}

TEST_F(ConstAnalysisFixture, bitwiseOp_rshift_int64_2_cj)
{
    // from LLT ConstAnalysis/bitwiseOp/rshift_int64_2.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "-80");
        auto* rhs = Lit("Int64", "2");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Int64"), BinaryExprKind::RSHIFT, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"RShift=-20"});
}

TEST_F(ConstAnalysisFixture, bitwiseOp_rshift_uint64_1_cj)
{
    // from LLT ConstAnalysis/bitwiseOp/rshift_uint64_1.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("UInt64", "10");
        auto* rhs = Lit("Int64", "1");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt64"), BinaryExprKind::RSHIFT, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"RShift=5"});
}

TEST_F(ConstAnalysisFixture, bitwiseOp_rshift_uint64_2_cj)
{
    // from LLT ConstAnalysis/bitwiseOp/rshift_uint64_2.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("UInt64", "30");
        auto* rhs = Lit("Int64", "1");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt64"), BinaryExprKind::RSHIFT, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"RShift=15"});
}

TEST_F(ConstAnalysisFixture, relationalOp_eq_float64_1_cj)
{
    // from LLT ConstAnalysis/relationalOp/eq_float64_1.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Float64", "-1024.512");
        auto* rhs = Lit("Float64", "-1024.512");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::EQUAL, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Equal=true"});
}

TEST_F(ConstAnalysisFixture, relationalOp_eq_float64_2_cj)
{
    // from LLT ConstAnalysis/relationalOp/eq_float64_2.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Float64", "-1024.512");
        auto* rhs = Lit("Float64", "-1024.513000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::EQUAL, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Equal=false"});
}

TEST_F(ConstAnalysisFixture, relationalOp_eq_int64_1_cj)
{
    // from LLT ConstAnalysis/relationalOp/eq_int64_1.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "-6000");
        auto* rhs = Lit("Int64", "-6000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::EQUAL, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Equal=true"});
}

TEST_F(ConstAnalysisFixture, relationalOp_eq_int64_2_cj)
{
    // from LLT ConstAnalysis/relationalOp/eq_int64_2.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "-6000");
        auto* rhs = Lit("Int64", "7999");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::EQUAL, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Equal=false"});
}

TEST_F(ConstAnalysisFixture, relationalOp_eq_uint64_1_cj)
{
    // from LLT ConstAnalysis/relationalOp/eq_uint64_1.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("UInt64", "6000");
        auto* rhs = Lit("UInt64", "7000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::EQUAL, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Equal=false"});
}

TEST_F(ConstAnalysisFixture, relationalOp_eq_uint64_2_cj)
{
    // from LLT ConstAnalysis/relationalOp/eq_uint64_2.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("UInt64", "6000");
        auto* rhs = Lit("UInt64", "6000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::EQUAL, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Equal=true"});
}

TEST_F(ConstAnalysisFixture, relationalOp_eq_unit_cj)
{
    // from LLT ConstAnalysis/relationalOp/eq_unit.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = builder->CreateConstantExpression<UnitLiteral>(Ty("Unit"), curBlock);
        curBlock->AppendExpression(lhs);
        auto* rhs = builder->CreateConstantExpression<UnitLiteral>(Ty("Unit"), curBlock);
        curBlock->AppendExpression(rhs);
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::EQUAL, lhs->GetResult(), rhs->GetResult(), OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Equal=true"});
}

TEST_F(ConstAnalysisFixture, relationalOp_ge_float_1_cj)
{
    // from LLT ConstAnalysis/relationalOp/ge_float_1.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Float64", "2e10");
        auto* rhs = Lit("Float64", "1e10");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::GE, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"GE=true"});
}

TEST_F(ConstAnalysisFixture, relationalOp_ge_float_2_cj)
{
    // from LLT ConstAnalysis/relationalOp/ge_float_2.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Float64", "2e10");
        auto* rhs = Lit("Float64", "2e10");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::GE, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"GE=true"});
}

TEST_F(ConstAnalysisFixture, relationalOp_ge_float_3_cj)
{
    // from LLT ConstAnalysis/relationalOp/ge_float_3.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Float64", "-2e10");
        auto* rhs = Lit("Float64", "1e10");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::GE, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"GE=false"});
}

TEST_F(ConstAnalysisFixture, relationalOp_ge_int64_1_cj)
{
    // from LLT ConstAnalysis/relationalOp/ge_int64_1.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "7001");
        auto* rhs = Lit("Int64", "7000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::GE, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"GE=true"});
}

TEST_F(ConstAnalysisFixture, relationalOp_ge_int64_2_cj)
{
    // from LLT ConstAnalysis/relationalOp/ge_int64_2.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "7000");
        auto* rhs = Lit("Int64", "7000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::GE, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"GE=true"});
}

TEST_F(ConstAnalysisFixture, relationalOp_ge_int64_3_cj)
{
    // from LLT ConstAnalysis/relationalOp/ge_int64_3.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "-100000");
        auto* rhs = Lit("Int64", "7000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::GE, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"GE=false"});
}

TEST_F(ConstAnalysisFixture, relationalOp_ge_uint64_1_cj)
{
    // from LLT ConstAnalysis/relationalOp/ge_uint64_1.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("UInt64", "6000");
        auto* rhs = Lit("UInt64", "7000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::GE, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"GE=false"});
}

TEST_F(ConstAnalysisFixture, relationalOp_ge_uint64_2_cj)
{
    // from LLT ConstAnalysis/relationalOp/ge_uint64_2.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("UInt64", "6000");
        auto* rhs = Lit("UInt64", "6000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::GE, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"GE=true"});
}

TEST_F(ConstAnalysisFixture, relationalOp_ge_uint64_3_cj)
{
    // from LLT ConstAnalysis/relationalOp/ge_uint64_3.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("UInt64", "9000");
        auto* rhs = Lit("UInt64", "7000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::GE, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"GE=true"});
}

TEST_F(ConstAnalysisFixture, relationalOp_gt_float64_1_cj)
{
    // from LLT ConstAnalysis/relationalOp/gt_float64_1.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Float64", "1111.0");
        auto* rhs = Lit("Float64", "-2000.512");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::GT, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"GT=true"});
}

TEST_F(ConstAnalysisFixture, relationalOp_gt_float64_2_cj)
{
    // from LLT ConstAnalysis/relationalOp/gt_float64_2.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Float64", "-2000.512");
        auto* rhs = Lit("Float64", "-2000.512");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::GT, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"GT=false"});
}

TEST_F(ConstAnalysisFixture, relationalOp_gt_float64_3_cj)
{
    // from LLT ConstAnalysis/relationalOp/gt_float64_3.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Float64", "-2000.512");
        auto* rhs = Lit("Float64", "1111.098");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::GT, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"GT=false"});
}

TEST_F(ConstAnalysisFixture, relationalOp_gt_int64_1_cj)
{
    // from LLT ConstAnalysis/relationalOp/gt_int64_1.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "-2");
        auto* rhs = Lit("Int64", "-2000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::GT, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"GT=true"});
}

TEST_F(ConstAnalysisFixture, relationalOp_gt_int64_2_cj)
{
    // from LLT ConstAnalysis/relationalOp/gt_int64_2.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "-2000");
        auto* rhs = Lit("Int64", "-2000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::GT, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"GT=false"});
}

TEST_F(ConstAnalysisFixture, relationalOp_gt_int64_3_cj)
{
    // from LLT ConstAnalysis/relationalOp/gt_int64_3.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "-30000");
        auto* rhs = Lit("Int64", "-2000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::GT, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"GT=false"});
}

TEST_F(ConstAnalysisFixture, relationalOp_gt_uint64_1_cj)
{
    // from LLT ConstAnalysis/relationalOp/gt_uint64_1.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("UInt64", "6000");
        auto* rhs = Lit("UInt64", "7000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::GT, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"GT=false"});
}

TEST_F(ConstAnalysisFixture, relationalOp_gt_uint64_2_cj)
{
    // from LLT ConstAnalysis/relationalOp/gt_uint64_2.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("UInt64", "6000");
        auto* rhs = Lit("UInt64", "6000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::GT, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"GT=false"});
}

TEST_F(ConstAnalysisFixture, relationalOp_gt_uint64_3_cj)
{
    // from LLT ConstAnalysis/relationalOp/gt_uint64_3.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("UInt64", "9000");
        auto* rhs = Lit("UInt64", "7000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::GT, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"GT=true"});
}

TEST_F(ConstAnalysisFixture, relationalOp_le_float64_1_cj)
{
    // from LLT ConstAnalysis/relationalOp/le_float64_1.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Float64", "-9e8");
        auto* rhs = Lit("Float64", "1.000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::LE, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"LE=true"});
}

TEST_F(ConstAnalysisFixture, relationalOp_le_float64_2_cj)
{
    // from LLT ConstAnalysis/relationalOp/le_float64_2.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Float64", "-9e8");
        auto* rhs = Lit("Float64", "-9e8");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::LE, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"LE=true"});
}

TEST_F(ConstAnalysisFixture, relationalOp_le_float64_3_cj)
{
    // from LLT ConstAnalysis/relationalOp/le_float64_3.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Float64", "9e8");
        auto* rhs = Lit("Float64", "-9e8");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::LE, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"LE=false"});
}

TEST_F(ConstAnalysisFixture, relationalOp_le_int64_1_cj)
{
    // from LLT ConstAnalysis/relationalOp/le_int64_1.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "-99999");
        auto* rhs = Lit("Int64", "7000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::LE, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"LE=true"});
}

TEST_F(ConstAnalysisFixture, relationalOp_le_int64_2_cj)
{
    // from LLT ConstAnalysis/relationalOp/le_int64_2.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "7000");
        auto* rhs = Lit("Int64", "7000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::LE, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"LE=true"});
}

TEST_F(ConstAnalysisFixture, relationalOp_le_int64_3_cj)
{
    // from LLT ConstAnalysis/relationalOp/le_int64_3.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "8999999");
        auto* rhs = Lit("Int64", "7000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::LE, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"LE=false"});
}

TEST_F(ConstAnalysisFixture, relationalOp_le_uint64_1_cj)
{
    // from LLT ConstAnalysis/relationalOp/le_uint64_1.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("UInt64", "9000");
        auto* rhs = Lit("UInt64", "7000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::LE, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"LE=false"});
}

TEST_F(ConstAnalysisFixture, relationalOp_le_uint64_2_cj)
{
    // from LLT ConstAnalysis/relationalOp/le_uint64_2.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("UInt64", "9000");
        auto* rhs = Lit("UInt64", "9000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::LE, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"LE=true"});
}

TEST_F(ConstAnalysisFixture, relationalOp_le_uint64_3_cj)
{
    // from LLT ConstAnalysis/relationalOp/le_uint64_3.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("UInt64", "7000");
        auto* rhs = Lit("UInt64", "9000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::LE, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"LE=true"});
}

TEST_F(ConstAnalysisFixture, relationalOp_lt_float64_1_cj)
{
    // from LLT ConstAnalysis/relationalOp/lt_float64_1.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Float64", "-1.002");
        auto* rhs = Lit("Float64", "7e9");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::LT, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"LT=true"});
}

TEST_F(ConstAnalysisFixture, relationalOp_lt_float64_2_cj)
{
    // from LLT ConstAnalysis/relationalOp/lt_float64_2.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Float64", "-1.002");
        auto* rhs = Lit("Float64", "-1.002");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::LT, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"LT=false"});
}

TEST_F(ConstAnalysisFixture, relationalOp_lt_float64_3_cj)
{
    // from LLT ConstAnalysis/relationalOp/lt_float64_3.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Float64", "-1.001");
        auto* rhs = Lit("Float64", "-1.002");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::LT, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"LT=false"});
}

TEST_F(ConstAnalysisFixture, relationalOp_lt_int64_1_cj)
{
    // from LLT ConstAnalysis/relationalOp/lt_int64_1.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "6444");
        auto* rhs = Lit("Int64", "7000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::LT, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"LT=true"});
}

TEST_F(ConstAnalysisFixture, relationalOp_lt_int64_2_cj)
{
    // from LLT ConstAnalysis/relationalOp/lt_int64_2.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "7000");
        auto* rhs = Lit("Int64", "7000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::LT, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"LT=false"});
}

TEST_F(ConstAnalysisFixture, relationalOp_lt_int64_3_cj)
{
    // from LLT ConstAnalysis/relationalOp/lt_int64_3.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "99999");
        auto* rhs = Lit("Int64", "-7000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::LT, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"LT=false"});
}

TEST_F(ConstAnalysisFixture, relationalOp_lt_uint64_1_cj)
{
    // from LLT ConstAnalysis/relationalOp/lt_uint64_1.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("UInt64", "9000");
        auto* rhs = Lit("UInt64", "7000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::LT, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"LT=false"});
}

TEST_F(ConstAnalysisFixture, relationalOp_lt_uint64_2_cj)
{
    // from LLT ConstAnalysis/relationalOp/lt_uint64_2.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("UInt64", "9000");
        auto* rhs = Lit("UInt64", "9000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::LT, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"LT=false"});
}

TEST_F(ConstAnalysisFixture, relationalOp_lt_uint64_3_cj)
{
    // from LLT ConstAnalysis/relationalOp/lt_uint64_3.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("UInt64", "6000");
        auto* rhs = Lit("UInt64", "7000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::LT, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"LT=true"});
}

TEST_F(ConstAnalysisFixture, relationalOp_neq_float64_1_cj)
{
    // from LLT ConstAnalysis/relationalOp/neq_float64_1.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Float64", "-6000.00");
        auto* rhs = Lit("Float64", "7000.512");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::NOTEQUAL, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"NotEqual=true"});
}

TEST_F(ConstAnalysisFixture, relationalOp_neq_float64_2_cj)
{
    // from LLT ConstAnalysis/relationalOp/neq_float64_2.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Float64", "7000.512");
        auto* rhs = Lit("Float64", "7000.512");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::NOTEQUAL, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"NotEqual=false"});
}

TEST_F(ConstAnalysisFixture, relationalOp_neq_int64_1_cj)
{
    // from LLT ConstAnalysis/relationalOp/neq_int64_1.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "-6000");
        auto* rhs = Lit("Int64", "7000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::NOTEQUAL, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"NotEqual=true"});
}

TEST_F(ConstAnalysisFixture, relationalOp_neq_int64_2_cj)
{
    // from LLT ConstAnalysis/relationalOp/neq_int64_2.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("Int64", "-6000");
        auto* rhs = Lit("Int64", "-6000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::NOTEQUAL, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"NotEqual=false"});
}

TEST_F(ConstAnalysisFixture, relationalOp_neq_uint64_1_cj)
{
    // from LLT ConstAnalysis/relationalOp/neq_uint64_1.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("UInt64", "6000");
        auto* rhs = Lit("UInt64", "7000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::NOTEQUAL, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"NotEqual=true"});
}

TEST_F(ConstAnalysisFixture, relationalOp_neq_uint64_2_cj)
{
    // from LLT ConstAnalysis/relationalOp/neq_uint64_2.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = Lit("UInt64", "6000");
        auto* rhs = Lit("UInt64", "6000");
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::NOTEQUAL, lhs, rhs, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"NotEqual=false"});
}

TEST_F(ConstAnalysisFixture, relationalOp_neq_unit_cj)
{
    // from LLT ConstAnalysis/relationalOp/neq_unit.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* lhs = builder->CreateConstantExpression<UnitLiteral>(Ty("Unit"), curBlock);
        curBlock->AppendExpression(lhs);
        auto* rhs = builder->CreateConstantExpression<UnitLiteral>(Ty("Unit"), curBlock);
        curBlock->AppendExpression(rhs);
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("Bool"), BinaryExprKind::NOTEQUAL, lhs->GetResult(), rhs->GetResult(), OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"NotEqual=false"});
}

TEST_F(ConstAnalysisFixture, terminator_branch_terminator_cj)
{
    // from LLT ConstAnalysis/terminator/branch_terminator.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* a = LitInt(Ty("UInt64"), 9000ull);
        auto* b = LitInt(Ty("UInt64"), 7000ull);
        auto* expr = builder->CreateExpression<BinaryExpression>(
            Ty("UInt64"), BinaryExprKind::SUB, a, b, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"Sub=2000"});
}

TEST_F(ConstAnalysisFixture, typecast_int_cj)
{
    // from LLT ConstAnalysis/typecast/int.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* src = Lit("Int8", "127");
        auto* expr = builder->CreateExpression<NumericCast>(
            Ty("Int16"), src, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* src = Lit("Int16", "-100");
        auto* expr = builder->CreateExpression<NumericCast>(
            Ty("Int8"), src, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"NumericCast=127", "NumericCast=-100"});
}

TEST_F(ConstAnalysisFixture, typecast_int2uint_cj)
{
    // from LLT ConstAnalysis/typecast/int2uint.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* src = Lit("Int8", "127");
        auto* expr = builder->CreateExpression<NumericCast>(
            Ty("UInt8"), src, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* src = Lit("UInt8", "100");
        auto* expr = builder->CreateExpression<NumericCast>(
            Ty("Int8"), src, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"NumericCast=127", "NumericCast=100"});
}

TEST_F(ConstAnalysisFixture, typecast_overflow_saturating_cj)
{
    // from LLT ConstAnalysis/typecast/overflow_saturating.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* src = Lit("Int16", "-2000");
        auto* expr = builder->CreateExpression<NumericCast>(
            Ty("Int8"), src, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* src = Lit("UInt16", "2000");
        auto* expr = builder->CreateExpression<NumericCast>(
            Ty("Int8"), src, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* src = Lit("Int16", "-2000");
        auto* expr = builder->CreateExpression<NumericCast>(
            Ty("UInt8"), src, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* src = Lit("UInt16", "2000");
        auto* expr = builder->CreateExpression<NumericCast>(
            Ty("UInt8"), src, OverflowStrategy::SATURATING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"NumericCast=255", "NumericCast=0", "NumericCast=127", "NumericCast=-128"});
}

TEST_F(ConstAnalysisFixture, typecast_overflow_wrapping_cj)
{
    // from LLT ConstAnalysis/typecast/overflow_wrapping.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* src = Lit("Int16", "-2000");
        auto* expr = builder->CreateExpression<NumericCast>(
            Ty("Int8"), src, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* src = Lit("UInt16", "2000");
        auto* expr = builder->CreateExpression<NumericCast>(
            Ty("Int8"), src, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* src = Lit("Int16", "-2000");
        auto* expr = builder->CreateExpression<NumericCast>(
            Ty("UInt8"), src, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* src = Lit("UInt16", "2000");
        auto* expr = builder->CreateExpression<NumericCast>(
            Ty("UInt8"), src, OverflowStrategy::WRAPPING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"NumericCast=208", "NumericCast=48", "NumericCast=-48", "NumericCast=48"});
}

TEST_F(ConstAnalysisFixture, typecast_uint_cj)
{
    // from LLT ConstAnalysis/typecast/uint.cj
    std::vector<std::string> got;
    {
        NewFunc();
        auto* src = Lit("UInt16", "65535");
        auto* expr = builder->CreateExpression<NumericCast>(
            Ty("UInt32"), src, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    {
        NewFunc();
        auto* src = Lit("UInt32", "40001");
        auto* expr = builder->CreateExpression<NumericCast>(
            Ty("UInt16"), src, OverflowStrategy::THROWING, curBlock);
        curBlock->AppendExpression(expr);
        FinishFunc();
        auto part = Analyse(curFunc);
        got.insert(got.end(), part.begin(), part.end());
    }
    ExpectSorted(got, {"NumericCast=65535", "NumericCast=40001"});
}
