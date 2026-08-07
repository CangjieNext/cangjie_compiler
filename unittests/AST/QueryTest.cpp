// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * Unit tests for the query DSL parser (QueryParser) and the Query tree's
 * PrettyPrint. Exercises boolean combinators (AND/OR/NOT), parenthesised
 * clauses, normal terms with precise/prefix/suffix match kinds, position terms
 * with every comparator, several malformed-input error paths, and the operator
 * rendering fallback in OpToStr.
 *
 * Each test constructs a QueryParser over a query string with a private
 * DiagnosticEngine + SourceManager and inspects the resulting Query tree.
 */

#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "QueryParser.h"
#include "cangjie/AST/Query.h"
#include "cangjie/Basic/DiagnosticEngine.h"

using namespace Cangjie;

namespace {
// Parse a query string into a tree. Returns nullptr on parse failure.
std::unique_ptr<Query> ParseQuery(const std::string& src)
{
    DiagnosticEngine diag;
    SourceManager sm;
    sm.AddSource("./", src);
    diag.SetSourceManager(&sm);
    QueryParser parser(src, diag, diag.GetSourceManager());
    return parser.Parse();
}

// PrettyPrint a (possibly null) query into a string.
std::string PrettyPrint(const Query* q)
{
    std::string result;
    if (q != nullptr) {
        q->PrettyPrint(result);
    }
    return result;
}
} // namespace

// ---------------------------------------------------------------------------
// Single normal term: precise / prefix / suffix match kinds
// ---------------------------------------------------------------------------

TEST(QueryTest, PreciseNormalTerm)
{
    auto q = ParseQuery("name:foo");
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(q->key, "name");
    EXPECT_EQ(q->value, "foo");
    EXPECT_EQ(q->matchKind, MatchKind::PRECISE);
    EXPECT_EQ(q->sign, "=");
    EXPECT_EQ(PrettyPrint(q.get()), "name=foo");
}

TEST(QueryTest, PrefixNormalTerm)
{
    // `foo*` -> PREFIX.
    auto q = ParseQuery("name:foo*");
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(q->value, "foo");
    EXPECT_EQ(q->matchKind, MatchKind::PREFIX);
    EXPECT_EQ(PrettyPrint(q.get()), "name=foo*");
}

TEST(QueryTest, SuffixNormalTerm)
{
    // `*foo` -> SUFFIX.
    auto q = ParseQuery("name:*foo");
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(q->value, "foo");
    EXPECT_EQ(q->matchKind, MatchKind::SUFFIX);
    EXPECT_EQ(PrettyPrint(q.get()), "name=*foo");
}

TEST(QueryTest, SuffixWildcardOnlyTerm)
{
    // `key:*` with a bare wildcard suffix -> SUFFIX with an empty value; the
    // END marker is handled by the explicit END-first check rather than as a
    // (empty) literal value.
    auto q = ParseQuery("name:*");
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(q->value, "");
    EXPECT_EQ(q->matchKind, MatchKind::SUFFIX);
    EXPECT_EQ(PrettyPrint(q.get()), "name=*");
}

// ---------------------------------------------------------------------------
// Boolean combinators
// ---------------------------------------------------------------------------

TEST(QueryTest, AndCombinesTwoTerms)
{
    // The boolean combinators are the symbolic tokens && / || / !.
    auto q = ParseQuery("a:1 && b:2");
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(q->type, QueryType::OP);
    EXPECT_EQ(q->op, Operator::AND);
    ASSERT_NE(q->left, nullptr);
    ASSERT_NE(q->right, nullptr);
    EXPECT_EQ(q->left->value, "1");
    EXPECT_EQ(q->right->value, "2");
    EXPECT_EQ(PrettyPrint(q.get()), "(a=1&&b=2)");
}

TEST(QueryTest, OrCombinesTwoTerms)
{
    auto q = ParseQuery("a:1 || b:2");
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(q->op, Operator::OR);
    EXPECT_EQ(PrettyPrint(q.get()), "(a=1||b=2)");
}

TEST(QueryTest, NotCombinesTwoTerms)
{
    // `!` lexes as the NOT combinator.
    auto q = ParseQuery("a:1 ! b:2");
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(q->op, Operator::NOT);
    EXPECT_EQ(PrettyPrint(q.get()), "(a=1!b=2)");
}

TEST(QueryTest, NestedBooleanIsRightNested)
{
    // Left-associative nesting: a && b && c => (a && b) && c.
    auto q = ParseQuery("a:1 && b:2 && c:3");
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(q->op, Operator::AND);
    EXPECT_EQ(q->right->value, "3");
    EXPECT_EQ(q->left->op, Operator::AND);
}

// ---------------------------------------------------------------------------
// Parenthesised clauses
// ---------------------------------------------------------------------------

TEST(QueryTest, ParenthesisedClause)
{
    auto q = ParseQuery("(a:1)");
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(q->value, "1");
}

TEST(QueryTest, ParenthesisedBooleanGrouping)
{
    auto q = ParseQuery("(a:1 || b:2) && c:3");
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(q->op, Operator::AND);
    EXPECT_EQ(q->right->value, "3");
    EXPECT_EQ(q->left->op, Operator::OR);
}

TEST(QueryTest, UnclosedParenFails)
{
    // Missing ')' -> ParseParenClause diagnoses and returns nullptr.
    auto q = ParseQuery("(a:1");
    EXPECT_EQ(q, nullptr);
}

// ---------------------------------------------------------------------------
// Position terms: _ <op> (fileID, line, column)
// ---------------------------------------------------------------------------

TEST(QueryTest, PositionTermWithEqual)
{
    auto q = ParseQuery("_=(1,2,3)");
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(q->type, QueryType::POS);
    EXPECT_EQ(q->sign, "=");
    EXPECT_EQ(q->pos.fileID, 1u);
    EXPECT_EQ(q->pos.line, 2);
    EXPECT_EQ(q->pos.column, 3);
}

TEST(QueryTest, PositionTermWithLessThan)
{
    auto q = ParseQuery("_<(4,5,6)");
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(q->sign, "<");
}

TEST(QueryTest, PositionTermWithLessEqual)
{
    auto q = ParseQuery("_<=(7,8,9)");
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(q->sign, "<=");
}

TEST(QueryTest, PositionTermWithGreaterThan)
{
    auto q = ParseQuery("_>(10,11,12)");
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(q->sign, ">");
}

TEST(QueryTest, PositionTermWithGreaterEqual)
{
    // >= is parsed via the GT+ASSIGN combinator branch.
    auto q = ParseQuery("_>=(13,14,15)");
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(q->sign, ">=");
    EXPECT_EQ(q->pos.fileID, 13u);
}

TEST(QueryTest, PositionTermPrettyPrint)
{
    auto q = ParseQuery("_=(1,2,3)");
    ASSERT_NE(q, nullptr);
    const std::string out = PrettyPrint(q.get());
    // The position term's key is the WILDCARD token's display name ("wildcard"),
    // rendered as "<key><sign>(<pos.ToString>())".
    EXPECT_EQ(out.substr(0, 10), "wildcard=(");
    EXPECT_EQ(out.back(), ')');
}

TEST(QueryTest, PositionTermMissingComparatorFails)
{
    // No comparator -> ParseComparator returns nullopt -> error.
    auto q = ParseQuery("_(1,2,3)");
    EXPECT_EQ(q, nullptr);
}

TEST(QueryTest, PositionTermMissingOpenParenFails)
{
    auto q = ParseQuery("_=1,2,3)");
    EXPECT_EQ(q, nullptr);
}

TEST(QueryTest, PositionTermMissingFileIdFails)
{
    // Comparator + '(' but no INTEGER_LITERAL for fileID.
    auto q = ParseQuery("_=(,2,3)");
    EXPECT_EQ(q, nullptr);
}

TEST(QueryTest, PositionTermMissingCommaFails)
{
    auto q = ParseQuery("_=(1 2 3)");
    EXPECT_EQ(q, nullptr);
}

TEST(QueryTest, PositionTermMissingLineNumFails)
{
    auto q = ParseQuery("_=(1,,3)");
    EXPECT_EQ(q, nullptr);
}

TEST(QueryTest, PositionTermMissingColumnNumFails)
{
    auto q = ParseQuery("_=(1,2,)");
    EXPECT_EQ(q, nullptr);
}

TEST(QueryTest, PositionTermMissingCloseParenFails)
{
    auto q = ParseQuery("_=(1,2,3");
    EXPECT_EQ(q, nullptr);
}

// ---------------------------------------------------------------------------
// Malformed-input error paths
// ---------------------------------------------------------------------------

TEST(QueryTest, EmptyQueryFails)
{
    // Nothing parseable as a term -> expected-query-symbol diagnosis.
    auto q = ParseQuery("");
    EXPECT_EQ(q, nullptr);
}

TEST(QueryTest, NormalTermMissingColonFails)
{
    // `name` with no `:` -> ParseNormalTerm diagnoses expected ':'.
    auto q = ParseQuery("name");
    EXPECT_EQ(q, nullptr);
}

TEST(QueryTest, NormalTermTrailingEndYieldsEmptyValue)
{
    // `name:` leaves END as the value position; the END-first check accepts it
    // as an empty (precise) value rather than diagnosing an invalid value.
    auto q = ParseQuery("name:");
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(q->value, "");
    EXPECT_EQ(q->matchKind, MatchKind::PRECISE);
    EXPECT_EQ(PrettyPrint(q.get()), "name=");
}

TEST(QueryTest, NormalTermDollarValueFails)
{
    // A '$' value triggers the lex_unrecognized_symbol diagnosis.
    auto q = ParseQuery("name:$");
    EXPECT_EQ(q, nullptr);
}

TEST(QueryTest, BooleanRightSideMissingTermFails)
{
    // `a:1 AND` with no following term -> right-side parse fails.
    auto q = ParseQuery("a:1 AND");
    EXPECT_EQ(q, nullptr);
}

TEST(QueryTest, UnexpectedTokenAfterTermFails)
{
    // A term followed by a non-logic token -> unexpected-end diagnosis.
    auto q = ParseQuery("a:1 b:2");
    EXPECT_EQ(q, nullptr);
}

// ---------------------------------------------------------------------------
// OpToStr fallback (default branch, reached only via an out-of-range op)
// ---------------------------------------------------------------------------

TEST(QueryTest, PrettyPrintUnknownOpFallsBackToInteger)
{
    // Construct a Query node whose op is outside the AND/OR/NOT set and render
    // it through PrettyPrint to hit OpToStr's default branch.
    Query opNode(Operator::AND);
    opNode.op = static_cast<Operator>(999); // out of range -> default branch
    opNode.type = QueryType::OP;
    // left/right must be non-null for PrettyPrint to recurse safely.
    opNode.left = std::make_unique<Query>("a", "1");
    opNode.right = std::make_unique<Query>("b", "2");
    std::string out;
    opNode.PrettyPrint(out);
    // Default branch renders the op as its integer representation.
    EXPECT_NE(out.find("999"), std::string::npos);
}
