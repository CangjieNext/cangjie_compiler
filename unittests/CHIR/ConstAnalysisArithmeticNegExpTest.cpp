// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * ConstAnalysis arithmetic NEG/EXP folding tests (migrated from LLT).
 */


#include "ConstAnalysisFixture.h"

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

