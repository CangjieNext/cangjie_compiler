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
 * `Any` and `Object` stand in for two such incomparable interface types: neither is a sub type of
 * the other, and both are class types, as required by `GenericType::SetUpperBounds`.
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

    auto anyTy = builder.GetAnyTy();
    auto objectTy = builder.GetObjectTy();
    ASSERT_NE(anyTy, objectTy);
    // the bounds are incomparable, so the bound that matches has to be picked by identity
    ASSERT_FALSE(anyTy->IsEqualOrSubTypeOf(*objectTy, builder));

    auto genericTy = builder.GetType<GenericType>("T1", "T1");
    genericTy->SetUpperBounds({anyTy, objectTy});

    // `Object` is one of `T1`'s upper bounds, so `T1 <: Object` holds even though `Any <: Object`
    // does not. This failed while every upper bound had to be a sub type of the parent.
    EXPECT_TRUE(genericTy->IsEqualOrSubTypeOf(*objectTy, builder));
    // and the matching bound still works when it is not the last one
    EXPECT_TRUE(genericTy->IsEqualOrSubTypeOf(*anyTy, builder));
}

// The flip side: a generic type whose upper bounds all fail the test is still not a sub type of the
// parent. Guards against the check degrading into an unconditional `true`.
TEST(GenericTypeSubtypeTest, IsNotSubTypeOfAnUnrelatedType)
{
    std::unordered_map<unsigned int, std::string> fileNameMap;
    CHIRContext cctx(&fileNameMap);
    CHIRBuilder builder(cctx);

    auto anyTy = builder.GetAnyTy();
    auto objectTy = builder.GetObjectTy();
    ASSERT_FALSE(anyTy->IsEqualOrSubTypeOf(*objectTy, builder));

    auto genericTy = builder.GetType<GenericType>("T2", "T2");
    genericTy->SetUpperBounds({anyTy});

    EXPECT_FALSE(genericTy->IsEqualOrSubTypeOf(*objectTy, builder));
}

// A generic type with no upper bounds is only known to be a sub type of `Any`, like the `T` of
// `func id<T>(a: T)`. Nothing else follows from an empty set of bounds, so it must not be treated as
// a sub type of every type.
TEST(GenericTypeSubtypeTest, NoUpperBoundsOnlyImpliesAny)
{
    std::unordered_map<unsigned int, std::string> fileNameMap;
    CHIRContext cctx(&fileNameMap);
    CHIRBuilder builder(cctx);

    auto anyTy = builder.GetAnyTy();
    auto objectTy = builder.GetObjectTy();
    ASSERT_FALSE(anyTy->IsEqualOrSubTypeOf(*objectTy, builder));

    auto genericTy = builder.GetType<GenericType>("T3", "T3");
    ASSERT_TRUE(genericTy->GetUpperBounds().empty());

    // `T` is bounded by `Any` implicitly, which is why this one holds
    EXPECT_TRUE(genericTy->IsEqualOrSubTypeOf(*anyTy, builder));
    // and this one does not
    EXPECT_FALSE(genericTy->IsEqualOrSubTypeOf(*objectTy, builder));
}
