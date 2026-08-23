// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * ConstAnalysis relational GE/GT/LE/LT folding tests (migrated from LLT).
 */


#include "ConstAnalysisFixture.h"

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

