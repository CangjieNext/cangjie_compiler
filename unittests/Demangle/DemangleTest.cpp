// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "gtest/gtest.h"

#define private public

#include "DeCompression.h"
#include "Demangler.h"
#include "StdString.h"

#include <string>
#include <utility>
#include <vector>

using namespace Cangjie;

namespace {
// Keep in sync with PRIMITIVE_PREFIX_SET in demangler/DeCompression.cpp.
// 'D' is excluded here because it is only a primitive when followed by 'h'.
constexpr const char* PRIMITIVE_PREFIXES = "nucbfdasilqhtjmrDv";
} // namespace

TEST(DemangleTest, PackageNameColonDelimiter)
{
    Demangler<StdString> demangler_1("4abc:");
    auto result_1 = demangler_1.DemanglePackageName();
    EXPECT_STREQ("abc:", result_1.pkgName.Str());

    Demangler<StdString> demangler_2("5abc:x");
    auto result_2 = demangler_2.DemanglePackageName();
    EXPECT_STREQ("abc::x", result_2.pkgName.Str());

    Demangler<StdString> demangler_3("7abc:xyz");
    auto result_3 = demangler_3.DemanglePackageName();
    EXPECT_STREQ("abc::xyz", result_3.pkgName.Str());
}

TEST(DemangleTest, IsSamePrefixBoundary)
{
    DeCompression<StdString> demangler("");
    StdString gv = "GV";
    StdString gp = "GP";
    StdString gx = "GX";
    StdString g = "G";
    StdString agv = "AGV";
    StdString xgv = "XGV";
    StdString xgp = "XGP";
    StdString empty = "";

    EXPECT_TRUE(demangler.IsSamePrefix(gv, gv, 0));
    EXPECT_FALSE(demangler.IsSamePrefix(gv, gp, 0));
    EXPECT_FALSE(demangler.IsSamePrefix(gp, gv, 0));
    EXPECT_FALSE(demangler.IsSamePrefix(gv, gx, 0));
    // Both inputs must hold at least PREFIX_LEN chars from idx: shorter
    // inputs compare as false instead of matching the truncated prefix.
    EXPECT_FALSE(demangler.IsSamePrefix(g, gv, 0));
    EXPECT_FALSE(demangler.IsSamePrefix(gv, g, 0));
    EXPECT_FALSE(demangler.IsSamePrefix(g, g, 0));
    EXPECT_FALSE(demangler.IsSamePrefix(empty, gv, 0));
    EXPECT_FALSE(demangler.IsSamePrefix(gv, empty, 0));
    // idx must leave room for the whole prefix in both inputs.
    EXPECT_FALSE(demangler.IsSamePrefix(agv, gv, 2));
    EXPECT_FALSE(demangler.IsSamePrefix(agv, gv, 3));
    // Prefix comparison honours a non-zero offset.
    EXPECT_TRUE(demangler.IsSamePrefix(agv, xgv, 1));
    EXPECT_FALSE(demangler.IsSamePrefix(agv, xgp, 1));
}

TEST(DemangleTest, ForwardTypePrimitivePrefixPopsPlaceholder)
{
    // Primitive encodes record no span: ForwardType pops the placeholder it
    // pushed and advances exactly one char (two for 'Dh').
    for (const char* p = PRIMITIVE_PREFIXES; *p != '\0'; ++p) {
        if (*p == 'D') {
            // Covered by the dedicated 'Dh'/'D<other>' cases below.
            continue;
        }
        char inputBuf[3] = {*p, 'E', '\0'};
        StdString input(inputBuf);
        DeCompression<StdString> demangler(input);
        demangler.isRecord = true;
        StdString mangled = input;
        size_t cnt = 0;
        size_t nextIdx = demangler.ForwardType(mangled, cnt, 0);
        EXPECT_EQ(nextIdx, 1u) << "primitive prefix '" << *p << "'";
        EXPECT_EQ(cnt, 0u) << "primitive prefix '" << *p << "'";
        EXPECT_TRUE(demangler.treeIdMap.empty()) << "primitive prefix '" << *p << "'";
        EXPECT_STREQ(mangled.Str(), input.Str()) << "primitive prefix '" << *p << "'";
    }
    // 'Dh' is the two-char primitive encode.
    StdString dhInput = "DhE";
    DeCompression<StdString> dhDemangler(dhInput);
    dhDemangler.isRecord = true;
    StdString dhMangled = dhInput;
    size_t dhCnt = 0;
    EXPECT_EQ(dhDemangler.ForwardType(dhMangled, dhCnt, 0), 2u);
    EXPECT_EQ(dhCnt, 0u);
    EXPECT_TRUE(dhDemangler.treeIdMap.empty());
    // 'D' followed by a non-'h' char is not a primitive encode: the
    // placeholder is still popped and no progress is made.
    StdString dxInput = "DxE";
    DeCompression<StdString> dxDemangler(dxInput);
    dxDemangler.isRecord = true;
    StdString dxMangled = dxInput;
    size_t dxCnt = 0;
    EXPECT_EQ(dxDemangler.ForwardType(dxMangled, dxCnt, 0), 0u);
    EXPECT_EQ(dxCnt, 0u);
    EXPECT_TRUE(dxDemangler.treeIdMap.empty());
}

TEST(DemangleTest, ForwardTypeCompoundPrefixRecordsSpan)
{
    // Compound encodes dispatch through ForwardCompoundType and record a span
    // covering the whole encode. Class encodes additionally record their
    // package and name spans.
    const std::vector<std::pair<const char*, size_t>> compoundCases = {
        {"CN4main3FooE", 3},
        {"RN4main3FooE", 3},
        {"NN4main3FooE", 3},
        {"F0iE", 1},
        {"T0_iE", 1},
        {"Pi", 1},
        {"A0_i", 1},
        {"V0_i", 1},
        {"G4test_", 1},
    };
    for (const auto& compoundCase : compoundCases) {
        StdString input(compoundCase.first);
        size_t mapSize = compoundCase.second;
        DeCompression<StdString> demangler(input);
        demangler.isRecord = true;
        StdString mangled = input;
        size_t cnt = 0;
        size_t nextIdx = demangler.ForwardType(mangled, cnt, 0);
        EXPECT_EQ(nextIdx, input.Length()) << "compound encode " << compoundCase.first;
        EXPECT_EQ(cnt, mapSize) << "compound encode " << compoundCase.first;
        ASSERT_EQ(demangler.treeIdMap.size(), mapSize) << "compound encode " << compoundCase.first;
        EXPECT_EQ(std::get<0>(demangler.treeIdMap[0]), 0u) << "compound encode " << compoundCase.first;
        EXPECT_EQ(std::get<1>(demangler.treeIdMap[0]), input.Length()) << "compound encode "
            << compoundCase.first;
        EXPECT_STREQ(mangled.Str(), input.Str()) << "compound encode " << compoundCase.first;
    }
}

TEST(DemangleTest, ForwardTypeRecordModeMatchesNoRecordMode)
{
    // ForwardType reads the type prefix only after pushing the treeIdMap
    // placeholder. The push must neither observe nor modify the mangled text,
    // so recording and non-recording runs must agree on the end index and the
    // resulting content for every encode.
    const std::vector<std::string> inputs = {
        "nE",
        "DhE",
        "DxE",
        "kE",
        "Y_E",
        "Y0_E",
        "CN4main3FooE",
        "F0iE",
        "T0_iE",
        "Pi",
        "A0_i",
        "G4test_",
        "xE",
    };
    for (const auto& input : inputs) {
        DeCompression<StdString> noRecordDemangler(input);
        StdString noRecordMangled = input;
        size_t noRecordCnt = 0;
        size_t noRecordNext = noRecordDemangler.ForwardType(noRecordMangled, noRecordCnt, 0);

        DeCompression<StdString> recordDemangler(input);
        recordDemangler.isRecord = true;
        StdString recordMangled = input;
        size_t recordCnt = 0;
        size_t recordNext = recordDemangler.ForwardType(recordMangled, recordCnt, 0);

        EXPECT_EQ(recordNext, noRecordNext) << "input " << input;
        EXPECT_STREQ(recordMangled.Str(), noRecordMangled.Str()) << "input " << input;
    }
}

TEST(DemangleTest, ForwardCompressedTypeResolvesRecordedSpan)
{
    // Forward the referent first so its span lands in treeIdMap, then forward
    // the Y reference: the reference is replaced by the recorded span in the
    // mangled text, and no span is recorded for the reference itself.
    StdString input = "CN4main3FooEiY_";
    DeCompression<StdString> demangler(input);
    demangler.isRecord = true;
    StdString mangled = input;
    size_t cnt = 0;
    EXPECT_EQ(demangler.ForwardType(mangled, cnt, 0), 12u);
    EXPECT_EQ(demangler.ForwardType(mangled, cnt, 12), 13u);
    EXPECT_EQ(demangler.ForwardType(mangled, cnt, 13), 25u);
    EXPECT_STREQ(mangled.Str(), "CN4main3FooEiCN4main3FooE");
    EXPECT_EQ(cnt, 3u);
    ASSERT_EQ(demangler.treeIdMap.size(), 3u);
    EXPECT_EQ(std::get<0>(demangler.treeIdMap[0]), 0u);
    EXPECT_EQ(std::get<1>(demangler.treeIdMap[0]), 12u);
}

TEST(DemangleTest, ForwardCompressedTypeResolvesChainedReference)
{
    // With the span of "Pi" recorded in treeIdMap, two chained Y_ references
    // resolve back-to-back inside a single ForwardType call.
    StdString input = "PiY_Y_";
    DeCompression<StdString> demangler(input);
    demangler.isRecord = true;
    StdString mangled = input;
    size_t cnt = 0;
    EXPECT_EQ(demangler.ForwardType(mangled, cnt, 0), 2u);
    EXPECT_EQ(demangler.ForwardType(mangled, cnt, 2), 6u);
    EXPECT_STREQ(mangled.Str(), "PiPiPi");
    EXPECT_EQ(cnt, 1u);
    ASSERT_EQ(demangler.treeIdMap.size(), 1u);
    EXPECT_EQ(std::get<0>(demangler.treeIdMap[0]), 0u);
    EXPECT_EQ(std::get<1>(demangler.treeIdMap[0]), 2u);
}

TEST(DemangleTest, ForwardCompressedTypeKeepsUnresolvableReference)
{
    // A reference whose index is outside treeIdMap makes no progress and is
    // left untouched in the mangled text, with the placeholder popped.
    StdString input = "PiY9_";
    DeCompression<StdString> demangler(input);
    demangler.isRecord = true;
    StdString mangled = input;
    size_t cnt = 0;
    EXPECT_EQ(demangler.ForwardType(mangled, cnt, 0), 2u);
    EXPECT_EQ(demangler.ForwardType(mangled, cnt, 2), 2u);
    EXPECT_STREQ(mangled.Str(), "PiY9_");
    EXPECT_EQ(cnt, 1u);
    ASSERT_EQ(demangler.treeIdMap.size(), 1u);
}
