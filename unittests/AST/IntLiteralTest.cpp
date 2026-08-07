// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * Unit tests for IntLiteral. Covers every public API: constructors, value
 * accessors, sign handling, string parsing (InitIntLiteral), overflow detection,
 * wrapping/saturating value computation, bit-length comparison, and the
 * SetOutOfRange type-driven overflow entry point.
 *
 * The private wrapping/saturating caches have no getters, so their correctness
 * is observed indirectly through SetWrappingValue / SetSaturatingValue, which
 * copy the cached values back into the value fields.
 *
 * IntLiteral has two value ctors -- IntLiteral(uint64_t,...) and
 * IntLiteral(int64_t,...) -- that are ambiguous when called with plain integer
 * literals. The first argument is therefore always passed as a typed local so
 * overload resolution picks the intended ctor.
 */

#include <cstdint>
#include <limits>
#include <string>

#include <gtest/gtest.h>

#include "cangjie/AST/IntLiteral.h"
#include "cangjie/AST/Types.h"
#include "cangjie/Utils/SafePointer.h"

using namespace Cangjie;
using namespace AST;

namespace {
// INT64_MIN expressed as a positive magnitude in uint64 space (2^63).
constexpr uint64_t ABS_INT64_MIN = static_cast<uint64_t>(std::numeric_limits<int64_t>::min());
} // namespace

// ---------------------------------------------------------------------------
// Constructors & default state
// ---------------------------------------------------------------------------

TEST(IntLiteralTest, DefaultConstructorYieldsZeroIdealInt)
{
    IntLiteral lit;
    EXPECT_EQ(lit.Sign(), 1);
    EXPECT_EQ(lit.Int64(), 0);
    EXPECT_EQ(lit.Uint64(), 0);
    EXPECT_FALSE(lit.IsOutOfRange());
    EXPECT_FALSE(lit.IsNegativeNum());
    EXPECT_EQ(lit.GetValue(), "0");
}

TEST(IntLiteralTest, Uint64ConstructorRecordsValueAndFlags)
{
    // In-range UInt64 literal.
    uint64_t v = 42ULL;
    IntLiteral lit(v, TypeKind::TYPE_UINT64, false);
    EXPECT_EQ(lit.Uint64(), 42ULL);
    EXPECT_EQ(lit.Int64(), 42);
    EXPECT_EQ(lit.Sign(), 1);
    EXPECT_FALSE(lit.IsOutOfRange());
    EXPECT_EQ(lit.GetValue(), "42");

    // Overflow flag is stored verbatim; the uint64 ctor does not derive sign.
    uint64_t ov = 300ULL;
    IntLiteral overflowLit(ov, TypeKind::TYPE_UINT8, true);
    EXPECT_TRUE(overflowLit.IsOutOfRange());
}

TEST(IntLiteralTest, Int64ConstructorDerivesSignFromValue)
{
    int64_t pos = 100;
    IntLiteral posLit(pos, TypeKind::TYPE_INT64, false);
    EXPECT_EQ(posLit.Int64(), 100);
    EXPECT_EQ(posLit.Sign(), 1);
    EXPECT_FALSE(posLit.IsNegativeNum());

    int64_t neg = -100;
    IntLiteral negLit(neg, TypeKind::TYPE_INT64, false);
    EXPECT_EQ(negLit.Int64(), -100);
    EXPECT_EQ(negLit.Sign(), -1);
    EXPECT_TRUE(negLit.IsNegativeNum());
}

TEST(IntLiteralTest, Int64ConstructorAtInt64MinKeepsNegativeSign)
{
    int64_t minVal = std::numeric_limits<int64_t>::min();
    IntLiteral lit(minVal, TypeKind::TYPE_INT64, false);
    EXPECT_EQ(lit.Int64(), std::numeric_limits<int64_t>::min());
    EXPECT_EQ(lit.Sign(), -1);
    EXPECT_TRUE(lit.IsNegativeNum());
}

// ---------------------------------------------------------------------------
// Assign & value accessors
// ---------------------------------------------------------------------------

TEST(IntLiteralTest, AssignCopiesStateAcrossAccessors)
{
    int64_t seven = 7;
    IntLiteral src(seven, TypeKind::TYPE_INT64, false);
    int64_t big = 999;
    IntLiteral dst(big, TypeKind::TYPE_INT64, false);
    dst.Assign(src);
    EXPECT_EQ(dst.Int64(), 7);
    EXPECT_EQ(dst.Uint64(), 7ULL);
    EXPECT_EQ(dst.Sign(), 1);
    EXPECT_FALSE(dst.IsOutOfRange());

    // Assign copies outOfRange/type/sign/values from an overflow source.
    uint64_t ov = 300ULL;
    IntLiteral overflowSrc(ov, TypeKind::TYPE_UINT8, true, true);
    IntLiteral overflowDst;
    overflowDst.Assign(overflowSrc);
    EXPECT_TRUE(overflowDst.IsOutOfRange());
}

TEST(IntLiteralTest, SignAccessorsRoundTrip)
{
    int64_t v = 5;
    IntLiteral lit(v, TypeKind::TYPE_INT64, false);
    lit.SetSign(-1);
    EXPECT_EQ(lit.Sign(), -1);
    EXPECT_TRUE(lit.IsNegativeNum());
    lit.SetSign(1);
    EXPECT_EQ(lit.Sign(), 1);
    EXPECT_FALSE(lit.IsNegativeNum());
}

TEST(IntLiteralTest, SetInt64KeepsIntAndUintInSync)
{
    IntLiteral lit;
    lit.SetInt64(-1);
    EXPECT_EQ(lit.Int64(), -1);
    EXPECT_EQ(lit.Uint64(), std::numeric_limits<uint64_t>::max());
    // negative value set via SetInt64 must not imply IsNegativeNum on its own;
    // IsNegativeNum consults sign, which defaults to +1.
    EXPECT_FALSE(lit.IsNegativeNum());
}

TEST(IntLiteralTest, SetUint64KeepsUintAndIntInSync)
{
    IntLiteral lit;
    lit.SetUint64(std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(lit.Uint64(), std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(lit.Int64(), static_cast<int64_t>(std::numeric_limits<uint64_t>::max()));
    EXPECT_FALSE(lit.IsNegativeNum());
}

TEST(IntLiteralTest, IsNegativeNumIsFalseForZeroEvenWithNegativeSign)
{
    // sign == -1 but value == 0 must not be treated as a negative number.
    int64_t v = 0;
    IntLiteral lit(v, TypeKind::TYPE_INT64, false);
    lit.SetSign(-1);
    EXPECT_FALSE(lit.IsNegativeNum());
}

// ---------------------------------------------------------------------------
// InitIntLiteral / string constructor: bases, sign, underscores, byte literal
// ---------------------------------------------------------------------------

TEST(IntLiteralTest, StringConstructorParsesDecimal)
{
    IntLiteral lit(std::string("123"), TypeKind::TYPE_IDEAL_INT);
    EXPECT_EQ(lit.Int64(), 123);
    EXPECT_EQ(lit.Uint64(), 123ULL);
    EXPECT_EQ(lit.Sign(), 1);
    EXPECT_FALSE(lit.IsOutOfRange());
    EXPECT_EQ(lit.GetValue(), "123");
}

TEST(IntLiteralTest, StringConstructorParsesNegativeDecimal)
{
    IntLiteral lit(std::string("-42"), TypeKind::TYPE_IDEAL_INT);
    EXPECT_EQ(lit.Int64(), -42);
    EXPECT_EQ(lit.Sign(), -1);
    EXPECT_TRUE(lit.IsNegativeNum());
    EXPECT_FALSE(lit.IsOutOfRange());
    EXPECT_EQ(lit.GetValue(), "-42");
}

TEST(IntLiteralTest, StringConstructorParsesHexPrefix)
{
    IntLiteral lit(std::string("0xff"), TypeKind::TYPE_IDEAL_INT);
    EXPECT_EQ(lit.Uint64(), 0xffULL);
    EXPECT_EQ(lit.Int64(), 0xff);
    EXPECT_FALSE(lit.IsOutOfRange());
}

TEST(IntLiteralTest, StringConstructorParsesBinaryPrefix)
{
    IntLiteral lit(std::string("0b1010"), TypeKind::TYPE_IDEAL_INT);
    EXPECT_EQ(lit.Uint64(), 10ULL);
}

TEST(IntLiteralTest, StringConstructorParsesOctalPrefix)
{
    IntLiteral lit(std::string("0o17"), TypeKind::TYPE_IDEAL_INT);
    EXPECT_EQ(lit.Uint64(), 15ULL);
}

TEST(IntLiteralTest, StringConstructorStripsUnderscores)
{
    IntLiteral lit(std::string("1_2_3"), TypeKind::TYPE_IDEAL_INT);
    EXPECT_EQ(lit.Int64(), 123);
}

TEST(IntLiteralTest, StringConstructorEmptyStringLeavesZero)
{
    IntLiteral lit(std::string(""), TypeKind::TYPE_IDEAL_INT);
    EXPECT_EQ(lit.Int64(), 0);
    EXPECT_EQ(lit.Uint64(), 0ULL);
    EXPECT_FALSE(lit.IsOutOfRange());
}

TEST(IntLiteralTest, StringConstructorByteLiteralAsciiChar)
{
    IntLiteral lit(std::string("b'Y'"), TypeKind::TYPE_IDEAL_INT);
    EXPECT_EQ(lit.Int64(), static_cast<int64_t>('Y'));
}

TEST(IntLiteralTest, StringConstructorByteLiteralEscapeSequence)
{
    IntLiteral lit(std::string("b'\\n'"), TypeKind::TYPE_IDEAL_INT);
    EXPECT_EQ(lit.Int64(), static_cast<int64_t>('\n'));
}

TEST(IntLiteralTest, StringConstructorByteLiteralUnicodeEscape)
{
    IntLiteral lit(std::string("b'\\u{ff}'"), TypeKind::TYPE_IDEAL_INT);
    EXPECT_EQ(lit.Int64(), 0xff);
}

// ---------------------------------------------------------------------------
// GetValue: signed vs unsigned rendering
// ---------------------------------------------------------------------------

TEST(IntLiteralTest, GetValueRendersUnsignedForUnsignedKinds)
{
    // A value whose int64 reinterpret would be negative must still be printed
    // as the unsigned magnitude when the type is unsigned.
    uint64_t max = std::numeric_limits<uint64_t>::max();
    IntLiteral lit(max, TypeKind::TYPE_UINT64, false);
    EXPECT_EQ(lit.GetValue(), std::to_string(std::numeric_limits<uint64_t>::max()));
}

TEST(IntLiteralTest, GetValueRendersSignedForSignedKinds)
{
    int64_t neg = -1;
    IntLiteral lit(neg, TypeKind::TYPE_INT64, false);
    EXPECT_EQ(lit.GetValue(), "-1");
}

// ---------------------------------------------------------------------------
// GreaterThanOrEqualBitLen
// ---------------------------------------------------------------------------

TEST(IntLiteralTest, GreaterThanOrEqualBitLenComparesValueToTypeBitLen)
{
    // Despite the name, GreaterThanOrEqualBitLen compares uint64Val against the
    // type's bit-length entry (8/16/32/64), not the bit width the value would
    // need. A value of 20 is >= 8 (INT8/UINT8) and >= 16 (INT16/UINT16), but
    // < 32 (INT32/UINT32) and < 64 (INT64/UINT64).
    int64_t v = 20;
    IntLiteral lit(v, TypeKind::TYPE_INT64, false);
    EXPECT_TRUE(lit.GreaterThanOrEqualBitLen(TypeKind::TYPE_INT8));
    EXPECT_TRUE(lit.GreaterThanOrEqualBitLen(TypeKind::TYPE_UINT8));
    EXPECT_TRUE(lit.GreaterThanOrEqualBitLen(TypeKind::TYPE_INT16));
    EXPECT_TRUE(lit.GreaterThanOrEqualBitLen(TypeKind::TYPE_UINT16));
    EXPECT_FALSE(lit.GreaterThanOrEqualBitLen(TypeKind::TYPE_INT32));
    EXPECT_FALSE(lit.GreaterThanOrEqualBitLen(TypeKind::TYPE_UINT32));
    EXPECT_FALSE(lit.GreaterThanOrEqualBitLen(TypeKind::TYPE_INT64));
    EXPECT_FALSE(lit.GreaterThanOrEqualBitLen(TypeKind::TYPE_UINT64));
}

TEST(IntLiteralTest, GreaterThanOrEqualBitLenBoundaryIsInclusive)
{
    // uint64Val == the type's bit length is still "greater than or equal".
    int64_t v = 16;
    IntLiteral lit(v, TypeKind::TYPE_INT64, false);
    EXPECT_TRUE(lit.GreaterThanOrEqualBitLen(TypeKind::TYPE_INT16));   // 16 >= 16
    EXPECT_FALSE(lit.GreaterThanOrEqualBitLen(TypeKind::TYPE_INT32));  // 16 < 32
}

TEST(IntLiteralTest, GreaterThanOrEqualBitLenReturnsFalseForNonIntegerKind)
{
    int64_t v = 0;
    IntLiteral lit(v, TypeKind::TYPE_IDEAL_INT, false);
    // Any non-integer TypeKind is absent from the bit-length table.
    EXPECT_FALSE(lit.GreaterThanOrEqualBitLen(TypeKind::TYPE_ANY));
}

// ---------------------------------------------------------------------------
// Wrapping & saturating value computation
// ---------------------------------------------------------------------------

TEST(IntLiteralTest, CalcWrappingAndSaturatingValNoOpWhenInRange)
{
    int64_t v = 5;
    IntLiteral lit(v, TypeKind::TYPE_INT8, false);
    lit.CalcWrappingAndSaturatingVal();
    // outOfRange is false -> SetWrappingValue/SetSaturatingValue are no-ops;
    // the value must stay unchanged.
    lit.SetWrappingValue();
    lit.SetSaturatingValue();
    EXPECT_EQ(lit.Int64(), 5);
}

TEST(IntLiteralTest, WrappingTruncatesUnsignedOverflowToWidth)
{
    // 300 wrapped to UInt8 (8 bits) is 300 - 256 = 44.
    uint64_t v = 300ULL;
    IntLiteral lit(v, TypeKind::TYPE_UINT8, true, true);
    lit.SetWrappingValue();
    EXPECT_EQ(lit.Uint64(), 44ULL);
}

TEST(IntLiteralTest, SaturatingUnsignedMaxOverflowClampsToMax)
{
    uint64_t v = 300ULL;
    IntLiteral lit(v, TypeKind::TYPE_UINT8, true, true);
    lit.SetSaturatingValue();
    EXPECT_EQ(lit.Uint64(), std::numeric_limits<uint8_t>::max());
}

TEST(IntLiteralTest, SaturatingUnsignedMinOverflowClampsToZero)
{
    // outOfMax=false means minimum-side overflow -> clamp to the type's min (0).
    uint64_t v = 300ULL;
    IntLiteral lit(v, TypeKind::TYPE_UINT8, true, false);
    lit.SetSaturatingValue();
    EXPECT_EQ(lit.Uint64(), 0ULL);
}

TEST(IntLiteralTest, WrappingTruncatesSignedOverflowToWidth)
{
    // 200 wrapped to Int8 (8 bits, signed) is 200 - 256 = -56.
    int64_t v = 200;
    IntLiteral lit(v, TypeKind::TYPE_INT8, true, true);
    lit.SetWrappingValue();
    EXPECT_EQ(lit.Int64(), -56);
}

TEST(IntLiteralTest, SaturatingSignedMaxOverflowClampsToMax)
{
    int64_t v = 200;
    IntLiteral lit(v, TypeKind::TYPE_INT8, true, true);
    lit.SetSaturatingValue();
    EXPECT_EQ(lit.Int64(), std::numeric_limits<int8_t>::max());
}

TEST(IntLiteralTest, SaturatingSignedMinOverflowClampsToMin)
{
    // outOfMax=false -> minimum-side overflow -> clamp to INT8 min (-128).
    int64_t v = -300;
    IntLiteral lit(v, TypeKind::TYPE_INT8, true, false);
    lit.SetSaturatingValue();
    EXPECT_EQ(lit.Int64(), std::numeric_limits<int8_t>::min());
}

// ---------------------------------------------------------------------------
// SetOutOfRange: type-driven overflow detection with native normalization
// ---------------------------------------------------------------------------

TEST(IntLiteralTest, SetOutOfRangeDetectsUnsignedOverflow)
{
    uint64_t v = 300ULL;
    IntLiteral lit(v, TypeKind::TYPE_IDEAL_INT, false);
    auto ty = MakeOwned<PrimitiveTy>(TypeKind::TYPE_UINT8);
    lit.SetOutOfRange(ty.get());
    EXPECT_TRUE(lit.IsOutOfRange());
}

TEST(IntLiteralTest, SetOutOfRangeDetectsSignedNegativeOverflow)
{
    int64_t v = -300;
    IntLiteral lit(v, TypeKind::TYPE_IDEAL_INT, false);
    auto ty = MakeOwned<PrimitiveTy>(TypeKind::TYPE_INT8);
    lit.SetOutOfRange(ty.get());
    EXPECT_TRUE(lit.IsOutOfRange());
}

TEST(IntLiteralTest, SetOutOfRangeNormalizesIntNativeToInt64)
{
    int64_t v = std::numeric_limits<int64_t>::max();
    IntLiteral lit(v, TypeKind::TYPE_IDEAL_INT, false);
    auto ty = MakeOwned<PrimitiveTy>(TypeKind::TYPE_INT_NATIVE);
    ty->bitness = 64; // 64-bit native int maps to INT64
    lit.SetOutOfRange(ty.get());
    // Max int64 against INT64 is exactly the max -> not overflow.
    EXPECT_FALSE(lit.IsOutOfRange());

    // Same value against 32-bit native int overflows.
    ty->bitness = 32; // 32-bit native int maps to INT32
    lit.SetOutOfRange(ty.get());
    EXPECT_TRUE(lit.IsOutOfRange());
}

TEST(IntLiteralTest, SetOutOfRangeNormalizesUintNativeToUint64)
{
    uint64_t v = std::numeric_limits<uint64_t>::max();
    IntLiteral lit(v, TypeKind::TYPE_IDEAL_INT, false);
    auto ty = MakeOwned<PrimitiveTy>(TypeKind::TYPE_UINT_NATIVE);
    ty->bitness = 64; // 64-bit native uint maps to UINT64
    lit.SetOutOfRange(ty.get());
    EXPECT_FALSE(lit.IsOutOfRange()); // max uint64 fits UINT64 exactly
}

// ---------------------------------------------------------------------------
// Edge: int64 min magnitude as unsigned
// ---------------------------------------------------------------------------

TEST(IntLiteralTest, Int64MinWrapsCorrectlyAsUnsigned)
{
    // INT64_MIN as uint64 is 2^63; the int64 ctor stores it verbatim.
    int64_t minVal = std::numeric_limits<int64_t>::min();
    IntLiteral lit(minVal, TypeKind::TYPE_INT64, false);
    EXPECT_EQ(lit.Uint64(), ABS_INT64_MIN);
    EXPECT_EQ(lit.Sign(), -1);
    EXPECT_TRUE(lit.IsNegativeNum());
}
