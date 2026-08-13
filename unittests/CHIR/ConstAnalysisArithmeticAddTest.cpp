// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * ConstAnalysis arithmetic ADD folding tests (migrated from LLT).
 */


#include "ConstAnalysisFixture.h"

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

