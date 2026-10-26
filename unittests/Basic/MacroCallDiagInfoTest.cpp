// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/Basic/MacroCallDiagInfo.h"
#include "cangjie/Basic/Position.h"

#include "gtest/gtest.h"

using namespace Cangjie;

namespace {
// Mimic the info of a macro call `@Moo` at (1, 5, 1): its expanded tokens are repacked onto
// line 5 of file 1 (see RefreshNewTokensPos), repacked column 40/60 maps to (331, 9, 3)/(331, 9, 10)
// in the *.macrocall file.
MacroCallDiagInfo MakeInfo()
{
    MacroCallDiagInfo info;
    info.macroCallBegin = Position(1, 5, 1);
    info.macroCallEnd = Position(1, 5, 9);
    info.isCurFile = false;
    info.new2macroCallPosMap[40] = Position(331, 9, 3);
    info.new2macroCallPosMap[60] = Position(331, 9, 10);
    return info;
}
} // namespace

TEST(MacroCallDiagInfoTest, MapPosKeepsPositionOfOriginalSource)
{
    auto info = MakeInfo();
    // A position on another line belongs to the original source, not to the repacked
    // expansion tokens. It must pass through untouched, for both begin and end lookups.
    EXPECT_EQ(info.MapPos(Position(1, 103, 20)), Position(1, 103, 20));
    EXPECT_EQ(info.MapPos(Position(1, 105, 14), true), Position(1, 105, 14));
}

TEST(MacroCallDiagInfoTest, MapPosKeepsOriginalRangeOrdered)
{
    // Regression of the accept.cj ICE: a CHIR warning range of user code inside a macro
    // call must stay begin <= end after remapping (DiagnosticEmitter asserts this).
    auto info = MakeInfo();
    auto begin = info.MapPos(Position(1, 103, 20));
    auto end = info.MapPos(Position(1, 105, 14), true);
    EXPECT_EQ(begin, Position(1, 103, 20));
    EXPECT_EQ(end, Position(1, 105, 14));
    EXPECT_LE(begin, end);
}

TEST(MacroCallDiagInfoTest, MapPosKeepsPositionOfOtherFile)
{
    auto info = MakeInfo();
    EXPECT_EQ(info.MapPos(Position(7, 5, 40)), Position(7, 5, 40));
    EXPECT_EQ(info.MapPos(Position(7, 5, 40), true), Position(7, 5, 40));
}

TEST(MacroCallDiagInfoTest, MapPosMapsRepackedBeginPosition)
{
    auto info = MakeInfo();
    EXPECT_EQ(info.MapPos(Position(1, 5, 40)), Position(331, 9, 3));
    // A repacked column without an exact entry keeps the position.
    EXPECT_EQ(info.MapPos(Position(1, 5, 45)), Position(1, 5, 45));
}

TEST(MacroCallDiagInfoTest, MapPosMapsRepackedEndPositionByLowerBound)
{
    auto info = MakeInfo();
    // End of a token that begins at repacked column 40: lower_bound finds key 40,
    // column offset (50 - 40) is added to the mapped begin (331, 9, 3).
    EXPECT_EQ(info.MapPos(Position(1, 5, 50), true), Position(331, 9, 13));
}

TEST(MacroCallDiagInfoTest, MapPosUsesHash32KeysWhenIsCurFile)
{
    MacroCallDiagInfo info;
    info.macroCallBegin = Position(1, 5, 1);
    info.isCurFile = true;
    info.new2macroCallPosMap[Position(5, 40).Hash32()] = Position(331, 9, 3);
    info.new2macroCallPosMap[Position(5, 60).Hash32()] = Position(331, 9, 10);
    // Same line and column: found via Hash32.
    EXPECT_EQ(info.MapPos(Position(1, 5, 40)), Position(331, 9, 3));
    // Same line, between tokens: lower_bound finds the entry of this line (key 40).
    EXPECT_EQ(info.MapPos(Position(1, 5, 50), true), Position(331, 9, 13));
    // Another line: exact lookup never collides with the Hash32 keys, and a lower_bound
    // hit from another line must not be remapped either.
    EXPECT_EQ(info.MapPos(Position(1, 103, 40)), Position(1, 103, 40));
    EXPECT_EQ(info.MapPos(Position(1, 103, 40), true), Position(1, 103, 40));
    EXPECT_EQ(info.MapPos(Position(1, 103, 45), true), Position(1, 103, 45));
}
