// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * ConstAnalysis bitwise operator folding tests (migrated from LLT).
 */


#include "ConstAnalysisFixture.h"

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

