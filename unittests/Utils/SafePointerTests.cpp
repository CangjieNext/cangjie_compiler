// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <unordered_set>
#include <utility>

#include "gtest/gtest.h"

#include "cangjie/Utils/SafePointer.h"

namespace {
struct Base {
    explicit Base(int v) : value(v)
    {
    }
    virtual ~Base()
    {
        ++destroyed;
    }
    virtual int Get() const
    {
        return value;
    }
    int value;
    static int destroyed;
};
int Base::destroyed = 0;

struct Derived : public Base {
    explicit Derived(int v) : Base(v)
    {
    }
    int Get() const override
    {
        return value * 2;
    }
};
} // namespace


TEST(SafePointerTest, OwnedPtrOwnsAndReleases)
{
    Base::destroyed = 0;
    {
        OwnedPtr<Base> owned = MakeOwned<Base>(7);
        ASSERT_TRUE(static_cast<bool>(owned));
        EXPECT_EQ(owned->Get(), 7);
        EXPECT_EQ((*owned).value, 7);
        EXPECT_TRUE(owned != nullptr);
        EXPECT_FALSE(owned == nullptr);
    }
    EXPECT_EQ(Base::destroyed, 1);

    Base::destroyed = 0;
    OwnedPtr<Base> released = MakeOwned<Base>(9);
    Base* raw = released.release();
    EXPECT_EQ(Base::destroyed, 0);
    EXPECT_TRUE(released == nullptr);
    delete raw;
    EXPECT_EQ(Base::destroyed, 1);
}

TEST(SafePointerTest, ResetDestroysThePreviousObject)
{
    Base::destroyed = 0;
    OwnedPtr<Base> owned = MakeOwned<Base>(1);

    // reset(raw) with a live payload must delete the old object.
    owned.reset(new Base(2));
    EXPECT_EQ(Base::destroyed, 1);
    EXPECT_EQ(owned->Get(), 2);

    // reset() with a live payload must delete it and leave the pointer empty.
    owned.reset();
    EXPECT_EQ(Base::destroyed, 2);
    EXPECT_TRUE(owned == nullptr);

    // reset() on an already empty pointer must not double-delete.
    owned.reset();
    EXPECT_EQ(Base::destroyed, 2);

    // Assigning nullptr goes down the same path.
    owned.reset(new Base(3));
    owned = nullptr;
    EXPECT_EQ(Base::destroyed, 3);
    EXPECT_TRUE(owned == nullptr);
}

TEST(SafePointerTest, SwapAndMoveTransferOwnership)
{
    Base::destroyed = 0;
    {
        OwnedPtr<Base> lhs = MakeOwned<Base>(1);
        OwnedPtr<Base> rhs = MakeOwned<Base>(2);
        lhs.swap(rhs);
        EXPECT_EQ(lhs->value, 2);
        EXPECT_EQ(rhs->value, 1);
        EXPECT_EQ(Base::destroyed, 0);

        OwnedPtr<Base> moved = std::move(lhs);
        EXPECT_EQ(moved->value, 2);
        EXPECT_TRUE(lhs == nullptr);

        // Move-assignment must destroy whatever the target held.
        moved = std::move(rhs);
        EXPECT_EQ(Base::destroyed, 1);
        EXPECT_EQ(moved->value, 1);
    }
    EXPECT_EQ(Base::destroyed, 2);
}

TEST(SafePointerTest, DerivedToBaseConversionKeepsDynamicDispatch)
{
    Base::destroyed = 0;
    {
        OwnedPtr<Derived> derived = MakeOwned<Derived>(5);
        OwnedPtr<Base> base = std::move(derived);
        EXPECT_EQ(base->Get(), 10);
        EXPECT_TRUE(derived == nullptr);
    }
    EXPECT_EQ(Base::destroyed, 1);
}

TEST(SafePointerTest, PtrObservesWithoutOwning)
{
    Base::destroyed = 0;
    OwnedPtr<Base> owned = MakeOwned<Base>(4);

    Ptr<Base> observer = owned.get();
    EXPECT_TRUE(static_cast<bool>(observer));
    EXPECT_EQ(observer->Get(), 4);
    EXPECT_EQ((*observer).value, 4);
    EXPECT_TRUE(observer != nullptr);

    Ptr<Base> copy = observer;
    EXPECT_TRUE(copy == observer);
    EXPECT_FALSE(copy != observer);

    Ptr<Base> empty;
    EXPECT_FALSE(static_cast<bool>(empty));
    EXPECT_TRUE(empty == nullptr);
    EXPECT_TRUE(empty != observer);

    // Observers never take ownership.
    EXPECT_EQ(Base::destroyed, 0);
}

TEST(SafePointerTest, PointersAreHashableAndOrdered)
{
    OwnedPtr<Base> a = MakeOwned<Base>(1);
    OwnedPtr<Base> b = MakeOwned<Base>(2);

    std::unordered_set<Ptr<Base>> observers;
    (void)observers.emplace(a.get());
    (void)observers.emplace(b.get());
    (void)observers.emplace(a.get());
    EXPECT_EQ(observers.size(), 2U);
    EXPECT_EQ(observers.count(a.get()), 1U);

    EXPECT_EQ(std::hash<Ptr<Base>>{}(a.get()), std::hash<Ptr<Base>>{}(a.get()));
    EXPECT_EQ(std::hash<OwnedPtr<Base>>{}(a), std::hash<Ptr<Base>>{}(a.get()));

    // Ordering falls back to the raw addresses, so exactly one direction must hold.
    EXPECT_NE(a < b, b < a);
    EXPECT_NE(a.get() < b.get(), b.get() < a.get());
    EXPECT_TRUE(a != b);
    EXPECT_FALSE(a == b);
}

TEST(SafePointerTest, StaticCastNarrowsToTheDerivedType)
{
    OwnedPtr<Base> owned = MakeOwned<Derived>(6);
    Ptr<Base> base = owned.get();
    Ptr<Derived> derived = base.StaticCast<Derived>();
    ASSERT_TRUE(static_cast<bool>(derived));
    EXPECT_EQ(derived->Get(), 12);
    EXPECT_EQ(derived->value, 6);
    EXPECT_EQ(base.get(), static_cast<Base*>(derived.get()));
}

TEST(SafePointerTest, StaticPointerCastAndSwapMoveOwnership)
{
    Base::destroyed = 0;
    {
        OwnedPtr<Base> base = MakeOwned<Derived>(8);
        OwnedPtr<Derived> derived = StaticPointerCast<Base, Derived>(std::move(base));
        EXPECT_TRUE(base == nullptr);
        ASSERT_TRUE(static_cast<bool>(derived));
        EXPECT_EQ(derived->Get(), 16);

        OwnedPtr<Derived> other = MakeOwned<Derived>(1);
        Swap(derived, other);
        EXPECT_EQ(derived->value, 1);
        EXPECT_EQ(other->value, 8);
        EXPECT_EQ(Base::destroyed, 0);
    }
    EXPECT_EQ(Base::destroyed, 2);
}
