// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <gtest/gtest.h>

#include "cangjie/CHIR/IR/CHIRBuilder.h"

using namespace Cangjie::CHIR;

/*
 * `Type::IsEqualOrSubTypeOf` used to ask EVERY upper bound of a generic type to be a sub type of the
 * parent, which made `T <: P` unprovable as soon as one bound did not match.
 *
 * A generic type is a sub type of each of its upper bounds, so `T <: P` holds as soon as ANY one of
 * them is a sub type of `P`. Requiring all of them rejected valid CHIR, e.g. the CHIR checker
 * reported "the 2nd instantiated type(s) don't satisfy the generic constraints" for
 *
 *  interface I {}
 *  interface J {}
 *  enum E<T> where T <: I { A }
 *
 *  extend<T> E<T> where T <: J {
 *      func derivingChecks(): Unit {
 *          func check<S>(): Unit where S <: J {}
 *          let _ = check<T>
 *      }
 *  }
 *
 * Here `T` has two upper bounds: `I`, implied by the extended type `E<T>`, and `J`, declared by the
 * extend itself. `T <: J` follows from the `J` bound alone, even though `I <: J` does not hold.
 *
 * These tests stand `Array<Int64>` and `Array<Bool>` in for two such incomparable interface types.
 * A bare `CHIRContext` is enough to build them, while a class type would need a real `ClassDef`;
 * both are reference types, as `GenericType::SetUpperBounds` requires of an upper bound.
 *
 * A generic type with no upper bound at all is the other half of the same reasoning: `func
 * id<T>(a: T)` constrains `T` to `Any` and to nothing else, so no other sub-typing relation is
 * provable from it. It used to pass every check, because the loop over the empty set of bounds was
 * vacuously true.
 */
TEST(GenericTypeSubtypeTest, IsSubTypeOfAnyOfItsUpperBounds)
{
    std::unordered_map<unsigned int, std::string> fileNameMap;
    CHIRContext cctx(&fileNameMap);
    CHIRBuilder builder(cctx);

    auto int64ArrTy = builder.GetType<RawArrayType>(builder.GetInt64Ty(), 1u);
    auto boolArrTy = builder.GetType<RawArrayType>(builder.GetBoolTy(), 1u);
    // the two bounds are incomparable, so the matching one has to be picked out of the two
    ASSERT_FALSE(int64ArrTy->IsEqualOrSubTypeOf(*boolArrTy, builder));
    ASSERT_FALSE(boolArrTy->IsEqualOrSubTypeOf(*int64ArrTy, builder));

    auto genericTy = builder.GetType<GenericType>("T1", "T1");
    genericTy->SetUpperBounds({int64ArrTy, boolArrTy});

    // `Array<Bool>` is one of `T1`'s upper bounds, so `T1 <: Array<Bool>` holds even though the
    // other bound, `Array<Int64>`, is not a sub type of it. This failed while every upper bound had
    // to be a sub type of the parent.
    EXPECT_TRUE(genericTy->IsEqualOrSubTypeOf(*boolArrTy, builder));
    // and the same holds for the other bound
    EXPECT_TRUE(genericTy->IsEqualOrSubTypeOf(*int64ArrTy, builder));
}

// The flip side: a generic type whose upper bounds all fail the test is still not a sub type of the
// parent. Guards against the check degrading into an unconditional `true`.
TEST(GenericTypeSubtypeTest, IsNotSubTypeOfAnUnrelatedType)
{
    std::unordered_map<unsigned int, std::string> fileNameMap;
    CHIRContext cctx(&fileNameMap);
    CHIRBuilder builder(cctx);

    auto int64ArrTy = builder.GetType<RawArrayType>(builder.GetInt64Ty(), 1u);
    auto boolArrTy = builder.GetType<RawArrayType>(builder.GetBoolTy(), 1u);
    ASSERT_FALSE(int64ArrTy->IsEqualOrSubTypeOf(*boolArrTy, builder));

    auto genericTy = builder.GetType<GenericType>("T2", "T2");
    genericTy->SetUpperBounds({int64ArrTy});

    EXPECT_FALSE(genericTy->IsEqualOrSubTypeOf(*boolArrTy, builder));
}

// A generic type with no upper bounds, like the `T` of `func id<T>(a: T)`, is only known to follow
// `Any`. Nothing else follows from an empty set of bounds, so it must not be treated as a sub type
// of every type.
//
// The `T <: Any` half of that is decided further up in `IsEqualOrSubTypeOf`, before the generic
// branch, and is out of reach here: `Any` is a class type, and building one needs a real `ClassDef`
// from the core package, which a bare `CHIRContext` does not have.
TEST(GenericTypeSubtypeTest, NoUpperBoundsIsNotSubTypeOfEverything)
{
    std::unordered_map<unsigned int, std::string> fileNameMap;
    CHIRContext cctx(&fileNameMap);
    CHIRBuilder builder(cctx);

    auto int64ArrTy = builder.GetType<RawArrayType>(builder.GetInt64Ty(), 1u);

    auto genericTy = builder.GetType<GenericType>("T3", "T3");
    ASSERT_TRUE(genericTy->GetUpperBounds().empty());

    // nothing is provable from an empty set of bounds, so this must not hold
    EXPECT_FALSE(genericTy->IsEqualOrSubTypeOf(*int64ArrTy, builder));
}
