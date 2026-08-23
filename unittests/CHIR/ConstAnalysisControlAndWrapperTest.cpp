// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * ConstAnalysis branch/multi-branch/try terminators and ConstAnalysisWrapper paths.
 */


#include "ConstAnalysisIRFixture.h"

TEST_F(ConstAnalysisIRFixture, ConstBranchSelectsTrueFalse)
{
    NewFunc();
    auto* entry = curBlock;
    auto* thenB = NewBlock();
    auto* elseB = NewBlock();
    auto* cond = LitBool(true);
    entry->AppendExpression(builder->CreateTerminator<Branch>(cond, thenB, elseB, entry));
    curBlock = thenB;
    FinishWithExit();
    curBlock = elseB;
    FinishWithExit();
    ASSERT_NE(Analyse(curFunc), nullptr);

    // false branch
    NewFunc();
    entry = curBlock;
    thenB = NewBlock();
    elseB = NewBlock();
    cond = LitBool(false);
    entry->AppendExpression(builder->CreateTerminator<Branch>(cond, thenB, elseB, entry));
    curBlock = thenB;
    FinishWithExit();
    curBlock = elseB;
    FinishWithExit();
    ASSERT_NE(Analyse(curFunc), nullptr);
}

TEST_F(ConstAnalysisIRFixture, MultiBranchHitsCaseAndDefault)
{
    NewFunc();
    auto* entry = curBlock;
    auto* case0 = NewBlock();
    auto* case1 = NewBlock();
    auto* defB = NewBlock();
    auto* cond = LitInt(builder->GetUInt64Ty(), 1);
    entry->AppendExpression(
        builder->CreateTerminator<MultiBranch>(cond, defB, std::vector<uint64_t>{0, 1}, std::vector<Block*>{case0, case1},
            entry));
    for (auto* b : {case0, case1, defB}) {
        curBlock = b;
        FinishWithExit();
    }
    ASSERT_NE(Analyse(curFunc), nullptr);

    NewFunc();
    entry = curBlock;
    case0 = NewBlock();
    defB = NewBlock();
    cond = LitInt(builder->GetUInt64Ty(), 99);
    entry->AppendExpression(
        builder->CreateTerminator<MultiBranch>(cond, defB, std::vector<uint64_t>{0}, std::vector<Block*>{case0}, entry));
    curBlock = case0;
    FinishWithExit();
    curBlock = defB;
    FinishWithExit();
    ASSERT_NE(Analyse(curFunc), nullptr);
}

TEST_F(ConstAnalysisIRFixture, TryAddTerminatorSuccessPath)
{
    NewFunc();
    auto* entry = curBlock;
    auto* ok = NewBlock();
    auto* err = NewBlock();
    auto* a = LitInt(builder->GetInt64Ty(), 1);
    auto* b = LitInt(builder->GetInt64Ty(), 2);
    auto* tryAdd = builder->CreateExpression<TryBinaryExpression>(builder->GetInt64Ty(), BinaryExprKind::ADD, a, b,
        OverflowStrategy::THROWING, ok, err, entry);
    entry->AppendExpression(tryAdd);
    curBlock = ok;
    FinishWithExit();
    curBlock = err;
    FinishWithExit();
    ASSERT_NE(Analyse(curFunc), nullptr);
}

TEST_F(ConstAnalysisIRFixture, WrapperRunOnPackageAndCheckResults)
{
    NewFunc();
    auto* a = LitInt(builder->GetInt64Ty(), 1);
    auto* b = LitInt(builder->GetInt64Ty(), 2);
    auto* add = builder->CreateExpression<BinaryExpression>(
        builder->GetInt64Ty(), BinaryExprKind::ADD, a, b, OverflowStrategy::THROWING, curBlock);
    curBlock->AppendExpression(add);
    FinishWithExit();

    // std.* package name forces FullStatePool strategy
    auto* stdFuncTy = builder->GetType<FuncType>(std::vector<Type*>{}, builder->GetUnitTy());
    auto* stdFunc = builder->CreateFunction(stdFuncTy, "std_f", "std_f", "", "std.core");
    auto* stdBody = builder->CreateBlockGroup(*stdFunc);
    stdFunc->InitBody(*stdBody);
    auto* stdBlock = builder->CreateBlock(stdBody);
    stdBody->SetEntryBlock(stdBlock);
    stdBlock->AppendExpression(builder->CreateTerminator<Exit>(stdBlock));

    wrapper->RunOnPackage(package, false, 1, *diag);
    EXPECT_NE(wrapper->CheckFuncResult(*curFunc), nullptr);
    EXPECT_EQ(wrapper->CheckFuncActiveResult(*curFunc), nullptr);
    wrapper->InvalidateAllAnalysisResults();
    EXPECT_EQ(wrapper->CheckFuncResult(*curFunc), nullptr);

    // Parallel path
    NewFunc();
    FinishWithExit();
    wrapper->RunOnPackage(package, false, 2, *diag);
    wrapper->InvalidateAllAnalysisResults();
}

TEST_F(ConstAnalysisIRFixture, DebugPrintPath)
{
    NewFunc();
    auto* a = LitInt(builder->GetInt64Ty(), 1);
    auto* b = LitInt(builder->GetInt64Ty(), 1);
    auto* add = builder->CreateExpression<BinaryExpression>(
        builder->GetInt64Ty(), BinaryExprKind::ADD, a, b, OverflowStrategy::THROWING, curBlock);
    curBlock->AppendExpression(add);
    FinishWithExit();
    // isDebug=true covers PrintDebugMessage
    ASSERT_NE(wrapper->RunOnFunc(curFunc, true, *diag), nullptr);
}

TEST_F(ConstAnalysisIRFixture, WrapperActiveStatePoolAndSkipHugeFunc)
{
    // >300 blocks => ActiveStatePool; also hit CheckFuncActiveResult.
    auto* midTy = builder->GetType<FuncType>(std::vector<Type*>{}, builder->GetUnitTy());
    auto* mid = builder->CreateFunction(midTy, "mid_blocks", "mid_blocks", "", "default");
    auto* midBody = builder->CreateBlockGroup(*mid);
    mid->InitBody(*midBody);
    Block* prev = nullptr;
    for (int i = 0; i < 320; ++i) {
        auto* b = builder->CreateBlock(midBody);
        if (i == 0) {
            midBody->SetEntryBlock(b);
        } else {
            prev->AppendExpression(builder->CreateTerminator<GoTo>(b, prev));
        }
        if (i + 1 == 320) {
            b->AppendExpression(builder->CreateTerminator<Exit>(b));
        }
        prev = b;
    }

    // >1000 blocks => SkipAnalysis
    auto* huge = builder->CreateFunction(midTy, "huge_blocks", "huge_blocks", "", "default");
    auto* hugeBody = builder->CreateBlockGroup(*huge);
    huge->InitBody(*hugeBody);
    prev = nullptr;
    for (int i = 0; i < 1002; ++i) {
        auto* b = builder->CreateBlock(hugeBody);
        if (i == 0) {
            hugeBody->SetEntryBlock(b);
        } else {
            prev->AppendExpression(builder->CreateTerminator<GoTo>(b, prev));
        }
        if (i + 1 == 1002) {
            b->AppendExpression(builder->CreateTerminator<Exit>(b));
        }
        prev = b;
    }

    wrapper->RunOnPackage(package, false, 1, *diag);
    EXPECT_NE(wrapper->CheckFuncActiveResult(*mid), nullptr);
    EXPECT_EQ(wrapper->CheckFuncResult(*huge), nullptr);
    EXPECT_EQ(wrapper->CheckFuncActiveResult(*huge), nullptr);
}

TEST_F(ConstAnalysisIRFixture, WrapperLambdaBlockCountAndSkipAlreadyAnalysed)
{
    // Lambda body blocks are counted via GetBlockSize; >1000 via lambda triggers early OVERHEAD return.
    auto* hugeViaLambda = NewFunc();
    auto* lambdaTy = builder->GetType<FuncType>(std::vector<Type*>{}, builder->GetUnitTy());
    auto* lambda = builder->CreateExpression<Lambda>(lambdaTy, lambdaTy, curBlock, false, "hugeL", "hugeL");
    auto* lBg = builder->CreateBlockGroup(*curFunc);
    lambda->InitBody(*lBg);
    Block* prev = nullptr;
    for (int i = 0; i < 1001; ++i) {
        auto* b = builder->CreateBlock(lBg);
        if (i == 0) {
            lBg->SetEntryBlock(b);
        } else {
            prev->AppendExpression(builder->CreateTerminator<GoTo>(b, prev));
        }
        if (i + 1 == 1001) {
            b->AppendExpression(builder->CreateTerminator<Exit>(b));
        }
        prev = b;
    }
    curBlock->AppendExpression(lambda);
    FinishWithExit();

    // Small func analysed first, then re-run skips already-analysed.
    auto* small = NewFunc();
    LitInt(builder->GetInt64Ty(), 1);
    FinishWithExit();

    wrapper->RunOnPackage(package, false, 1, *diag);
    EXPECT_NE(wrapper->CheckFuncResult(*small), nullptr);
    EXPECT_EQ(wrapper->CheckFuncResult(*hugeViaLambda), nullptr); // skipped as huge via lambda
    // Second run: ChooseAnalysisStrategy SkipAnalysis (already analysed)
    wrapper->RunOnPackage(package, false, 1, *diag);
    EXPECT_NE(wrapper->CheckFuncResult(*small), nullptr);
}

TEST_F(ConstAnalysisIRFixture, WrapperParallelActivePoolAndReadonlyStructGV)
{
    // Mid-size => ActiveStatePool on parallel path.
    auto* midTy = builder->GetType<FuncType>(std::vector<Type*>{}, builder->GetUnitTy());
    auto* mid = builder->CreateFunction(midTy, "mid_par", "mid_par", "", "default");
    auto* midBody = builder->CreateBlockGroup(*mid);
    mid->InitBody(*midBody);
    Block* prev = nullptr;
    for (int i = 0; i < 320; ++i) {
        auto* b = builder->CreateBlock(midBody);
        if (i == 0) {
            midBody->SetEntryBlock(b);
        } else {
            prev->AppendExpression(builder->CreateTerminator<GoTo>(b, prev));
        }
        if (i + 1 == 320) {
            // Put a const add in last block so ActiveStatePool Instantiation does real work if visited.
            auto* a = builder->CreateConstantExpression<IntLiteral>(builder->GetInt64Ty(), b, 1);
            b->AppendExpression(a);
            auto* add = builder->CreateExpression<BinaryExpression>(
                builder->GetInt64Ty(), BinaryExprKind::ADD, a->GetResult(), a->GetResult(), OverflowStrategy::THROWING,
                b);
            b->AppendExpression(add);
            b->AppendExpression(builder->CreateTerminator<Exit>(b));
        }
        prev = b;
    }

    // Readonly struct GV => IsTrackedGV STRUCT branch + SetUpGlobalVarState init analysis.
    auto* stDef = builder->CreateStruct(INVALID_LOCATION, "S", "S", "default", false);
    auto* stTy = builder->GetType<StructType>(stDef);
    stDef->SetType(*stTy);
    MemberVarInfo mi;
    mi.name = "x";
    mi.type = builder->GetInt64Ty();
    stDef->AddInstanceVar(mi);
    auto* gvTy = builder->GetType<RefType>(stTy);
    auto* gv = builder->CreateGlobalVar(gvTy, "g_s", "g_s", "g_s", "default");
    gv->EnableAttr(Attribute::READONLY);
    auto* initTy = builder->GetType<FuncType>(std::vector<Type*>{}, builder->GetUnitTy());
    auto* init = builder->CreateFunction(initTy, "g_s_init", "g_s_init", "", "default");
    auto* iBody = builder->CreateBlockGroup(*init);
    init->InitBody(*iBody);
    auto* iBlock = builder->CreateBlock(iBody);
    iBody->SetEntryBlock(iBlock);
    iBlock->AppendExpression(builder->CreateTerminator<Exit>(iBlock));
    gv->SetInitFunc(*init);

    wrapper->RunOnPackage(package, false, 2, *diag);
    EXPECT_NE(wrapper->CheckFuncActiveResult(*mid), nullptr);
    EXPECT_NE(wrapper->CheckFuncResult(*init), nullptr);
}

