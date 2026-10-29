// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <gtest/gtest.h>

#include "cangjie/CHIR/IR/CHIRBuilder.h"
#include "cangjie/CHIR/IR/Type/ClassDef.h"
#include "cangjie/CHIR/IR/Type/EnumDef.h"
#include "cangjie/CHIR/IR/Type/StructDef.h"

using namespace Cangjie::CHIR;

// The verdict of a type test must only be a constant when it is a constant for *every* runtime
// value of the static source type.  A two-valued "is it a sub type" formulation is unsound: the
// source type may have a sub type that the target also describes, in which case the runtime answer
// depends on the actual value (issue #667).
class TypeTestVerdictTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        builder = std::make_unique<CHIRBuilder>(cctx);
        // `Any` must be registered in the context: Type::IsAny() identifies it by the core package
        // name, and CHIRContext::GetAnyTy() asserts when it was never registered.
        auto* anyDef = builder->CreateClass(
            INVALID_LOCATION, "Any", "Any", "std.core", /*isClass=*/true, /*isImported=*/false);
        auto* anyTy = builder->GetType<ClassType>(anyDef);
        anyDef->SetType(*anyTy);
        cctx.SetAnyTy(anyTy);
    }

    ClassType* MakeClass(ClassDef*& def, const std::string& name)
    {
        def = builder->CreateClass(INVALID_LOCATION, name, name, "default", /*isClass=*/true, /*isImported=*/false);
        auto ty = builder->GetType<ClassType>(def);
        def->SetType(*ty);
        return ty;
    }

    ClassType* MakeInterface(ClassDef*& def, const std::string& name)
    {
        def = builder->CreateClass(INVALID_LOCATION, name, name, "default", /*isClass=*/false, /*isImported=*/false);
        auto ty = builder->GetType<ClassType>(def);
        def->SetType(*ty);
        return ty;
    }

    StructType* MakeStruct(StructDef*& def, const std::string& name)
    {
        def = builder->CreateStruct(INVALID_LOCATION, name, name, "default", /*isImported=*/false);
        auto ty = builder->GetType<StructType>(def);
        def->SetType(*ty);
        return ty;
    }

    EnumType* MakeEnum(EnumDef*& def, const std::string& name)
    {
        def = builder->CreateEnum(
            INVALID_LOCATION, name, name, "default", /*isImported=*/false, /*isNonExhaustive=*/false);
        auto ty = builder->GetType<EnumType>(def);
        def->SetType(*ty);
        return ty;
    }

    TupleType* MakeTuple(std::vector<Type*> elems)
    {
        return builder->GetType<TupleType>(elems);
    }

    std::unordered_map<unsigned int, std::string> fileNameMap;
    CHIRContext cctx{&fileNameMap};
    std::unique_ptr<CHIRBuilder> builder;
};

TEST_F(TypeTestVerdictTest, SameTypeIsAlwaysTrue)
{
    ClassDef* a1 = nullptr;
    auto* a1Ty = MakeClass(a1, "A1");
    EXPECT_EQ(GetTypeTestVerdict(*a1Ty, *a1Ty, *builder), TypeTestVerdict::ALWAYS_TRUE);
}

TEST_F(TypeTestVerdictTest, SubtypeIsAlwaysTrue)
{
    ClassDef* a2 = nullptr;
    ClassDef* a3 = nullptr;
    auto* a2Ty = MakeClass(a2, "A2");
    auto* a3Ty = MakeClass(a3, "A3");
    a3->SetSuperClassTy(*a2Ty);

    // A3 <: A2 => every A3 value is an A2.
    EXPECT_EQ(GetTypeTestVerdict(*a3Ty, *a2Ty, *builder), TypeTestVerdict::ALWAYS_TRUE);
}

// Regression test for issue #667: the source static type A2 has a sub type A3 that the target type
// describes, so `x is A3` for an `x: A2` must stay a runtime test.  The old implementation folded
// `!A2.IsSubtypeOf(A3)` to `false`, which made the answer depend on the optimization level.
TEST_F(TypeTestVerdictTest, SupertypeOfTargetIsRuntime)
{
    ClassDef* a2 = nullptr;
    ClassDef* a3 = nullptr;
    auto* a2Ty = MakeClass(a2, "A2");
    auto* a3Ty = MakeClass(a3, "A3");
    a3->SetSuperClassTy(*a2Ty);

    EXPECT_EQ(GetTypeTestVerdict(*a2Ty, *a3Ty, *builder), TypeTestVerdict::RUNTIME);
}

TEST_F(TypeTestVerdictTest, UnrelatedClassesAreAlwaysFalse)
{
    ClassDef* a3 = nullptr;
    ClassDef* b = nullptr;
    auto* a3Ty = MakeClass(a3, "A3");
    auto* bTy = MakeClass(b, "B");

    // Cangjie classes use single inheritance: two classes that are not comparable cannot share a
    // runtime instance.
    EXPECT_EQ(GetTypeTestVerdict(*a3Ty, *bTy, *builder), TypeTestVerdict::ALWAYS_FALSE);
}

TEST_F(TypeTestVerdictTest, DifferentPrimitivesAreAlwaysFalse)
{
    auto* int64Ty = builder->GetInt64Ty();
    auto* boolTy = builder->GetBoolTy();
    EXPECT_EQ(GetTypeTestVerdict(*int64Ty, *boolTy, *builder), TypeTestVerdict::ALWAYS_FALSE);
}

TEST_F(TypeTestVerdictTest, ClassAgainstInterfaceIsRuntime)
{
    ClassDef* a2 = nullptr;
    ClassDef* i1 = nullptr;
    auto* a2Ty = MakeClass(a2, "A2");
    auto* i1Ty = MakeInterface(i1, "I1");

    // `extend A2 <: I1` may be added in any package, so the test can never be proven disjoint.
    EXPECT_EQ(GetTypeTestVerdict(*a2Ty, *i1Ty, *builder), TypeTestVerdict::RUNTIME);
    EXPECT_EQ(GetTypeTestVerdict(*i1Ty, *a2Ty, *builder), TypeTestVerdict::RUNTIME);
}

TEST_F(TypeTestVerdictTest, InterfaceAgainstInterfaceIsRuntime)
{
    ClassDef* i1 = nullptr;
    ClassDef* i2 = nullptr;
    auto* i1Ty = MakeInterface(i1, "I1");
    auto* i2Ty = MakeInterface(i2, "I2");
    EXPECT_EQ(GetTypeTestVerdict(*i1Ty, *i2Ty, *builder), TypeTestVerdict::RUNTIME);
}

// The exact shape of issue #667: `(A2, A2) is (A2, A3)` where the value is a `(A3, A3)`.
// Tuple tests are element-wise at runtime, so the second element decides the answer at runtime.
TEST_F(TypeTestVerdictTest, Issue667TupleShapeIsRuntime)
{
    ClassDef* a2 = nullptr;
    ClassDef* a3 = nullptr;
    auto* a2Ty = MakeClass(a2, "A2");
    auto* a3Ty = MakeClass(a3, "A3");
    a3->SetSuperClassTy(*a2Ty);

    auto* srcTy = MakeTuple({a2Ty, a2Ty});
    auto* targetTy = MakeTuple({a2Ty, a3Ty});
    EXPECT_EQ(GetTypeTestVerdict(*srcTy, *targetTy, *builder), TypeTestVerdict::RUNTIME);
}

// Negative control: a provably disjoint tuple must still be folded, otherwise the fix would just
// disable the optimization instead of making it correct.
TEST_F(TypeTestVerdictTest, DisjointTupleElementsAreAlwaysFalse)
{
    auto* int64Ty = builder->GetInt64Ty();
    auto* boolTy = builder->GetBoolTy();
    auto* srcTy = MakeTuple({int64Ty, int64Ty});
    auto* targetTy = MakeTuple({boolTy, boolTy});
    EXPECT_EQ(GetTypeTestVerdict(*srcTy, *targetTy, *builder), TypeTestVerdict::ALWAYS_FALSE);
}

TEST_F(TypeTestVerdictTest, TupleOfSubtypesIsAlwaysTrue)
{
    ClassDef* a2 = nullptr;
    ClassDef* a3 = nullptr;
    auto* a2Ty = MakeClass(a2, "A2");
    auto* a3Ty = MakeClass(a3, "A3");
    a3->SetSuperClassTy(*a2Ty);

    auto* srcTy = MakeTuple({a3Ty, a3Ty});
    auto* targetTy = MakeTuple({a2Ty, a2Ty});
    EXPECT_EQ(GetTypeTestVerdict(*srcTy, *targetTy, *builder), TypeTestVerdict::ALWAYS_TRUE);
}

TEST_F(TypeTestVerdictTest, TargetAnyIsAlwaysTrue)
{
    ClassDef* a2 = nullptr;
    auto* a2Ty = MakeClass(a2, "A2");
    auto* anyTy = builder->GetAnyTy();
    EXPECT_EQ(GetTypeTestVerdict(*a2Ty, *anyTy, *builder), TypeTestVerdict::ALWAYS_TRUE);
}

TEST_F(TypeTestVerdictTest, SourceAnyIsRuntime)
{
    ClassDef* a2 = nullptr;
    auto* a2Ty = MakeClass(a2, "A2");
    auto* anyTy = builder->GetAnyTy();
    EXPECT_EQ(GetTypeTestVerdict(*anyTy, *a2Ty, *builder), TypeTestVerdict::RUNTIME);
}

TEST_F(TypeTestVerdictTest, NothingTargetIsAlwaysFalse)
{
    ClassDef* a2 = nullptr;
    auto* a2Ty = MakeClass(a2, "A2");
    auto* nothingTy = builder->GetNothingType();
    EXPECT_EQ(GetTypeTestVerdict(*a2Ty, *nothingTy, *builder), TypeTestVerdict::ALWAYS_FALSE);
}

TEST_F(TypeTestVerdictTest, DifferentStructsAreAlwaysFalse)
{
    StructDef* s1 = nullptr;
    StructDef* s2 = nullptr;
    auto* s1Ty = MakeStruct(s1, "S1");
    auto* s2Ty = MakeStruct(s2, "S2");
    EXPECT_EQ(GetTypeTestVerdict(*s1Ty, *s2Ty, *builder), TypeTestVerdict::ALWAYS_FALSE);
}

TEST_F(TypeTestVerdictTest, DifferentEnumsAreAlwaysFalse)
{
    EnumDef* e1 = nullptr;
    EnumDef* e2 = nullptr;
    auto* e1Ty = MakeEnum(e1, "E1");
    auto* e2Ty = MakeEnum(e2, "E2");
    EXPECT_EQ(GetTypeTestVerdict(*e1Ty, *e2Ty, *builder), TypeTestVerdict::ALWAYS_FALSE);
}

TEST_F(TypeTestVerdictTest, ReferencesAreStripped)
{
    ClassDef* a2 = nullptr;
    ClassDef* a3 = nullptr;
    auto* a2Ty = MakeClass(a2, "A2");
    auto* a3Ty = MakeClass(a3, "A3");
    a3->SetSuperClassTy(*a2Ty);
    auto* refA3 = builder->GetType<RefType>(a3Ty);

    EXPECT_EQ(GetTypeTestVerdict(*refA3, *a2Ty, *builder), TypeTestVerdict::ALWAYS_TRUE);
}

// Unmodelled type shapes must stay conservative: extending the type system must never turn an
// unhandled case into a wrong constant fold.
TEST_F(TypeTestVerdictTest, UnmodelledShapesStayConservative)
{
    auto* int64Ty = builder->GetInt64Ty();
    auto* boolTy = builder->GetBoolTy();
    auto* arrInt64 = builder->GetType<RawArrayType>(int64Ty, 1);
    auto* arrBool = builder->GetType<RawArrayType>(boolTy, 1);
    EXPECT_EQ(GetTypeTestVerdict(*arrInt64, *arrBool, *builder), TypeTestVerdict::RUNTIME);

    auto* varr1 = builder->GetType<VArrayType>(int64Ty, 2);
    auto* varr2 = builder->GetType<VArrayType>(int64Ty, 3);
    EXPECT_EQ(GetTypeTestVerdict(*varr1, *varr2, *builder), TypeTestVerdict::RUNTIME);
}
