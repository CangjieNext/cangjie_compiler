// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <cstring>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "cangjie/Utils/Unicode.h"

using namespace Cangjie;
using namespace Cangjie::Unicode;

namespace {
constexpr UTF32 BOM_NATIVE = 0x0000FEFF;
constexpr UTF32 BOM_SWAPPED = 0xFFFE0000;
constexpr UTF32 SURROGATE = 0xD800;
constexpr UTF32 ABOVE_MAX_CODEPOINT = 0x110000;

std::string ToUtf8(const std::vector<UTF32>& codepoints)
{
    std::string res;
    EXPECT_TRUE(ConvertUTF32ToUTF8String(ArrayRef<UTF32>{codepoints}, res));
    return res;
}

UTF32 SwapBytes(UTF32 v)
{
    return ((v & 0x000000FFU) << 24) | ((v & 0x0000FF00U) << 8) | ((v & 0x00FF0000U) >> 8) |
        ((v & 0xFF000000U) >> 24);
}
} // namespace

TEST(UnicodeConversionTest, Utf32ToUtf8RejectsMisalignedByteCount)
{
    // Not a whole number of UTF32 code units.
    const char bytes[5] = {'a', 0, 0, 0, 'b'};
    std::string res = "leftover";
    EXPECT_FALSE(ConvertUTF32ToUTF8String(ArrayRef<char>{bytes, 5}, res));
}

TEST(UnicodeConversionTest, Utf32ToUtf8AcceptsEmptyInput)
{
    std::string res = "leftover";
    EXPECT_TRUE(ConvertUTF32ToUTF8String(ArrayRef<char>{nullptr, 0}, res));
    // The empty case returns before touching the output buffer.
    EXPECT_EQ(res, "leftover");
}

TEST(UnicodeConversionTest, Utf32ToUtf8ConvertsPlainCodepoints)
{
    EXPECT_EQ(ToUtf8({'a', 'b', 'c'}), "abc");
    // U+4F60 U+597D -> "你好"
    EXPECT_EQ(ToUtf8({0x4F60, 0x597D}), "\xE4\xBD\xA0\xE5\xA5\xBD");
    // Beyond the BMP.
    EXPECT_EQ(ToUtf8({0x1F600}), "\xF0\x9F\x98\x80");
}

TEST(UnicodeConversionTest, Utf32ToUtf8SkipsTheNativeByteOrderMark)
{
    const std::vector<UTF32> withBom{BOM_NATIVE, 'h', 'i'};
    EXPECT_EQ(ToUtf8(withBom), "hi");
}

TEST(UnicodeConversionTest, Utf32ToUtf8ByteSwapsWhenTheMarkIsReversed)
{
    // A byte-swapped BOM means the whole buffer is in the opposite endianness.
    std::vector<UTF32> swapped{BOM_SWAPPED};
    for (UTF32 c : {static_cast<UTF32>('h'), static_cast<UTF32>('i')}) {
        swapped.push_back(SwapBytes(c));
    }
    std::string res;
    ASSERT_TRUE(ConvertUTF32ToUTF8String(ArrayRef<UTF32>{swapped}, res));
    EXPECT_EQ(res, "hi");
}

TEST(UnicodeConversionTest, Utf32ToUtf8RejectsIllegalCodepoints)
{
    std::string res = "leftover";
    const std::vector<UTF32> surrogate{SURROGATE};
    EXPECT_FALSE(ConvertUTF32ToUTF8String(ArrayRef<UTF32>{surrogate}, res));
    // On failure the output is cleared so a caller cannot read a half-converted string.
    EXPECT_TRUE(res.empty());

    const std::vector<UTF32> tooLarge{ABOVE_MAX_CODEPOINT};
    EXPECT_FALSE(ConvertUTF32ToUTF8String(ArrayRef<UTF32>{tooLarge}, res));
    EXPECT_TRUE(res.empty());
}

TEST(UnicodeConversionTest, ConvertCodepointToUtf8WritesTheSequence)
{
    char buffer[8] = {0};
    char* cursor = buffer;
    ASSERT_TRUE(ConvertCodepointToUTF8(0x4F60, cursor));
    EXPECT_EQ(cursor - buffer, 3);
    EXPECT_EQ(std::string(buffer, 3), "\xE4\xBD\xA0");

    cursor = buffer;
    ASSERT_TRUE(ConvertCodepointToUTF8('A', cursor));
    EXPECT_EQ(cursor - buffer, 1);
    EXPECT_EQ(buffer[0], 'A');

    // A lone surrogate is not a legal scalar value.
    cursor = buffer;
    EXPECT_FALSE(ConvertCodepointToUTF8(SURROGATE, cursor));
    EXPECT_EQ(cursor, buffer);
}

TEST(UnicodeConversionTest, Utf8ToUtf32ReportsIllegalAndTruncatedInput)
{
    const auto convert = [](const std::string& s) {
        auto start = reinterpret_cast<const UTF8*>(s.data());
        const UTF8* end = start + s.size();
        UTF32 out[8] = {0};
        UTF32* target = out;
        return ConvertUTF8toUTF32(&start, end, &target, out + 8);
    };

    EXPECT_EQ(convert("ok"), ConversionResult::OK);
    // Leading byte announces three bytes but only two are present.
    EXPECT_EQ(convert("\xE4\xBD"), ConversionResult::SOURCE_EXHAUSTED);
    // Enough bytes are present, but 0x28 is not a continuation byte.
    EXPECT_EQ(convert("\xE4\x28\xA0"), ConversionResult::SOURCE_ILLEGAL);
}

TEST(UnicodeConversionTest, StringRefToUtf32CopiesTheTailOfIllegalInput)
{
    // Well-formed input converts one-to-one.
    auto ok = StringRef{"abc"}.ToUTF32();
    ASSERT_EQ(ok.size(), 3U);
    EXPECT_EQ(ok[0], static_cast<UTF32>('a'));

    // Multi-byte input yields fewer code points than bytes.
    const std::string cjk = "\xE4\xBD\xA0\xE5\xA5\xBD";
    auto wide = StringRef{cjk}.ToUTF32();
    ASSERT_EQ(wide.size(), 2U);
    EXPECT_EQ(wide[0], 0x4F60U);

    // On illegal input the untranslatable tail is copied over byte by byte so that no
    // character is silently dropped.
    const std::string broken = std::string("ab") + '\xFF' + 'c';
    auto recovered = StringRef{broken}.ToUTF32();
    ASSERT_EQ(recovered.size(), 4U);
    EXPECT_EQ(recovered[0], static_cast<UTF32>('a'));
    EXPECT_EQ(recovered[1], static_cast<UTF32>('b'));
    EXPECT_EQ(recovered[2], 0xFFU);
    EXPECT_EQ(recovered[3], static_cast<UTF32>('c'));
}

TEST(UnicodeConversionTest, IsNFCCoversQuickCheckYesNoAndMaybe)
{
    // Pure ASCII: quick check answers YES immediately, no recomposition needed.
    EXPECT_EQ(NfcQuickCheck("plain ascii"), NfcQcResult::YES);
    EXPECT_TRUE(IsNFC("plain ascii"));
    // The composed form U+00C1 is in NFC too.
    EXPECT_TRUE(IsNFC("\xC3\x81"));

    // U+0301 (ccc 230) followed by U+0316 (ccc 220) is out of canonical order, so the quick
    // check can rule it out without recomposing.
    const std::string outOfOrder = "\xCC\x81\xCC\x96";
    EXPECT_EQ(NfcQuickCheck(outOfOrder), NfcQcResult::NO);
    EXPECT_FALSE(IsNFC(outOfOrder));

    // U+0041 U+0301 (A + combining acute) quick-checks MAYBE; recomposing shows it is NFD.
    const std::string decomposed = "A\xCC\x81";
    EXPECT_EQ(NfcQuickCheck(decomposed), NfcQcResult::MAYBE);
    EXPECT_FALSE(IsNFC(decomposed));

    // A lone combining mark also quick-checks MAYBE, but recomposing leaves it unchanged.
    const std::string loneMark = "\xCC\x81";
    EXPECT_EQ(NfcQuickCheck(loneMark), NfcQcResult::MAYBE);
    EXPECT_TRUE(IsNFC(loneMark));
}

TEST(UnicodeConversionTest, NFCNormalisesOnlyWhenNeeded)
{
    std::string alreadyNfc = "plain";
    NFC(alreadyNfc);
    EXPECT_EQ(alreadyNfc, "plain");

    std::string decomposed = "A\xCC\x81";
    NFC(decomposed);
    EXPECT_EQ(decomposed, "\xC3\x81");
}
