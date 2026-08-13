// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * ConstAnalysis relational EQ/NEQ folding tests (migrated from LLT).
 */


#include "ConstAnalysisFixture.h"

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

