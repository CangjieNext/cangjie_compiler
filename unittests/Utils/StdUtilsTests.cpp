// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <climits>
#include <cmath>
#include <limits>
#include <string>

#include "gtest/gtest.h"

#include "cangjie/Utils/StdUtils.h"

using namespace Cangjie;

namespace {
constexpr int BIN_BASE = 2;
constexpr int OCT_BASE = 8;
constexpr int HEX_BASE = 16;
// Wider than every integral type the Sto* helpers return, so std::sto* throws std::out_of_range.
const std::string OUT_OF_RANGE_DIGITS = "999999999999999999999999999999999999999";
} // namespace

TEST(StdUtilsTest, StoiAcceptsDecimalAndBaseVariants)
{
    EXPECT_EQ(Stoi("42").value(), 42);
    EXPECT_EQ(Stoi("-42").value(), -42);
    EXPECT_EQ(Stoi("  17").value(), 17);
    EXPECT_EQ(Stoi("ff", HEX_BASE).value(), 255);
    EXPECT_EQ(Stoi("777", OCT_BASE).value(), 511);
    EXPECT_EQ(Stoi("101", BIN_BASE).value(), 5);
    // std::stoi stops at the first character that is not part of the number.
    EXPECT_EQ(Stoi("12abc").value(), 12);
}

TEST(StdUtilsTest, StoiRejectsInvalidAndOutOfRange)
{
    EXPECT_FALSE(Stoi("").has_value());
    EXPECT_FALSE(Stoi("abc").has_value());
    EXPECT_FALSE(Stoi(OUT_OF_RANGE_DIGITS).has_value());
}

TEST(StdUtilsTest, StouiRejectsValuesAboveUintMax)
{
    EXPECT_EQ(Stoui("42").value(), 42U);
    EXPECT_EQ(Stoui("ff", HEX_BASE).value(), 255U);
    EXPECT_EQ(Stoui(std::to_string(UINT_MAX)).value(), UINT_MAX);
    EXPECT_FALSE(Stoui("abc").has_value());
    EXPECT_FALSE(Stoui(OUT_OF_RANGE_DIGITS).has_value());

    // The `result > UINT_MAX` guard only has anything to reject where `unsigned long` is wider
    // than `unsigned int` (LP64). On LLP64 both are 32 bits, std::stoul itself throws
    // out_of_range and the value never reaches the comparison.
    if constexpr (sizeof(unsigned long) > sizeof(unsigned int)) {
        // Fits in `unsigned long` but overflows `unsigned int`, so Stoui must reject it instead
        // of silently truncating.
        EXPECT_FALSE(Stoui(std::to_string(static_cast<unsigned long>(UINT_MAX) + 1UL)).has_value());
        // std::stoul wraps a negative literal around to ULONG_MAX, which is also above UINT_MAX.
        EXPECT_FALSE(Stoui("-1").has_value());
    } else {
        // ULONG_MAX == UINT_MAX here, so a wrapped "-1" is a legal unsigned int.
        EXPECT_EQ(Stoui("-1").value(), UINT_MAX);
    }
}

TEST(StdUtilsTest, StolAcceptsValidAndRejectsInvalid)
{
    EXPECT_EQ(Stol("1234567890").value(), 1234567890L);
    EXPECT_EQ(Stol("-1234567890").value(), -1234567890L);
    EXPECT_EQ(Stol("ff", HEX_BASE).value(), 255L);
    EXPECT_FALSE(Stol("").has_value());
    EXPECT_FALSE(Stol("cangjie").has_value());
    EXPECT_FALSE(Stol(OUT_OF_RANGE_DIGITS).has_value());
}

TEST(StdUtilsTest, StoulAcceptsValidAndRejectsInvalid)
{
    EXPECT_EQ(Stoul("1234567890").value(), 1234567890UL);
    EXPECT_EQ(Stoul("ff", HEX_BASE).value(), 255UL);
    EXPECT_FALSE(Stoul("").has_value());
    EXPECT_FALSE(Stoul("cangjie").has_value());
    EXPECT_FALSE(Stoul(OUT_OF_RANGE_DIGITS).has_value());
}

TEST(StdUtilsTest, StollAcceptsValidAndRejectsInvalid)
{
    EXPECT_EQ(Stoll(std::to_string(std::numeric_limits<long long>::max())).value(),
        std::numeric_limits<long long>::max());
    EXPECT_EQ(Stoll("-9007199254740993").value(), -9007199254740993LL);
    EXPECT_EQ(Stoll("ff", HEX_BASE).value(), 255LL);
    EXPECT_FALSE(Stoll("").has_value());
    EXPECT_FALSE(Stoll("cangjie").has_value());
    EXPECT_FALSE(Stoll(OUT_OF_RANGE_DIGITS).has_value());
}

TEST(StdUtilsTest, StoullAcceptsValidAndRejectsInvalid)
{
    EXPECT_EQ(Stoull(std::to_string(std::numeric_limits<unsigned long long>::max())).value(),
        std::numeric_limits<unsigned long long>::max());
    EXPECT_EQ(Stoull("ff", HEX_BASE).value(), 255ULL);
    EXPECT_FALSE(Stoull("").has_value());
    EXPECT_FALSE(Stoull("cangjie").has_value());
    EXPECT_FALSE(Stoull(OUT_OF_RANGE_DIGITS).has_value());
}

TEST(StdUtilsTest, StodAcceptsValidAndRejectsInvalid)
{
    EXPECT_DOUBLE_EQ(Stod("3.5").value(), 3.5);
    EXPECT_DOUBLE_EQ(Stod("-0.25").value(), -0.25);
    EXPECT_DOUBLE_EQ(Stod("1e3").value(), 1000.0);
    EXPECT_FALSE(Stod("").has_value());
    EXPECT_FALSE(Stod("cangjie").has_value());
    // Exponent far beyond the double range makes std::stod throw std::out_of_range.
    EXPECT_FALSE(Stod("1e999999").has_value());
}

TEST(StdUtilsTest, StoldAcceptsValidAndRejectsInvalid)
{
    EXPECT_TRUE(Stold("2.25").has_value());
    EXPECT_NEAR(static_cast<double>(Stold("2.25").value()), 2.25, 1e-9);
    EXPECT_FALSE(Stold("").has_value());
    EXPECT_FALSE(Stold("cangjie").has_value());
    EXPECT_FALSE(Stold("1e999999").has_value());
}
