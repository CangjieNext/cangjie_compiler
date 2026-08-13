// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * ConstAnalysis arithmetic DIV/MOD folding tests (migrated from LLT).
 */


#include "ConstAnalysisFixture.h"

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

