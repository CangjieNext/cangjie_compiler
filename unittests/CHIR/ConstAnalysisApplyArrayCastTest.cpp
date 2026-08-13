// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * ConstAnalysis Array/Range apply helpers, casts, and intrinsic/try paths.
 */


#include "ConstAnalysisIRFixture.h"

TEST_F(ConstAnalysisIRFixture, ArrayInitZeroArgSetsLen)
{
    auto* arrayDef = builder->CreateStruct(INVALID_LOCATION, "Array", "Array", "std.core", false);
    auto* arrayTy = builder->GetType<StructType>(arrayDef);
    arrayDef->SetType(*arrayTy);
    for (const char* name : {"rawptr", "start", "len"}) {
        MemberVarInfo info;
        info.name = name;
        info.type = builder->GetInt64Ty();
        arrayDef->AddInstanceVar(info);
    }

    auto* thisTy = builder->GetType<RefType>(arrayTy);
    auto* initTy = builder->GetType<FuncType>(std::vector<Type*>{thisTy}, builder->GetUnitTy());
    auto* init = builder->CreateFunction(initTy, "Array_init", "init", "", "std.core");
    arrayDef->AddMethod(init);
    auto* initBody = builder->CreateBlockGroup(*init);
    init->InitBody(*initBody);
    auto* initBlock = builder->CreateBlock(initBody);
    initBody->SetEntryBlock(initBlock);
    initBlock->AppendExpression(builder->CreateTerminator<Exit>(initBlock));

    NewFunc();
    auto* alloc = builder->CreateExpression<Allocate>(thisTy, arrayTy, curBlock);
    curBlock->AppendExpression(alloc);
    auto* apply = builder->CreateExpression<Apply>(
        builder->GetUnitTy(), init, FuncCallContext{.args = {alloc->GetResult()}}, curBlock);
    curBlock->AppendExpression(apply);

    // sizeget
    auto* sizegetTy = builder->GetType<FuncType>(std::vector<Type*>{thisTy}, builder->GetInt64Ty());
    auto* sizeget = builder->CreateFunction(sizegetTy, "Array_sizeget", "$sizeget", "", "std.core");
    arrayDef->AddMethod(sizeget);
    auto* sgBody = builder->CreateBlockGroup(*sizeget);
    sizeget->InitBody(*sgBody);
    auto* sgBlock = builder->CreateBlock(sgBody);
    sgBody->SetEntryBlock(sgBlock);
    sgBlock->AppendExpression(builder->CreateTerminator<Exit>(sgBlock));
    auto* sizeApply = builder->CreateExpression<Apply>(
        builder->GetInt64Ty(), sizeget, FuncCallContext{.args = {alloc->GetResult()}}, curBlock);
    curBlock->AppendExpression(sizeApply);

    // get(index) with const index to exercise bounds check path
    auto* getTy =
        builder->GetType<FuncType>(std::vector<Type*>{thisTy, builder->GetInt64Ty()}, builder->GetInt64Ty());
    auto* get = builder->CreateFunction(getTy, "Array_get", "get", "", "std.core");
    arrayDef->AddMethod(get);
    auto* getBody = builder->CreateBlockGroup(*get);
    get->InitBody(*getBody);
    auto* getBlock = builder->CreateBlock(getBody);
    getBody->SetEntryBlock(getBlock);
    getBlock->AppendExpression(builder->CreateTerminator<Exit>(getBlock));
    auto* idx = LitInt(builder->GetInt64Ty(), 0);
    auto* getApply = builder->CreateExpression<Apply>(
        builder->GetInt64Ty(), get, FuncCallContext{.args = {alloc->GetResult(), idx}}, curBlock);
    curBlock->AppendExpression(getApply);

    FinishWithExit();
    ASSERT_NE(Analyse(curFunc), nullptr);
}

TEST_F(ConstAnalysisIRFixture, RangeInitStepZeroAndNonZero)
{
    auto* rangeDef = builder->CreateStruct(INVALID_LOCATION, "Range", "Range", "std.core", false);
    auto* rangeTy = builder->GetType<StructType>(rangeDef);
    rangeDef->SetType(*rangeTy);

    auto* thisTy = builder->GetType<RefType>(rangeTy);
    // init(this, start, end, step, hasStart, hasEnd, isClosed) -> 7 args
    std::vector<Type*> params{
        thisTy, builder->GetInt64Ty(), builder->GetInt64Ty(), builder->GetInt64Ty(), builder->GetBoolTy(),
        builder->GetBoolTy(), builder->GetBoolTy()};
    auto* initTy = builder->GetType<FuncType>(params, builder->GetUnitTy());
    auto* init = builder->CreateFunction(initTy, "Range_init", "init", "", "std.core");
    rangeDef->AddMethod(init);
    auto* initBody = builder->CreateBlockGroup(*init);
    init->InitBody(*initBody);
    auto* initBlock = builder->CreateBlock(initBody);
    initBody->SetEntryBlock(initBlock);
    initBlock->AppendExpression(builder->CreateTerminator<Exit>(initBlock));

    auto BuildCall = [&](int64_t step) {
        NewFunc();
        auto* alloc = builder->CreateExpression<Allocate>(thisTy, rangeTy, curBlock);
        curBlock->AppendExpression(alloc);
        auto* start = LitInt(builder->GetInt64Ty(), 0);
        auto* end = LitInt(builder->GetInt64Ty(), 10);
        auto* st = LitInt(builder->GetInt64Ty(), static_cast<uint64_t>(step));
        auto* hs = LitBool(true);
        auto* he = LitBool(true);
        auto* ic = LitBool(true);
        auto* apply = builder->CreateExpression<Apply>(builder->GetUnitTy(), init,
            FuncCallContext{.args = {alloc->GetResult(), start, end, st, hs, he, ic}}, curBlock);
        curBlock->AppendExpression(apply);
        FinishWithExit();
        ASSERT_NE(Analyse(curFunc), nullptr);
    };
    BuildCall(1);
    BuildCall(0);
}

TEST_F(ConstAnalysisIRFixture, ApplyNonFunctionCalleeIsNoOp)
{
    NewFunc();
    // callee is a local var (not Function*) -> HandleApply returns NA
    auto* alloc = builder->CreateExpression<Allocate>(
        builder->GetType<RefType>(builder->GetInt64Ty()), builder->GetInt64Ty(), curBlock);
    curBlock->AppendExpression(alloc);
    // Can't easily Apply a non-function; use a Function without matching Array/Range info.
    auto* plain = builder->CreateFunction(
        builder->GetType<FuncType>(std::vector<Type*>{}, builder->GetUnitTy()), "plain", "plain", "", "default");
    auto* pBody = builder->CreateBlockGroup(*plain);
    plain->InitBody(*pBody);
    auto* pBlock = builder->CreateBlock(pBody);
    pBody->SetEntryBlock(pBlock);
    pBlock->AppendExpression(builder->CreateTerminator<Exit>(pBlock));
    auto* apply =
        builder->CreateExpression<Apply>(builder->GetUnitTy(), plain, FuncCallContext{}, curBlock);
    curBlock->AppendExpression(apply);
    FinishWithExit();
    ASSERT_NE(Analyse(curFunc), nullptr);
}

TEST_F(ConstAnalysisIRFixture, TypeCastWiderIntWidths)
{
    NewFunc();
    auto* v8 = LitInt(builder->GetInt8Ty(), 1);
    auto* to32 = builder->CreateExpression<NumericCast>(
        builder->GetInt32Ty(), v8, OverflowStrategy::THROWING, curBlock);
    curBlock->AppendExpression(to32);
    auto* to64 = builder->CreateExpression<NumericCast>(
        builder->GetInt64Ty(), to32->GetResult(), OverflowStrategy::THROWING, curBlock);
    curBlock->AppendExpression(to64);
    auto* toU = builder->CreateExpression<NumericCast>(
        builder->GetUInt64Ty(), to64->GetResult(), OverflowStrategy::WRAPPING, curBlock);
    curBlock->AppendExpression(toU);
    auto* u8 = LitInt(builder->GetUInt8Ty(), 255);
    auto* uTo64 = builder->CreateExpression<NumericCast>(
        builder->GetUInt64Ty(), u8, OverflowStrategy::THROWING, curBlock);
    curBlock->AppendExpression(uTo64);
    FinishWithExit();
    ASSERT_NE(Analyse(curFunc), nullptr);
}

TEST_F(ConstAnalysisIRFixture, ArraySliceAndInitWithLen)
{
    auto* arrayDef = builder->CreateStruct(INVALID_LOCATION, "Array", "Array2", "std.core", false);
    auto* arrayTy = builder->GetType<StructType>(arrayDef);
    arrayDef->SetType(*arrayTy);
    for (const char* name : {"rawptr", "start", "len"}) {
        MemberVarInfo info;
        info.name = name;
        info.type = builder->GetInt64Ty();
        arrayDef->AddInstanceVar(info);
    }
    auto* thisTy = builder->GetType<RefType>(arrayTy);

    // init(this, size, item) -> 3 args, propagate size to len
    auto* init3Ty = builder->GetType<FuncType>(
        std::vector<Type*>{thisTy, builder->GetInt64Ty(), builder->GetInt64Ty()}, builder->GetUnitTy());
    auto* init3 = builder->CreateFunction(init3Ty, "Array_init3", "init", "", "std.core");
    arrayDef->AddMethod(init3);
    auto* i3Body = builder->CreateBlockGroup(*init3);
    init3->InitBody(*i3Body);
    auto* i3Block = builder->CreateBlock(i3Body);
    i3Body->SetEntryBlock(i3Block);
    i3Block->AppendExpression(builder->CreateTerminator<Exit>(i3Block));

    // slice(this, start, len) -> Array
    auto* sliceTy = builder->GetType<FuncType>(
        std::vector<Type*>{thisTy, builder->GetInt64Ty(), builder->GetInt64Ty()}, arrayTy);
    auto* slice = builder->CreateFunction(sliceTy, "Array_slice", "slice", "", "std.core");
    arrayDef->AddMethod(slice);
    auto* sBody = builder->CreateBlockGroup(*slice);
    slice->InitBody(*sBody);
    auto* sBlock = builder->CreateBlock(sBody);
    sBody->SetEntryBlock(sBlock);
    sBlock->AppendExpression(builder->CreateTerminator<Exit>(sBlock));

    NewFunc();
    auto* alloc = builder->CreateExpression<Allocate>(thisTy, arrayTy, curBlock);
    curBlock->AppendExpression(alloc);
    auto* size = LitInt(builder->GetInt64Ty(), 5);
    auto* item = LitInt(builder->GetInt64Ty(), 0);
    auto* initApply = builder->CreateExpression<Apply>(
        builder->GetUnitTy(), init3, FuncCallContext{.args = {alloc->GetResult(), size, item}}, curBlock);
    curBlock->AppendExpression(initApply);
    auto* start = LitInt(builder->GetInt64Ty(), 1);
    auto* len = LitInt(builder->GetInt64Ty(), 2);
    auto* sliceApply = builder->CreateExpression<Apply>(
        arrayTy, slice, FuncCallContext{.args = {alloc->GetResult(), start, len}}, curBlock);
    curBlock->AppendExpression(sliceApply);
    // out-of-bounds get
    auto* getTy =
        builder->GetType<FuncType>(std::vector<Type*>{thisTy, builder->GetInt64Ty()}, builder->GetInt64Ty());
    auto* get = builder->CreateFunction(getTy, "Array_get2", "get", "", "std.core");
    arrayDef->AddMethod(get);
    auto* gBody = builder->CreateBlockGroup(*get);
    get->InitBody(*gBody);
    auto* gBlock = builder->CreateBlock(gBody);
    gBody->SetEntryBlock(gBlock);
    gBlock->AppendExpression(builder->CreateTerminator<Exit>(gBlock));
    auto* badIdx = LitInt(builder->GetInt64Ty(), 99);
    auto* getApply = builder->CreateExpression<Apply>(
        builder->GetInt64Ty(), get, FuncCallContext{.args = {alloc->GetResult(), badIdx}}, curBlock);
    curBlock->AppendExpression(getApply);
    FinishWithExit();
    ASSERT_NE(AnalyseWithDiagnostics(curFunc), nullptr);
}

TEST_F(ConstAnalysisIRFixture, TryApplyIntrinsicClassStaticCastAndNegOverflow)
{
    NewFunc();
    // ClassStaticCast of a class ref
    auto* clsDef = builder->CreateClass(INVALID_LOCATION, "C", "C", "default", true, false);
    auto* clsTy = builder->GetType<ClassType>(clsDef);
    clsDef->SetType(*clsTy);
    auto* refTy = builder->GetType<RefType>(clsTy);
    auto* alloc = builder->CreateExpression<Allocate>(refTy, clsTy, curBlock);
    curBlock->AppendExpression(alloc);
    auto* csc = builder->CreateExpression<ClassStaticCast>(refTy, alloc->GetResult(), curBlock);
    curBlock->AppendExpression(csc);

    // VArray get/set intrinsics (in-bounds + OOB + negative index)
    auto* varrTy = builder->GetType<VArrayType>(builder->GetInt64Ty(), 2);
    auto* varrRefTy = builder->GetType<RefType>(varrTy);
    auto* valloc = builder->CreateExpression<Allocate>(varrRefTy, varrTy, curBlock);
    curBlock->AppendExpression(valloc);
    auto* idx0 = LitInt(builder->GetInt64Ty(), 0);
    auto* idxBad = LitInt(builder->GetInt64Ty(), 9);
    auto* idxNeg = LitInt(builder->GetInt64Ty(), static_cast<uint64_t>(-1));
    auto* val = LitInt(builder->GetInt64Ty(), 42);
    // VARRAY_GET(arr, index) — arr is the VArray value type for Get; use allocated object as arg.
    // HandleVArrayGet expects args[0] type IsVArray(); so pass a VArray-typed local.
    auto* varrLit = builder->CreateExpression<VArray>(
        varrTy, std::vector<Value*>{LitInt(builder->GetInt64Ty(), 1), LitInt(builder->GetInt64Ty(), 2)}, curBlock);
    curBlock->AppendExpression(varrLit);
    IntrisicCallContext getCtx{.kind = IntrinsicKind::VARRAY_GET, .args = {varrLit->GetResult(), idx0}};
    auto* iget = builder->CreateExpression<Intrinsic>(builder->GetInt64Ty(), getCtx, curBlock);
    curBlock->AppendExpression(iget);
    IntrisicCallContext getOob{.kind = IntrinsicKind::VARRAY_GET, .args = {varrLit->GetResult(), idxBad}};
    auto* iget2 = builder->CreateExpression<Intrinsic>(builder->GetInt64Ty(), getOob, curBlock);
    curBlock->AppendExpression(iget2);
    IntrisicCallContext setCtx{.kind = IntrinsicKind::VARRAY_SET, .args = {valloc->GetResult(), val, idx0}};
    auto* iset = builder->CreateExpression<Intrinsic>(builder->GetUnitTy(), setCtx, curBlock);
    curBlock->AppendExpression(iset);
    IntrisicCallContext setNeg{.kind = IntrinsicKind::VARRAY_SET, .args = {valloc->GetResult(), val, idxNeg}};
    auto* iset2 = builder->CreateExpression<Intrinsic>(builder->GetUnitTy(), setNeg, curBlock);
    curBlock->AppendExpression(iset2);
    // Non-varray intrinsic still hits HandleIntrinsic SetToTop path
    IntrisicCallContext other{.kind = IntrinsicKind::SIZE_OF, .args = {}};
    auto* isize = builder->CreateExpression<Intrinsic>(builder->GetInt64Ty(), other, curBlock);
    curBlock->AppendExpression(isize);

    // Int8 min NEG overflow => GenerateTypeRangePrompt via Raise path
    auto* min8 = LitInt(builder->GetInt8Ty(), static_cast<uint64_t>(-128));
    auto* neg = builder->CreateExpression<UnaryExpression>(
        builder->GetInt8Ty(), UnaryExprKind::NEG, min8, OverflowStrategy::THROWING, curBlock);
    curBlock->AppendExpression(neg);

    // Narrowing cast overflow => cast GenerateTypeRangePrompt
    auto* big = LitInt(builder->GetInt64Ty(), 1000);
    auto* cast = builder->CreateExpression<NumericCast>(
        builder->GetInt8Ty(), big, OverflowStrategy::THROWING, curBlock);
    curBlock->AppendExpression(cast);

    // TryApply to a simple function (success path / NA)
    auto* calleeTy = builder->GetType<FuncType>(std::vector<Type*>{}, builder->GetUnitTy());
    auto* callee = builder->CreateFunction(calleeTy, "ta_cal", "ta_cal", "", "default");
    auto* cBody = builder->CreateBlockGroup(*callee);
    callee->InitBody(*cBody);
    auto* cBlock = builder->CreateBlock(cBody);
    cBody->SetEntryBlock(cBlock);
    cBlock->AppendExpression(builder->CreateTerminator<Exit>(cBlock));
    auto* ok = NewBlock();
    auto* err = NewBlock();
    auto* tryApply = builder->CreateExpression<TryApply>(
        builder->GetUnitTy(), callee, FuncCallContext{.args = {}}, ok, err, curBlock);
    curBlock->AppendExpression(tryApply);
    curBlock = ok;
    FinishWithExit();
    curBlock = err;
    FinishWithExit();

    ASSERT_NE(AnalyseWithDiagnostics(curFunc), nullptr);
    ASSERT_NE(AnalysePool(curFunc), nullptr);
}

TEST_F(ConstAnalysisIRFixture, ArrayInitFourArgsAndUIntTrivialOps)
{
    auto* arrayDef = builder->CreateStruct(INVALID_LOCATION, "Array", "Array4", "std.core", false);
    auto* arrayTy = builder->GetType<StructType>(arrayDef);
    arrayDef->SetType(*arrayTy);
    for (const char* name : {"rawptr", "start", "len"}) {
        MemberVarInfo info;
        info.name = name;
        info.type = builder->GetInt64Ty();
        arrayDef->AddInstanceVar(info);
    }
    auto* thisTy = builder->GetType<RefType>(arrayTy);
    auto* rawArrTy = builder->GetType<RawArrayType>(builder->GetInt64Ty(), 1);
    auto* init4Ty = builder->GetType<FuncType>(
        std::vector<Type*>{thisTy, rawArrTy, builder->GetInt64Ty(), builder->GetInt64Ty()}, builder->GetUnitTy());
    auto* init4 = builder->CreateFunction(init4Ty, "Array_init4", "init", "", "std.core");
    arrayDef->AddMethod(init4);
    auto* iBody = builder->CreateBlockGroup(*init4);
    init4->InitBody(*iBody);
    auto* iBlock = builder->CreateBlock(iBody);
    iBody->SetEntryBlock(iBlock);
    iBlock->AppendExpression(builder->CreateTerminator<Exit>(iBlock));

    NewFunc();
    auto* alloc = builder->CreateExpression<Allocate>(thisTy, arrayTy, curBlock);
    curBlock->AppendExpression(alloc);
    auto* rawAlloc = builder->CreateExpression<Allocate>(
        builder->GetType<RefType>(rawArrTy), rawArrTy, curBlock);
    curBlock->AppendExpression(rawAlloc);
    auto* start = LitInt(builder->GetInt64Ty(), 0);
    auto* len = LitInt(builder->GetInt64Ty(), 3);
    auto* apply = builder->CreateExpression<Apply>(builder->GetUnitTy(), init4,
        FuncCallContext{.args = {alloc->GetResult(), rawAlloc->GetResult(), start, len}}, curBlock);
    curBlock->AppendExpression(apply);

    // UInt trivial: a*0, 0/a, a%1, a/0
    auto* u = LitInt(builder->GetUInt64Ty(), 5);
    auto* z = LitInt(builder->GetUInt64Ty(), 0);
    auto* one = LitInt(builder->GetUInt64Ty(), 1);
    curBlock->AppendExpression(builder->CreateExpression<BinaryExpression>(
        builder->GetUInt64Ty(), BinaryExprKind::MUL, u, z, OverflowStrategy::THROWING, curBlock));
    curBlock->AppendExpression(builder->CreateExpression<BinaryExpression>(
        builder->GetUInt64Ty(), BinaryExprKind::DIV, z, u, OverflowStrategy::THROWING, curBlock));
    curBlock->AppendExpression(builder->CreateExpression<BinaryExpression>(
        builder->GetUInt64Ty(), BinaryExprKind::MOD, u, one, OverflowStrategy::THROWING, curBlock));
    curBlock->AppendExpression(builder->CreateExpression<BinaryExpression>(
        builder->GetUInt64Ty(), BinaryExprKind::DIV, u, z, OverflowStrategy::THROWING, curBlock));
    // UInt EXP and float nan path
    curBlock->AppendExpression(builder->CreateExpression<BinaryExpression>(
        builder->GetUInt64Ty(), BinaryExprKind::EXP, u, LitInt(builder->GetUInt64Ty(), 2), OverflowStrategy::THROWING,
        curBlock));
    auto* f1 = LitFloat(builder->GetFloat64Ty(), 1.0);
    auto* f0 = LitFloat(builder->GetFloat64Ty(), 0.0);
    curBlock->AppendExpression(builder->CreateExpression<BinaryExpression>(
        builder->GetFloat64Ty(), BinaryExprKind::DIV, f1, f0, OverflowStrategy::THROWING, curBlock));

    FinishWithExit();
    ASSERT_NE(AnalyseWithDiagnostics(curFunc), nullptr);
}

TEST_F(ConstAnalysisIRFixture, NumericCastMatrixAndArrayInitCollection)
{
    NewFunc();
    std::vector<Type*> tys = {builder->GetInt8Ty(), builder->GetInt16Ty(), builder->GetInt32Ty(),
        builder->GetInt64Ty(), builder->GetIntNativeTy(), builder->GetUInt8Ty(), builder->GetUInt16Ty(),
        builder->GetUInt32Ty(), builder->GetUInt64Ty(), builder->GetUIntNativeTy()};
    for (auto* sTy : tys) {
        auto* src = LitInt(sTy, 7);
        for (auto* dTy : tys) {
            curBlock->AppendExpression(
                builder->CreateExpression<NumericCast>(dTy, src, OverflowStrategy::WRAPPING, curBlock));
        }
    }
    auto* big = LitInt(builder->GetInt32Ty(), 100000);
    curBlock->AppendExpression(
        builder->CreateExpression<NumericCast>(builder->GetInt8Ty(), big, OverflowStrategy::THROWING, curBlock));

    auto* arrayDef = builder->CreateStruct(INVALID_LOCATION, "Array", "ArrayBox", "std.core", false);
    auto* arrayTy = builder->GetType<StructType>(arrayDef);
    arrayDef->SetType(*arrayTy);
    for (const char* name : {"rawptr", "start", "len"}) {
        MemberVarInfo info;
        info.name = name;
        info.type = builder->GetInt64Ty();
        arrayDef->AddInstanceVar(info);
    }
    auto* boxDef = builder->CreateClass(INVALID_LOCATION, "$Box_Array", "$Box_Array", "std.core", true, false);
    auto* boxTy = builder->GetType<ClassType>(boxDef);
    boxDef->SetType(*boxTy);
    MemberVarInfo boxMem;
    boxMem.name = "$value";
    boxMem.type = arrayTy;
    boxDef->AddInstanceVar(boxMem);
    auto* thisTy = builder->GetType<RefType>(arrayTy);
    auto* boxRefTy = builder->GetType<RefType>(boxTy);
    auto* init2Ty = builder->GetType<FuncType>(std::vector<Type*>{thisTy, boxRefTy}, builder->GetUnitTy());
    auto* init2 = builder->CreateFunction(init2Ty, "Array_init2", "init", "", "std.core");
    arrayDef->AddMethod(init2);
    auto* iBody = builder->CreateBlockGroup(*init2);
    init2->InitBody(*iBody);
    auto* iBlock = builder->CreateBlock(iBody);
    iBody->SetEntryBlock(iBlock);
    iBlock->AppendExpression(builder->CreateTerminator<Exit>(iBlock));
    auto* alloc = builder->CreateExpression<Allocate>(thisTy, arrayTy, curBlock);
    curBlock->AppendExpression(alloc);
    auto* boxAlloc = builder->CreateExpression<Allocate>(boxRefTy, boxTy, curBlock);
    curBlock->AppendExpression(boxAlloc);
    curBlock->AppendExpression(builder->CreateExpression<Apply>(builder->GetUnitTy(), init2,
        FuncCallContext{.args = {alloc->GetResult(), boxAlloc->GetResult()}}, curBlock));

    auto* varrTy = builder->GetType<VArrayType>(builder->GetInt64Ty(), 2);
    auto* varrLit = builder->CreateExpression<VArray>(
        varrTy, std::vector<Value*>{LitInt(builder->GetInt64Ty(), 1), LitInt(builder->GetInt64Ty(), 2)}, curBlock);
    curBlock->AppendExpression(varrLit);
    auto* ok = NewBlock();
    auto* err = NewBlock();
    IntrisicCallContext getCtx{
        .kind = IntrinsicKind::VARRAY_GET, .args = {varrLit->GetResult(), LitInt(builder->GetInt64Ty(), 0)}};
    curBlock->AppendExpression(
        builder->CreateExpression<TryIntrinsic>(builder->GetInt64Ty(), getCtx, ok, err, curBlock));
    curBlock = ok;
    FinishWithExit();
    curBlock = err;
    FinishWithExit();

    ASSERT_NE(AnalyseWithDiagnostics(curFunc), nullptr);
    ASSERT_NE(AnalysePool(curFunc), nullptr);
}

TEST_F(ConstAnalysisIRFixture, TryApplyRangeVArraySetAndCollectionLen)
{
    auto* rangeDef = builder->CreateStruct(INVALID_LOCATION, "Range", "Range2", "std.core", false);
    auto* rangeTy = builder->GetType<StructType>(rangeDef);
    rangeDef->SetType(*rangeTy);
    auto* thisTy = builder->GetType<RefType>(rangeTy);
    std::vector<Type*> params{
        thisTy, builder->GetInt64Ty(), builder->GetInt64Ty(), builder->GetInt64Ty(), builder->GetBoolTy(),
        builder->GetBoolTy(), builder->GetBoolTy()};
    auto* initTy = builder->GetType<FuncType>(params, builder->GetUnitTy());
    auto* init = builder->CreateFunction(initTy, "Range_init2", "init", "", "std.core");
    rangeDef->AddMethod(init);
    auto* initBody = builder->CreateBlockGroup(*init);
    init->InitBody(*initBody);
    auto* initBlock = builder->CreateBlock(initBody);
    initBody->SetEntryBlock(initBlock);
    initBlock->AppendExpression(builder->CreateTerminator<Exit>(initBlock));

    NewFunc();
    auto* alloc = builder->CreateExpression<Allocate>(thisTy, rangeTy, curBlock);
    curBlock->AppendExpression(alloc);
    auto* ok = NewBlock();
    auto* err = NewBlock();
    curBlock->AppendExpression(builder->CreateExpression<TryApply>(builder->GetUnitTy(), init,
        FuncCallContext{.args = {alloc->GetResult(), LitInt(builder->GetInt64Ty(), 0), LitInt(builder->GetInt64Ty(), 10),
            LitInt(builder->GetInt64Ty(), 1), LitBool(true), LitBool(true), LitBool(true)}},
        ok, err, curBlock));
    curBlock = ok;
    auto* alloc2 = builder->CreateExpression<Allocate>(thisTy, rangeTy, curBlock);
    curBlock->AppendExpression(alloc2);
    auto* ok2 = NewBlock();
    auto* err2 = NewBlock();
    curBlock->AppendExpression(builder->CreateExpression<TryApply>(builder->GetUnitTy(), init,
        FuncCallContext{.args = {alloc2->GetResult(), LitInt(builder->GetInt64Ty(), 0), LitInt(builder->GetInt64Ty(), 10),
            LitInt(builder->GetInt64Ty(), 0), LitBool(true), LitBool(true), LitBool(true)}},
        ok2, err2, curBlock));
    curBlock = err2;
    auto* iRef = builder->GetType<RefType>(builder->GetInt64Ty());
    auto* iAlloc = builder->CreateExpression<Allocate>(iRef, builder->GetInt64Ty(), curBlock);
    curBlock->AppendExpression(iAlloc);
    auto* unkStep = builder->CreateExpression<Load>(builder->GetInt64Ty(), iAlloc->GetResult(), curBlock);
    curBlock->AppendExpression(unkStep);
    auto* alloc3 = builder->CreateExpression<Allocate>(thisTy, rangeTy, curBlock);
    curBlock->AppendExpression(alloc3);
    curBlock->AppendExpression(builder->CreateExpression<Apply>(builder->GetUnitTy(), init,
        FuncCallContext{.args = {alloc3->GetResult(), LitInt(builder->GetInt64Ty(), 0), LitInt(builder->GetInt64Ty(), 1),
            unkStep->GetResult(), LitBool(true), LitBool(true), LitBool(true)}},
        curBlock));

    auto* varrTy = builder->GetType<VArrayType>(builder->GetInt64Ty(), 2);
    auto* varrRefTy = builder->GetType<RefType>(varrTy);
    auto* valloc = builder->CreateExpression<Allocate>(varrRefTy, varrTy, curBlock);
    curBlock->AppendExpression(valloc);
    auto* val = LitInt(builder->GetInt64Ty(), 7);
    auto* sok = NewBlock();
    auto* serr = NewBlock();
    IntrisicCallContext setCtx{
        .kind = IntrinsicKind::VARRAY_SET, .args = {valloc->GetResult(), val, LitInt(builder->GetInt64Ty(), 0)}};
    curBlock->AppendExpression(
        builder->CreateExpression<TryIntrinsic>(builder->GetUnitTy(), setCtx, sok, serr, curBlock));
    curBlock = sok;
    IntrisicCallContext setUnk{
        .kind = IntrinsicKind::VARRAY_SET, .args = {valloc->GetResult(), val, unkStep->GetResult()}};
    curBlock->AppendExpression(builder->CreateExpression<Intrinsic>(builder->GetUnitTy(), setUnk, curBlock));
    auto* varrLit = builder->CreateExpression<VArray>(
        varrTy, std::vector<Value*>{LitInt(builder->GetInt64Ty(), 1), LitInt(builder->GetInt64Ty(), 2)}, curBlock);
    curBlock->AppendExpression(varrLit);
    IntrisicCallContext badArity{.kind = IntrinsicKind::VARRAY_GET, .args = {varrLit->GetResult()}};
    curBlock->AppendExpression(builder->CreateExpression<Intrinsic>(builder->GetInt64Ty(), badArity, curBlock));
    IntrisicCallContext unkIdx{.kind = IntrinsicKind::VARRAY_GET, .args = {varrLit->GetResult(), unkStep->GetResult()}};
    curBlock->AppendExpression(builder->CreateExpression<Intrinsic>(builder->GetInt64Ty(), unkIdx, curBlock));

    auto* arrayDef = builder->CreateStruct(INVALID_LOCATION, "Array", "ArrayColl", "std.core", false);
    auto* arrayTy = builder->GetType<StructType>(arrayDef);
    arrayDef->SetType(*arrayTy);
    for (const char* name : {"rawptr", "start", "len"}) {
        MemberVarInfo info;
        info.name = name;
        info.type = builder->GetInt64Ty();
        arrayDef->AddInstanceVar(info);
    }
    auto* arrRefTy = builder->GetType<RefType>(arrayTy);
    auto* boxDef = builder->CreateClass(INVALID_LOCATION, "$Box_Array2", "$Box_Array2", "std.core", true, false);
    auto* boxTy = builder->GetType<ClassType>(boxDef);
    boxDef->SetType(*boxTy);
    MemberVarInfo boxMem;
    boxMem.name = "$value";
    boxMem.type = arrRefTy;
    boxDef->AddInstanceVar(boxMem);
    auto* boxRefTy = builder->GetType<RefType>(boxTy);
    auto* init2Ty = builder->GetType<FuncType>(std::vector<Type*>{arrRefTy, boxRefTy}, builder->GetUnitTy());
    auto* init2 = builder->CreateFunction(init2Ty, "Array_init_coll", "init", "", "std.core");
    arrayDef->AddMethod(init2);
    auto* iBody = builder->CreateBlockGroup(*init2);
    init2->InitBody(*iBody);
    auto* iBlock = builder->CreateBlock(iBody);
    iBody->SetEntryBlock(iBlock);
    iBlock->AppendExpression(builder->CreateTerminator<Exit>(iBlock));

    auto* destArr = builder->CreateExpression<Allocate>(arrRefTy, arrayTy, curBlock);
    curBlock->AppendExpression(destArr);
    auto* srcArr = builder->CreateExpression<Allocate>(arrRefTy, arrayTy, curBlock);
    curBlock->AppendExpression(srcArr);
    auto* init0Ty = builder->GetType<FuncType>(std::vector<Type*>{arrRefTy}, builder->GetUnitTy());
    auto* init0 = builder->CreateFunction(init0Ty, "Array_init0b", "init", "", "std.core");
    arrayDef->AddMethod(init0);
    auto* i0Body = builder->CreateBlockGroup(*init0);
    init0->InitBody(*i0Body);
    auto* i0Block = builder->CreateBlock(i0Body);
    i0Body->SetEntryBlock(i0Block);
    i0Block->AppendExpression(builder->CreateTerminator<Exit>(i0Block));
    curBlock->AppendExpression(builder->CreateExpression<Apply>(
        builder->GetUnitTy(), init0, FuncCallContext{.args = {srcArr->GetResult()}}, curBlock));
    auto* boxAlloc = builder->CreateExpression<Allocate>(boxRefTy, boxTy, curBlock);
    curBlock->AppendExpression(boxAlloc);
    curBlock->AppendExpression(builder->CreateExpression<StoreElementRef>(
        builder->GetUnitTy(), srcArr->GetResult(), boxAlloc->GetResult(), std::vector<uint64_t>{0}, curBlock));
    curBlock->AppendExpression(builder->CreateExpression<Apply>(builder->GetUnitTy(), init2,
        FuncCallContext{.args = {destArr->GetResult(), boxAlloc->GetResult()}}, curBlock));

    auto* getTy =
        builder->GetType<FuncType>(std::vector<Type*>{arrRefTy, builder->GetInt64Ty()}, builder->GetInt64Ty());
    auto* get = builder->CreateFunction(getTy, "Array_get_coll", "get", "", "std.core");
    arrayDef->AddMethod(get);
    auto* gBody = builder->CreateBlockGroup(*get);
    get->InitBody(*gBody);
    auto* gBlock = builder->CreateBlock(gBody);
    gBody->SetEntryBlock(gBlock);
    gBlock->AppendExpression(builder->CreateTerminator<Exit>(gBlock));
    auto* bareArr = builder->CreateExpression<Allocate>(arrRefTy, arrayTy, curBlock);
    curBlock->AppendExpression(bareArr);
    curBlock->AppendExpression(builder->CreateExpression<Apply>(builder->GetInt64Ty(), get,
        FuncCallContext{.args = {bareArr->GetResult(), LitInt(builder->GetInt64Ty(), 0)}}, curBlock));

    FinishWithExit();
    curBlock = err;
    FinishWithExit();
    curBlock = ok2;
    FinishWithExit();
    curBlock = serr;
    FinishWithExit();

    ASSERT_NE(AnalyseWithDiagnostics(curFunc), nullptr);
    ASSERT_NE(AnalysePool(curFunc), nullptr);
}

