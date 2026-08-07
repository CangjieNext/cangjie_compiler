// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * Unit tests for the Comment related ToString() serializations
 * (Comment / CommentGroup / CommentGroups). Covers every CommentStyle and
 * CommentKind enum branch, plus the leading/inner/trailing group permutations
 * (including the comma-gluing logic between groups and the IsEmpty early-outs).
 */

#include <string>

#include <gtest/gtest.h>

#include "cangjie/AST/Comment.h"
#include "cangjie/Lex/Token.h"

using namespace Cangjie;
using namespace AST;

namespace {
// Comment has no default ctor (its Token member has none), so aggregate-init it.
Comment MakeComment(CommentStyle style, CommentKind kind, const std::string& value)
{
    return Comment{style, kind, Token(TokenKind::IDENTIFIER, value)};
}

CommentGroup MakeGroup(std::vector<Comment> cms)
{
    CommentGroup g;
    g.cms = std::move(cms);
    return g;
}
} // namespace

// ---------------------------------------------------------------------------
// Comment::ToString
// ---------------------------------------------------------------------------

TEST(CommentTest, ToStringLeadLineLineComment)
{
    auto c = MakeComment(CommentStyle::LEAD_LINE, CommentKind::LINE, "c1");
    EXPECT_EQ(c.ToString(), "{\"style\":\"leadLine\", \"kind\":\"line\", \"info\":\"c1\"}");
}

TEST(CommentTest, ToStringTrailCodeBlockComment)
{
    auto c = MakeComment(CommentStyle::TRAIL_CODE, CommentKind::BLOCK, "blk");
    EXPECT_EQ(c.ToString(), "{\"style\":\"trailCode\", \"kind\":\"block\", \"info\":\"blk\"}");
}

TEST(CommentTest, ToStringOtherDocumentComment)
{
    auto c = MakeComment(CommentStyle::OTHER, CommentKind::DOCUMENT, "doc");
    EXPECT_EQ(c.ToString(), "{\"style\":\"other\", \"kind\":\"doc\", \"info\":\"doc\"}");
}

TEST(CommentTest, ToStringEscapesJsonSpecialChars)
{
    // A value containing a double-quote must be JSON-escaped by ToString.
    auto c = MakeComment(CommentStyle::LEAD_LINE, CommentKind::LINE, "a\"b");
    EXPECT_EQ(c.ToString(), "{\"style\":\"leadLine\", \"kind\":\"line\", \"info\":\"a\\\"b\"}");
}

// ---------------------------------------------------------------------------
// CommentGroup::ToString
// ---------------------------------------------------------------------------

TEST(CommentTest, EmptyGroupSerializesEmptyCms)
{
    CommentGroup g;
    EXPECT_TRUE(g.IsEmpty());
    EXPECT_EQ(g.ToString(), "{\"cms\":[]}");
}

TEST(CommentTest, SingleCommentGroupHasNoComma)
{
    auto g = MakeGroup({MakeComment(CommentStyle::LEAD_LINE, CommentKind::LINE, "x")});
    EXPECT_EQ(g.ToString(), "{\"cms\":[{\"style\":\"leadLine\", \"kind\":\"line\", \"info\":\"x\"}]}");
}

TEST(CommentTest, MultiCommentGroupJoinsWithComma)
{
    auto g = MakeGroup({
        MakeComment(CommentStyle::LEAD_LINE, CommentKind::LINE, "a"),
        MakeComment(CommentStyle::TRAIL_CODE, CommentKind::BLOCK, "b"),
    });
    // The needComma flag flips after the first element, so the second is
    // preceded by ", ".
    EXPECT_EQ(g.ToString(),
              "{\"cms\":[{\"style\":\"leadLine\", \"kind\":\"line\", \"info\":\"a\"}, "
              "{\"style\":\"trailCode\", \"kind\":\"block\", \"info\":\"b\"}]}");
}

// ---------------------------------------------------------------------------
// CommentGroups::ToString
// ---------------------------------------------------------------------------

TEST(CommentTest, EmptyGroupsSerializesBracesOnly)
{
    CommentGroups gs;
    EXPECT_TRUE(gs.IsEmpty());
    EXPECT_EQ(gs.ToString(), "{}");
}

TEST(CommentTest, LeadingOnlyGroups)
{
    CommentGroups gs;
    gs.leadingComments.push_back(MakeGroup({MakeComment(CommentStyle::LEAD_LINE, CommentKind::LINE, "l1")}));
    gs.leadingComments.push_back(MakeGroup({MakeComment(CommentStyle::LEAD_LINE, CommentKind::LINE, "l2")}));
    // Two leading groups: the second is preceded by ", ".
    EXPECT_EQ(gs.ToString(),
              "{\"leadingComments\":[{\"cms\":[{\"style\":\"leadLine\", \"kind\":\"line\", \"info\":\"l1\"}]}, "
              "{\"cms\":[{\"style\":\"leadLine\", \"kind\":\"line\", \"info\":\"l2\"}]}]}");
}

TEST(CommentTest, InnerOnlyGroups)
{
    CommentGroups gs;
    gs.innerComments.push_back(MakeGroup({MakeComment(CommentStyle::OTHER, CommentKind::DOCUMENT, "i1")}));
    EXPECT_EQ(gs.ToString(),
              "{\"innerComments\":[{\"cms\":[{\"style\":\"other\", \"kind\":\"doc\", \"info\":\"i1\"}]}]}");
}

TEST(CommentTest, TrailingOnlyGroups)
{
    CommentGroups gs;
    gs.trailingComments.push_back(MakeGroup({MakeComment(CommentStyle::TRAIL_CODE, CommentKind::BLOCK, "t1")}));
    EXPECT_EQ(gs.ToString(),
              "{\"trailingComments\":[{\"cms\":[{\"style\":\"trailCode\", \"kind\":\"block\", \"info\":\"t1\"}]}]}");
}

TEST(CommentTest, LeadingAndInnerSeparatesWithComma)
{
    // needComma is true after leading -> ", " before innerComments header.
    CommentGroups gs;
    gs.leadingComments.push_back(MakeGroup({MakeComment(CommentStyle::LEAD_LINE, CommentKind::LINE, "l")}));
    gs.innerComments.push_back(MakeGroup({MakeComment(CommentStyle::OTHER, CommentKind::DOCUMENT, "i")}));
    EXPECT_EQ(gs.ToString(),
              "{\"leadingComments\":[{\"cms\":[{\"style\":\"leadLine\", \"kind\":\"line\", \"info\":\"l\"}]}], "
              "\"innerComments\":[{\"cms\":[{\"style\":\"other\", \"kind\":\"doc\", \"info\":\"i\"}]}]}");
}

TEST(CommentTest, InnerAndTrailingSeparatesWithComma)
{
    // After inner, needComma is set true -> ", " before trailingComments header.
    CommentGroups gs;
    gs.innerComments.push_back(MakeGroup({MakeComment(CommentStyle::OTHER, CommentKind::DOCUMENT, "i")}));
    gs.trailingComments.push_back(MakeGroup({MakeComment(CommentStyle::TRAIL_CODE, CommentKind::BLOCK, "t")}));
    EXPECT_EQ(gs.ToString(),
              "{\"innerComments\":[{\"cms\":[{\"style\":\"other\", \"kind\":\"doc\", \"info\":\"i\"}]}], "
              "\"trailingComments\":[{\"cms\":[{\"style\":\"trailCode\", \"kind\":\"block\", \"info\":\"t\"}]}]}");
}

TEST(CommentTest, LeadingInnerTrailingAllPresent)
{
    // Exercises every ", " glue path: leading->inner, inner->trailing.
    CommentGroups gs;
    gs.leadingComments.push_back(MakeGroup({MakeComment(CommentStyle::LEAD_LINE, CommentKind::LINE, "l")}));
    gs.innerComments.push_back(MakeGroup({MakeComment(CommentStyle::OTHER, CommentKind::DOCUMENT, "i")}));
    gs.trailingComments.push_back(MakeGroup({MakeComment(CommentStyle::TRAIL_CODE, CommentKind::BLOCK, "t")}));
    const std::string expected =
        "{\"leadingComments\":[{\"cms\":[{\"style\":\"leadLine\", \"kind\":\"line\", \"info\":\"l\"}]}], "
        "\"innerComments\":[{\"cms\":[{\"style\":\"other\", \"kind\":\"doc\", \"info\":\"i\"}]}], "
        "\"trailingComments\":[{\"cms\":[{\"style\":\"trailCode\", \"kind\":\"block\", \"info\":\"t\"}]}]}";
    EXPECT_EQ(gs.ToString(), expected);
}

TEST(CommentTest, MultipleInnerGroupsJoinWithComma)
{
    // Inner-group comma uses a bare "," (not ", ") between groups -- distinct
    // from the ", " used to separate sections.
    CommentGroups gs;
    gs.innerComments.push_back(MakeGroup({MakeComment(CommentStyle::OTHER, CommentKind::DOCUMENT, "i1")}));
    gs.innerComments.push_back(MakeGroup({MakeComment(CommentStyle::OTHER, CommentKind::DOCUMENT, "i2")}));
    EXPECT_EQ(gs.ToString(),
              "{\"innerComments\":[{\"cms\":[{\"style\":\"other\", \"kind\":\"doc\", \"info\":\"i1\"}]},"
              "{\"cms\":[{\"style\":\"other\", \"kind\":\"doc\", \"info\":\"i2\"}]}]}");
}
