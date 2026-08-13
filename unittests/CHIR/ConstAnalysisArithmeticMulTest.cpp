// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * ConstAnalysis arithmetic MUL folding tests (migrated from LLT).
 */


#include "ConstAnalysisFixture.h"

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

