// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * ConstAnalysis arithmetic SUB folding tests (migrated from LLT).
 */


#include "ConstAnalysisFixture.h"

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

