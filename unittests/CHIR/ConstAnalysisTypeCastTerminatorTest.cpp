// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * ConstAnalysis typecast, branch terminator, and misc tests (migrated from LLT).
 */


#include "ConstAnalysisFixture.h"

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

