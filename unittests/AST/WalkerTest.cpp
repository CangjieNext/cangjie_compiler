// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <vector>
#include "gtest/gtest.h"

#define private public
#include "cangjie/AST/Match.h"
#include "cangjie/AST/PrintNode.h"
#include "cangjie/AST/Walker.h"
#include "cangjie/Parse/Parser.h"

using namespace Cangjie;
using namespace AST;

class WalkerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        std::string code = "main(argc : Int32, argv : Array<String>) {\n"
                           "	let a : Int = 40\n"
                           "	let b = 2 ** -a\n"
                           "	print((a + 3 * b, (a + 3) * b))\n"
                           "}\n";
        Parser parser(code, diag, sm);
        file = parser.ParseTopLevel();
    }

    DiagnosticEngine diag;
    SourceManager sm;
    OwnedPtr<File> file;
};

TEST_F(WalkerTest, WalkPair)
{
    int count = 0;

    Walker walker(
        file.get(),
        [&count](Ptr<Node> node) -> VisitAction {
            ++count;
            return VisitAction::WALK_CHILDREN;
        },
        [&count](Ptr<Node> node) -> VisitAction {
            --count;
            return VisitAction::WALK_CHILDREN;
        });
    walker.Walk();

    EXPECT_EQ(0, count);
}

TEST_F(WalkerTest, WalkPairSkipChildren)
{
    int count = 0;

    Walker walker(
        file.get(),
        [&count](Ptr<Node> node) -> VisitAction {
            ++count;
            return VisitAction::SKIP_CHILDREN;
        },
        [&count](Ptr<Node> node) -> VisitAction {
            --count;
            return VisitAction::WALK_CHILDREN;
        });
    walker.Walk();

    EXPECT_EQ(0, count);
}

TEST_F(WalkerTest, WalkPairStopNow)
{
    int count = 0;

    Walker walker(
        file.get(),
        [&count](Ptr<Node> node) -> VisitAction {
            ++count;
            return VisitAction::STOP_NOW;
        },
        [&count](Ptr<Node> node) -> VisitAction {
            --count;
            return VisitAction::WALK_CHILDREN;
        });
    walker.Walk();

    EXPECT_EQ(1, count);
}

TEST_F(WalkerTest, WalkShareID)
{
    // Walker and ConstWalker must share same counter.
    Walker::nextWalkerID = 1;
    ConstWalker::nextWalkerID = 1;
    auto id1 = Walker(file.get()).ID;
    auto id2 = ConstWalker(file.get()).ID;
    EXPECT_NE(id1, id2);
}

TEST_F(WalkerTest, GetDecls)
{
    std::vector<std::string> identifiers;

    Walker walker(file.get(), [&identifiers](Ptr<Node> node) -> VisitAction {
        if (auto decl = AST::As<ASTKind::DECL>(node); decl) {
            identifiers.push_back(decl->identifier);
        }
        return VisitAction::WALK_CHILDREN;
    });
    walker.Walk();

    std::string expectedIdentifiers[] = {"main", "argc", "argv", "a", "b"};

    ASSERT_EQ(std::size(expectedIdentifiers), identifiers.size());
    for (size_t i = 0; i < identifiers.size(); i++) {
        EXPECT_EQ(expectedIdentifiers[i], identifiers[i]);
    }
}

TEST_F(WalkerTest, GetDeclsPost)
{
    std::vector<std::string> identifiers;

    Walker walker(file.get(), nullptr, [&identifiers](Ptr<Node> node) -> VisitAction {
        if (auto decl = AST::As<ASTKind::DECL>(node); decl) {
            identifiers.push_back(decl->identifier);
        }
        return VisitAction::WALK_CHILDREN;
    });
    walker.Walk();

    std::string expectedIdentifiers[] = {"argc", "argv", "a", "b", "main"};

    ASSERT_EQ(std::size(expectedIdentifiers), identifiers.size());
    for (size_t i = 0; i < identifiers.size(); i++) {
        EXPECT_EQ(expectedIdentifiers[i], identifiers[i]);
    }
}

TEST_F(WalkerTest, GetCallExprs)
{
    std::vector<std::string> callExprNames;

    Walker walker(file.get(), [&callExprNames](Ptr<Node> node) -> VisitAction {
        if (auto ce = AST::As<ASTKind::CALL_EXPR>(node); ce) {
            if (auto re = AST::As<ASTKind::REF_EXPR>(ce->baseFunc.get()); re) {
                callExprNames.push_back(re->ref.identifier);
            }
        }
        return VisitAction::WALK_CHILDREN;
    });

    walker.Walk();

    std::string expectedCallExprNames[] = {"print"};

    ASSERT_EQ(std::size(expectedCallExprNames), callExprNames.size());
    for (size_t i = 0; i < callExprNames.size(); i++) {
        EXPECT_EQ(expectedCallExprNames[i], callExprNames[i]);
    }
}

// ---------------------------------------------------------------------------
// Rich-source coverage: parse a file exercising many declaration & expression
// kinds and walk it, asserting that the walker visits each node kind at least
// once. Each visited kind drives a matching case in Walker::Walk's switch.
// ---------------------------------------------------------------------------

namespace {
// A file that spans class/interface/enum/struct/extend decls plus a func body
// using match/try/throw/for/while/do-while/array/tuple/lambda expressions.
const char *RICH_SRC = R"(
interface I {
    func foo(x: Int32) {
        print(x)
    }
}
enum Color { Red, Green, Blue }
enum TimeUnit { Year(Int32) }
struct Point { var x: Int32; var y: Int32 }
class C<T> <: I where T <: I {
    var p: Point = Point(x: 1, y: 2)
    var arr: Array<Int32> = [1, 2, 3]
    var t: (Int32, Int64) = (1, 2)
    override func foo(x: Int32) {
        var time = TimeUnit.Year(2020)
        var n = match (time) {
            case Year(v) => v
            case 1 => 1
            case _ => 0
        }
        for (i in 0..3) {
            print(i)
        }
        var j = 0
        while (j < 3) {
            j = j + 1
        }
        do {
            print(x)
        } while (x > 0)
        try {
            if (x > 0) { throw x }
        } catch (e: Int32) {
            print(e)
        }
    }
}
extend C {
    func g(): Int64 { return 7 }
}
typealias T = Int32
func lambdaExample(fn: (Int64) -> Int64): Int64 {
    var f = { a: Int64 => a + 1 }
    return fn(1)
}
main(): Int64 { return 0 }
)";
} // namespace

class WalkerRichTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Match the construction pattern used by WalkerTest above and ASTToSourceTest:
        // do NOT call sm.AddSource before parsing. AddSource would register a real
        // Source under a hashed fileID, while the Lexer reads via the default
        // fileID (0). That fileID/position mismatch makes the parser emit hundreds
        // of bogus position diagnostics and drop the top-level decls, leaving an
        // empty File. Passing `sm` directly keeps Lexer and SourceManager on the
        // same (default) fileID, so positions are self-consistent.
        static const std::string src(RICH_SRC);
        Parser parser(src, diag, sm);
        file = parser.ParseTopLevel();
        ASSERT_NE(file, nullptr);
    }
    DiagnosticEngine diag;
    SourceManager sm;
    OwnedPtr<File> file;
};

// Collect every ASTKind visited during a full walk.
TEST_F(WalkerRichTest, WalksAllDeclarationKinds)
{
    std::set<ASTKind> seen;
    Walker walker(file.get(), [&seen](Ptr<Node> node) -> VisitAction {
        seen.insert(node->astKind);
        return VisitAction::WALK_CHILDREN;
    });
    walker.Walk();

    // Declaration-kind cases in Walker::Walk that the rich source must reach.
    EXPECT_EQ(seen.count(ASTKind::CLASS_DECL), 1u);
    EXPECT_EQ(seen.count(ASTKind::ENUM_DECL), 1u);
    EXPECT_EQ(seen.count(ASTKind::STRUCT_DECL), 1u);
    EXPECT_EQ(seen.count(ASTKind::EXTEND_DECL), 1u);
    EXPECT_EQ(seen.count(ASTKind::FUNC_DECL), 1u);
    EXPECT_EQ(seen.count(ASTKind::VAR_DECL), 1u);
    EXPECT_EQ(seen.count(ASTKind::MAIN_DECL), 1u);
}

TEST_F(WalkerRichTest, WalksAllExpressionKinds)
{
    std::set<ASTKind> seen;
    Walker walker(file.get(), [&seen](Ptr<Node> node) -> VisitAction {
        seen.insert(node->astKind);
        return VisitAction::WALK_CHILDREN;
    });
    walker.Walk();

    // Expression-kind cases in Walker::Walk.
    EXPECT_GE(seen.count(ASTKind::MATCH_EXPR), 1u);
    EXPECT_GE(seen.count(ASTKind::MATCH_CASE), 1u);
    EXPECT_GE(seen.count(ASTKind::TRY_EXPR), 1u);
    EXPECT_GE(seen.count(ASTKind::THROW_EXPR), 1u);
    EXPECT_GE(seen.count(ASTKind::FOR_IN_EXPR), 1u);
    EXPECT_GE(seen.count(ASTKind::WHILE_EXPR), 1u);
    EXPECT_GE(seen.count(ASTKind::DO_WHILE_EXPR), 1u);
    EXPECT_GE(seen.count(ASTKind::ARRAY_LIT), 1u);
    EXPECT_GE(seen.count(ASTKind::TUPLE_LIT), 1u);
    EXPECT_GE(seen.count(ASTKind::IF_EXPR), 1u);
    EXPECT_GE(seen.count(ASTKind::ASSIGN_EXPR), 1u);
    EXPECT_GE(seen.count(ASTKind::BINARY_EXPR), 1u);
    EXPECT_GE(seen.count(ASTKind::CALL_EXPR), 1u);
    EXPECT_GE(seen.count(ASTKind::RANGE_EXPR), 1u);
    EXPECT_GE(seen.count(ASTKind::LAMBDA_EXPR), 1u);
}

TEST_F(WalkerRichTest, WalksAllTypeKinds)
{
    std::set<ASTKind> seen;
    Walker walker(file.get(), [&seen](Ptr<Node> node) -> VisitAction {
        seen.insert(node->astKind);
        return VisitAction::WALK_CHILDREN;
    });
    walker.Walk();

    // Type-kind cases in Walker::Walk reachable from the rich source.
    EXPECT_GE(seen.count(ASTKind::REF_TYPE), 1u);
    EXPECT_GE(seen.count(ASTKind::GENERIC_CONSTRAINT), 1u);
    EXPECT_GE(seen.count(ASTKind::FUNC_TYPE), 1u);
    EXPECT_GE(seen.count(ASTKind::TUPLE_TYPE), 1u);
}

TEST_F(WalkerRichTest, WalksPatternKinds)
{
    std::set<ASTKind> seen;
    Walker walker(file.get(), [&seen](Ptr<Node> node) -> VisitAction {
        seen.insert(node->astKind);
        return VisitAction::WALK_CHILDREN;
    });
    walker.Walk();

    // Pattern-kind cases (enum patterns, var patterns, except-type patterns).
    EXPECT_GE(seen.count(ASTKind::CONST_PATTERN), 1u);
    EXPECT_GE(seen.count(ASTKind::VAR_PATTERN), 1u);
    EXPECT_GE(seen.count(ASTKind::ENUM_PATTERN), 1u);
}

// The ConstWalker (template instantiated over const Node) shares the same Walk
// dispatch; exercising it covers the ConstWalker::Walk instantiation path.
TEST_F(WalkerRichTest, ConstWalkerVisitsSameKinds)
{
    std::set<ASTKind> seen;
    ConstWalker walker(file.get(), [&seen](Ptr<const Node> node) -> VisitAction {
        seen.insert(node->astKind);
        return VisitAction::WALK_CHILDREN;
    });
    walker.Walk();

    EXPECT_GE(seen.count(ASTKind::CLASS_DECL), 1u);
    EXPECT_GE(seen.count(ASTKind::MATCH_EXPR), 1u);
}

// SKIP_CHILDREN must prune the whole subtree of a decl whose children include
// many kinds -- none of those child kinds should appear.
TEST_F(WalkerRichTest, SkipChildrenPrunesSubtree)
{
    std::set<ASTKind> seen;
    Walker walker(file.get(), [&seen](Ptr<Node> node) -> VisitAction {
        seen.insert(node->astKind);
        // Skip the first class body encountered so its descendants are not walked.
        if (node->astKind == ASTKind::CLASS_BODY) {
            return VisitAction::SKIP_CHILDREN;
        }
        return VisitAction::WALK_CHILDREN;
    });
    walker.Walk();

    // The throw expression lives only inside the skipped class body.
    EXPECT_EQ(seen.count(ASTKind::THROW_EXPR), 0u);
}
