// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * ConstAnalysis ConstValue APIs, logical/relational ops, and overflow diagnostics.
 */


#include "ConstAnalysisIRFixture.h"

TEST_F(ConstAnalysisIRFixture, ConstValueJoinCloneToString)
{
    ConstBoolVal b1(true);
    ConstBoolVal b2(true);
    ConstBoolVal b3(false);
    EXPECT_EQ(b1.ToString(), "true");
    EXPECT_EQ(b1.GetVal(), true);
    EXPECT_FALSE(b1.Join(b2).has_value()); // same -> nullopt optional empty meaning no change... actually returns nullopt
    EXPECT_EQ(b1.Join(b2), std::nullopt);
    auto joinedDiff = b1.Join(b3);
    EXPECT_TRUE(joinedDiff.has_value());
    EXPECT_EQ(joinedDiff.value(), nullptr);
    auto bClone = b1.Clone();
    EXPECT_EQ(StaticCast<ConstBoolVal*>(bClone.get())->GetVal(), true);

    ConstRuneVal r1(U'a');
    ConstRuneVal r2(U'a');
    ConstRuneVal r3(U'b');
    EXPECT_EQ(r1.ToString(), std::to_string(static_cast<uint32_t>(U'a')));
    EXPECT_EQ(r1.GetVal(), U'a');
    EXPECT_EQ(r1.Join(r2), std::nullopt);
    EXPECT_EQ(r1.Join(r3).value(), nullptr);
    EXPECT_EQ(StaticCast<ConstRuneVal*>(r1.Clone().get())->GetVal(), U'a');

    ConstStrVal s1("hi");
    ConstStrVal s2("hi");
    ConstStrVal s3("ho");
    EXPECT_EQ(s1.ToString(), "hi");
    EXPECT_EQ(s1.GetVal(), "hi");
    EXPECT_EQ(s1.Join(s2), std::nullopt);
    EXPECT_EQ(s1.Join(s3).value(), nullptr);
    EXPECT_EQ(StaticCast<ConstStrVal*>(s1.Clone().get())->GetVal(), "hi");

    ConstUIntVal u1(7);
    ConstUIntVal u2(7);
    ConstUIntVal u3(8);
    EXPECT_EQ(u1.Join(u2), std::nullopt);
    EXPECT_EQ(u1.Join(u3).value(), nullptr);
    EXPECT_EQ(StaticCast<ConstUIntVal*>(u1.Clone().get())->GetVal(), 7u);

    ConstIntVal i1(-3);
    ConstIntVal i2(-3);
    ConstIntVal i3(1);
    EXPECT_EQ(i1.Join(i2), std::nullopt);
    EXPECT_EQ(i1.Join(i3).value(), nullptr);
    EXPECT_EQ(StaticCast<ConstIntVal*>(i1.Clone().get())->GetVal(), -3);

    ConstFloatVal f1(1.5);
    ConstFloatVal f2(1.5);
    ConstFloatVal f3(2.5);
    EXPECT_EQ(f1.Join(f2), std::nullopt);
    EXPECT_EQ(f1.Join(f3).value(), nullptr);
    EXPECT_EQ(StaticCast<ConstFloatVal*>(f1.Clone().get())->GetVal(), 1.5);

    // Cross-kind join -> bottom
    EXPECT_EQ(b1.Join(u1).value(), nullptr);
}

TEST_F(ConstAnalysisIRFixture, LiteralKindsIncludingFloat16RuneStringUnit)
{
    NewFunc();
    (void)LitFloat(builder->GetFloat16Ty(), 1.0);
    (void)LitRune(U'Z');
    (void)LitStr("abc");
    auto* unitLit = builder->CreateConstantExpression<UnitLiteral>(builder->GetUnitTy(), curBlock);
    curBlock->AppendExpression(unitLit);
    FinishWithExit();
    auto results = Analyse(curFunc);
    ASSERT_NE(results, nullptr);
}

TEST_F(ConstAnalysisIRFixture, LogicalAndOrAndBoolRelational)
{
    NewFunc();
    auto* t = LitBool(true);
    auto* f = LitBool(false);
    auto* andExpr = builder->CreateExpression<BinaryExpression>(
        builder->GetBoolTy(), BinaryExprKind::AND, t, f, OverflowStrategy::NA, curBlock);
    curBlock->AppendExpression(andExpr);
    auto* orExpr = builder->CreateExpression<BinaryExpression>(
        builder->GetBoolTy(), BinaryExprKind::OR, t, f, OverflowStrategy::NA, curBlock);
    curBlock->AppendExpression(orExpr);
    auto* eq = builder->CreateExpression<BinaryExpression>(
        builder->GetBoolTy(), BinaryExprKind::EQUAL, t, t, OverflowStrategy::NA, curBlock);
    curBlock->AppendExpression(eq);
    auto* ne = builder->CreateExpression<BinaryExpression>(
        builder->GetBoolTy(), BinaryExprKind::NOTEQUAL, t, f, OverflowStrategy::NA, curBlock);
    curBlock->AppendExpression(ne);
    FinishWithExit();
    auto results = Analyse(curFunc);
    ASSERT_NE(results, nullptr);
    std::vector<std::string> got;
    results->VisitWith(
        [](const ConstDomain&, Expression*, size_t) {},
        [&got](const ConstDomain& state, Expression* expr, size_t) {
            if (expr && expr->GetResult()) {
                if (auto* abs = state.CheckAbstractValue(expr->GetResult())) {
                    got.emplace_back(std::string(expr->GetExprKindName()) + "=" + abs->ToString());
                }
            }
        },
        [](const ConstDomain&, Expression*, std::optional<Block*>) {});
    EXPECT_NE(std::find(got.begin(), got.end(), "And=false"), got.end());
    EXPECT_NE(std::find(got.begin(), got.end(), "Equal=true"), got.end());
}

TEST_F(ConstAnalysisIRFixture, RuneAndStringRelational)
{
    NewFunc();
    auto* r1 = LitRune(U'a');
    auto* r2 = LitRune(U'b');
    auto* s1 = LitStr("a");
    auto* s2 = LitStr("b");
    auto* rLt = builder->CreateExpression<BinaryExpression>(
        builder->GetBoolTy(), BinaryExprKind::LT, r1, r2, OverflowStrategy::NA, curBlock);
    curBlock->AppendExpression(rLt);
    auto* sEq = builder->CreateExpression<BinaryExpression>(
        builder->GetBoolTy(), BinaryExprKind::EQUAL, s1, s2, OverflowStrategy::NA, curBlock);
    curBlock->AppendExpression(sEq);
    FinishWithExit();
    ASSERT_NE(Analyse(curFunc), nullptr);
}

TEST_F(ConstAnalysisIRFixture, UIntNegAndBitwiseXor)
{
    NewFunc();
    auto* u = LitInt(builder->GetUInt64Ty(), 5);
    auto* neg = builder->CreateExpression<UnaryExpression>(
        builder->GetUInt64Ty(), UnaryExprKind::NEG, u, OverflowStrategy::WRAPPING, curBlock);
    curBlock->AppendExpression(neg);
    auto* x = LitInt(builder->GetInt64Ty(), 1);
    auto* y = LitInt(builder->GetInt64Ty(), 2);
    auto* bx = builder->CreateExpression<BinaryExpression>(
        builder->GetInt64Ty(), BinaryExprKind::BITXOR, x, y, OverflowStrategy::NA, curBlock);
    curBlock->AppendExpression(bx);
    auto* ba = builder->CreateExpression<BinaryExpression>(
        builder->GetInt64Ty(), BinaryExprKind::BITAND, x, y, OverflowStrategy::NA, curBlock);
    curBlock->AppendExpression(ba);
    auto* bo = builder->CreateExpression<BinaryExpression>(
        builder->GetInt64Ty(), BinaryExprKind::BITOR, x, y, OverflowStrategy::NA, curBlock);
    curBlock->AppendExpression(bo);
    FinishWithExit();
    ASSERT_NE(Analyse(curFunc), nullptr);
}

TEST_F(ConstAnalysisIRFixture, DivByZeroAndOverflowDiagnostics)
{
    // VisitWith re-runs transfer after SetToStable, which enables Raise* diagnostics.
    NewFunc();
    auto* a = LitInt(builder->GetInt64Ty(), 1);
    auto* z = LitInt(builder->GetInt64Ty(), 0);
    auto* div = builder->CreateExpression<BinaryExpression>(
        builder->GetInt64Ty(), BinaryExprKind::DIV, a, z, OverflowStrategy::THROWING, curBlock);
    curBlock->AppendExpression(div);
    auto* mod = builder->CreateExpression<BinaryExpression>(
        builder->GetInt64Ty(), BinaryExprKind::MOD, a, z, OverflowStrategy::THROWING, curBlock);
    curBlock->AppendExpression(mod);
    // signed overflow: Int8 127 + 1
    auto* x = LitInt(builder->GetInt8Ty(), 127);
    auto* one = LitInt(builder->GetInt8Ty(), 1);
    auto* add = builder->CreateExpression<BinaryExpression>(
        builder->GetInt8Ty(), BinaryExprKind::ADD, x, one, OverflowStrategy::THROWING, curBlock);
    curBlock->AppendExpression(add);
    // negative / overshift
    auto* shL = LitInt(builder->GetInt64Ty(), 1);
    auto* negSh = LitInt(builder->GetInt64Ty(), static_cast<uint64_t>(-1));
    auto* lshift = builder->CreateExpression<BinaryExpression>(
        builder->GetInt64Ty(), BinaryExprKind::LSHIFT, shL, negSh, OverflowStrategy::THROWING, curBlock);
    curBlock->AppendExpression(lshift);
    auto* bigSh = LitInt(builder->GetInt64Ty(), 100);
    auto* rshift = builder->CreateExpression<BinaryExpression>(
        builder->GetInt64Ty(), BinaryExprKind::RSHIFT, shL, bigSh, OverflowStrategy::THROWING, curBlock);
    curBlock->AppendExpression(rshift);
    FinishWithExit();
    auto results = AnalyseWithDiagnostics(curFunc);
    ASSERT_NE(results, nullptr);
}

TEST_F(ConstAnalysisIRFixture, MixedSignBitwiseShift)
{
    NewFunc();
    auto* si = LitInt(builder->GetInt64Ty(), static_cast<uint64_t>(-2));
    auto* ui = LitInt(builder->GetUInt64Ty(), 3);
    // Int << UInt and UInt & Int — exercises mixed-sign HandleBitwiseOpOfType templates.
    auto* sh = builder->CreateExpression<BinaryExpression>(
        builder->GetInt64Ty(), BinaryExprKind::LSHIFT, si, ui, OverflowStrategy::THROWING, curBlock);
    curBlock->AppendExpression(sh);
    auto* ande = builder->CreateExpression<BinaryExpression>(
        builder->GetUInt64Ty(), BinaryExprKind::BITAND, ui, si, OverflowStrategy::NA, curBlock);
    curBlock->AppendExpression(ande);
    auto* bitorx = builder->CreateExpression<BinaryExpression>(
        builder->GetInt64Ty(), BinaryExprKind::BITOR, si, ui, OverflowStrategy::NA, curBlock);
    curBlock->AppendExpression(bitorx);
    auto* bitxor = builder->CreateExpression<BinaryExpression>(
        builder->GetUInt64Ty(), BinaryExprKind::BITXOR, ui, si, OverflowStrategy::NA, curBlock);
    curBlock->AppendExpression(bitxor);
    auto* rsh = builder->CreateExpression<BinaryExpression>(
        builder->GetInt64Ty(), BinaryExprKind::RSHIFT, si, LitInt(builder->GetUInt64Ty(), 1),
        OverflowStrategy::THROWING, curBlock);
    curBlock->AppendExpression(rsh);
    FinishWithExit();
    ASSERT_NE(Analyse(curFunc), nullptr);
}

TEST_F(ConstAnalysisIRFixture, NotFloatNegUnknownOpsAndTryTerminators)
{
    NewFunc();
    auto* t = LitBool(true);
    curBlock->AppendExpression(builder->CreateExpression<UnaryExpression>(
        builder->GetBoolTy(), UnaryExprKind::NOT, t, OverflowStrategy::NA, curBlock));

    auto* f1 = LitFloat(builder->GetFloat64Ty(), 1.5);
    curBlock->AppendExpression(builder->CreateExpression<UnaryExpression>(
        builder->GetFloat64Ty(), UnaryExprKind::NEG, f1, OverflowStrategy::NA, curBlock));
    auto* finf = LitFloat(builder->GetFloat64Ty(), std::numeric_limits<double>::infinity());
    curBlock->AppendExpression(builder->CreateExpression<UnaryExpression>(
        builder->GetFloat64Ty(), UnaryExprKind::NEG, finf, OverflowStrategy::NA, curBlock));

    auto* boolRef = builder->GetType<RefType>(builder->GetBoolTy());
    auto* boolAlloc = builder->CreateExpression<Allocate>(boolRef, builder->GetBoolTy(), curBlock);
    curBlock->AppendExpression(boolAlloc);
    auto* unkBool = builder->CreateExpression<Load>(builder->GetBoolTy(), boolAlloc->GetResult(), curBlock);
    curBlock->AppendExpression(unkBool);
    curBlock->AppendExpression(builder->CreateExpression<UnaryExpression>(
        builder->GetBoolTy(), UnaryExprKind::NOT, unkBool->GetResult(), OverflowStrategy::NA, curBlock));

    auto* iRef = builder->GetType<RefType>(builder->GetInt64Ty());
    auto* iAlloc = builder->CreateExpression<Allocate>(iRef, builder->GetInt64Ty(), curBlock);
    curBlock->AppendExpression(iAlloc);
    auto* unkI = builder->CreateExpression<Load>(builder->GetInt64Ty(), iAlloc->GetResult(), curBlock);
    curBlock->AppendExpression(unkI);
    auto* knownI = LitInt(builder->GetInt64Ty(), 3);
    curBlock->AppendExpression(builder->CreateExpression<BinaryExpression>(builder->GetInt64Ty(), BinaryExprKind::ADD,
        unkI->GetResult(), unkI->GetResult(), OverflowStrategy::THROWING, curBlock));
    curBlock->AppendExpression(builder->CreateExpression<BinaryExpression>(
        builder->GetInt64Ty(), BinaryExprKind::ADD, knownI, unkI->GetResult(), OverflowStrategy::THROWING, curBlock));
    curBlock->AppendExpression(builder->CreateExpression<BinaryExpression>(
        builder->GetInt64Ty(), BinaryExprKind::SUB, knownI, knownI, OverflowStrategy::THROWING, curBlock));
    curBlock->AppendExpression(builder->CreateExpression<BinaryExpression>(
        builder->GetBoolTy(), BinaryExprKind::LT, knownI, unkI->GetResult(), OverflowStrategy::NA, curBlock));
    curBlock->AppendExpression(builder->CreateExpression<BinaryExpression>(builder->GetInt64Ty(),
        BinaryExprKind::LSHIFT, knownI, unkI->GetResult(), OverflowStrategy::THROWING, curBlock));
    curBlock->AppendExpression(builder->CreateExpression<BinaryExpression>(builder->GetInt64Ty(),
        BinaryExprKind::LSHIFT, unkI->GetResult(), LitInt(builder->GetInt64Ty(), 1), OverflowStrategy::THROWING,
        curBlock));

    curBlock->AppendExpression(builder->CreateExpression<BinaryExpression>(builder->GetBoolTy(), BinaryExprKind::AND,
        LitBool(false), unkBool->GetResult(), OverflowStrategy::NA, curBlock));
    curBlock->AppendExpression(builder->CreateExpression<BinaryExpression>(builder->GetBoolTy(), BinaryExprKind::OR,
        LitBool(true), unkBool->GetResult(), OverflowStrategy::NA, curBlock));
    curBlock->AppendExpression(builder->CreateExpression<BinaryExpression>(builder->GetBoolTy(), BinaryExprKind::AND,
        unkBool->GetResult(), unkBool->GetResult(), OverflowStrategy::NA, curBlock));

    auto* fRef = builder->GetType<RefType>(builder->GetFloat64Ty());
    auto* fAlloc = builder->CreateExpression<Allocate>(fRef, builder->GetFloat64Ty(), curBlock);
    curBlock->AppendExpression(fAlloc);
    auto* unkF = builder->CreateExpression<Load>(builder->GetFloat64Ty(), fAlloc->GetResult(), curBlock);
    curBlock->AppendExpression(unkF);
    auto* f2 = LitFloat(builder->GetFloat64Ty(), 2.0);
    curBlock->AppendExpression(builder->CreateExpression<BinaryExpression>(
        builder->GetBoolTy(), BinaryExprKind::LT, f2, LitFloat(builder->GetFloat64Ty(), 3.0), OverflowStrategy::NA,
        curBlock));
    curBlock->AppendExpression(builder->CreateExpression<BinaryExpression>(
        builder->GetFloat64Ty(), BinaryExprKind::ADD, f2, unkF->GetResult(), OverflowStrategy::NA, curBlock));
    curBlock->AppendExpression(builder->CreateExpression<BinaryExpression>(
        builder->GetFloat64Ty(), BinaryExprKind::EXP, f2, f2, OverflowStrategy::NA, curBlock));

    auto* base = LitInt(builder->GetInt64Ty(), 100);
    auto* expU = LitInt(builder->GetUInt64Ty(), 20);
    curBlock->AppendExpression(builder->CreateExpression<BinaryExpression>(
        builder->GetInt64Ty(), BinaryExprKind::EXP, base, expU, OverflowStrategy::THROWING, curBlock));
    curBlock->AppendExpression(builder->CreateExpression<BinaryExpression>(builder->GetInt64Ty(), BinaryExprKind::EXP,
        unkI->GetResult(), LitInt(builder->GetUInt64Ty(), 3), OverflowStrategy::THROWING, curBlock));

    curBlock->AppendExpression(builder->CreateExpression<NumericCast>(
        builder->GetInt64Ty(), f2, OverflowStrategy::THROWING, curBlock));
    curBlock->AppendExpression(builder->CreateExpression<NumericCast>(
        builder->GetFloat64Ty(), knownI, OverflowStrategy::THROWING, curBlock));
    auto* clsDef = builder->CreateClass(INVALID_LOCATION, "C2", "C2", "default", true, false);
    auto* clsTy = builder->GetType<ClassType>(clsDef);
    clsDef->SetType(*clsTy);
    auto* clsRef = builder->GetType<RefType>(clsTy);
    curBlock->AppendExpression(builder->CreateExpression<ClassStaticCast>(clsRef, knownI, curBlock));

    auto* thenB = NewBlock();
    auto* elseB = NewBlock();
    auto* join = NewBlock();
    curBlock->AppendExpression(builder->CreateTerminator<Branch>(unkBool->GetResult(), thenB, elseB, curBlock));
    curBlock = thenB;
    curBlock->AppendExpression(builder->CreateTerminator<GoTo>(join, curBlock));
    curBlock = elseB;
    curBlock->AppendExpression(builder->CreateTerminator<GoTo>(join, curBlock));
    curBlock = join;
    auto* mbDefault = NewBlock();
    auto* mbCase = NewBlock();
    auto* mbJoin = NewBlock();
    auto* uRef = builder->GetType<RefType>(builder->GetUInt64Ty());
    auto* uAlloc = builder->CreateExpression<Allocate>(uRef, builder->GetUInt64Ty(), curBlock);
    curBlock->AppendExpression(uAlloc);
    auto* unkU = builder->CreateExpression<Load>(builder->GetUInt64Ty(), uAlloc->GetResult(), curBlock);
    curBlock->AppendExpression(unkU);
    curBlock->AppendExpression(builder->CreateTerminator<MultiBranch>(
        unkU->GetResult(), mbDefault, std::vector<uint64_t>{1}, std::vector<Block*>{mbCase}, curBlock));
    curBlock = mbCase;
    curBlock->AppendExpression(builder->CreateTerminator<GoTo>(mbJoin, curBlock));
    curBlock = mbDefault;
    curBlock->AppendExpression(builder->CreateTerminator<GoTo>(mbJoin, curBlock));
    curBlock = mbJoin;

    auto* ok1 = NewBlock();
    auto* err1 = NewBlock();
    curBlock->AppendExpression(builder->CreateExpression<TryUnaryExpression>(
        builder->GetInt64Ty(), UnaryExprKind::NEG, LitInt(builder->GetInt64Ty(), 5), ok1, err1, curBlock));
    curBlock = ok1;
    auto* ok2 = NewBlock();
    auto* err2 = NewBlock();
    curBlock->AppendExpression(builder->CreateExpression<TryUnaryExpression>(builder->GetInt8Ty(), UnaryExprKind::NEG,
        LitInt(builder->GetInt8Ty(), static_cast<uint64_t>(-128)), ok2, err2, curBlock));
    curBlock = err2;
    auto* ok3 = NewBlock();
    auto* err3 = NewBlock();
    curBlock->AppendExpression(builder->CreateExpression<TryNumericCast>(
        builder->GetInt8Ty(), LitInt(builder->GetInt64Ty(), 1), ok3, err3, curBlock));
    curBlock = ok3;
    auto* ok4 = NewBlock();
    auto* err4 = NewBlock();
    curBlock->AppendExpression(builder->CreateExpression<TryAllocate>(
        builder->GetType<RefType>(builder->GetInt64Ty()), builder->GetInt64Ty(), ok4, err4, curBlock));
    curBlock = ok4;
    FinishWithExit();
    curBlock = err1;
    FinishWithExit();
    curBlock = err3;
    FinishWithExit();
    curBlock = err4;
    FinishWithExit();
    curBlock = ok2;
    FinishWithExit();

    ASSERT_NE(AnalyseWithDiagnostics(curFunc), nullptr);
    ASSERT_NE(AnalysePool(curFunc), nullptr);
}

