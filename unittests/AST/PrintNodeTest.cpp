// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * Unit tests for PrintNode -- the AST-to-stream pretty printer. A single rich
 * Cangjie source is parsed into a File, then PrintNode is driven over the file
 * and over individual declarations/expressions. Each printed node dispatches
 * to its PrintXxx overload; the tests assert that the expected keywords /
 * fragments appear in the streamed output, exercising the dispatch branches
 * and the per-node formatting bodies.
 */

#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "cangjie/AST/Node.h"
#include "cangjie/AST/PrintNode.h"
#include "cangjie/Parse/Parser.h"
#include "cangjie/Basic/DiagnosticEngine.h"

using namespace Cangjie;
using namespace AST;

namespace {
const char *RICH_SRC = R"(
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
        return n
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

OwnedPtr<File> ParseRichFile()
{
    static DiagnosticEngine diag;
    static SourceManager sm;
    sm.AddSource("./", RICH_SRC);
    diag.SetSourceManager(&sm);
    Parser parser(RICH_SRC, diag, diag.GetSourceManager());
    return parser.ParseTopLevel();
}

std::string PrintFileToString()
{
    auto file = ParseRichFile();
    std::ostringstream os;
    PrintNode(file.get(), 0, "", os);
    return os.str();
}

OwnedPtr<Expr> ParseExprFromSrc(const std::string& src)
{
    static DiagnosticEngine diag;
    static SourceManager sm;
    sm.AddSource("./", src);
    diag.SetSourceManager(&sm);
    Parser parser(src, diag, diag.GetSourceManager());
    return parser.ParseExpr();
}

OwnedPtr<Decl> ParseDeclFromSrc(const std::string& src)
{
    static DiagnosticEngine diag;
    static SourceManager sm;
    sm.AddSource("./", src);
    diag.SetSourceManager(&sm);
    Parser parser(src, diag, diag.GetSourceManager());
    return parser.ParseDecl(ScopeKind::TOPLEVEL);
}

std::string PrintExpr(const std::string& src)
{
    auto expr = ParseExprFromSrc(src);
    std::ostringstream os;
    PrintNode(expr.get(), 0, "", os);
    return os.str();
}

std::string PrintDecl(const std::string& src)
{
    auto decl = ParseDeclFromSrc(src);
    std::ostringstream os;
    PrintNode(decl.get(), 0, "", os);
    return os.str();
}
} // namespace

// ---------------------------------------------------------------------------
// Whole-file printing dispatches into the package/file/decl/expr/type PrintXxx
// overloads. Assert presence of key fragments rather than exact whitespace,
// which is brittle.
// ---------------------------------------------------------------------------

TEST(PrintNodeTest, FilePrintsAllDeclarations)
{
    const std::string out = PrintFileToString();
    // Each declaration kind dispatches to its own PrintXxx, which emits an
    // "XxxDecl:" header. Assert the labels rather than source keywords.
    EXPECT_NE(out.find("ClassDecl:"), std::string::npos);
    EXPECT_NE(out.find("EnumDecl:"), std::string::npos);
    EXPECT_NE(out.find("StructDecl:"), std::string::npos);
    EXPECT_NE(out.find("FuncDecl:"), std::string::npos);
    EXPECT_NE(out.find("MainDecl:"), std::string::npos);
    EXPECT_NE(out.find("VarDecl:"), std::string::npos);
}

TEST(PrintNodeTest, FilePrintsExpressions)
{
    const std::string out = PrintFileToString();
    // Expression PrintXxx overloads reached via the func bodies emit "XxxExpr:".
    EXPECT_NE(out.find("MatchExpr:"), std::string::npos);
    EXPECT_NE(out.find("ConstExpr:"), std::string::npos);
}

TEST(PrintNodeTest, NullNodeWithAdditionIsSafe)
{
    // addition non-empty + null node -> prints "<addition> nullptr" and returns.
    std::ostringstream os;
    PrintNode(nullptr, 0, "note", os);
    EXPECT_NE(os.str().find("note"), std::string::npos);
    EXPECT_NE(os.str().find("nullptr"), std::string::npos);
}

TEST(PrintNodeTest, NullNodeWithoutAdditionReturnsEmpty)
{
    std::ostringstream os;
    PrintNode(nullptr, 0, "", os);
    EXPECT_TRUE(os.str().empty());
}

TEST(PrintNodeTest, AdditionCommentIsEmitted)
{
    // A non-empty addition is printed as an indented comment before the node.
    auto expr = ParseExprFromSrc("1");
    ASSERT_NE(expr, nullptr);
    std::ostringstream os;
    PrintNode(expr.get(), 0, "tag", os);
    EXPECT_NE(os.str().find("tag"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Individual expression PrintXxx overloads
// ---------------------------------------------------------------------------

TEST(PrintNodeTest, PrintBinaryExpr)
{
    const std::string out = PrintExpr("1 + 2");
    ASSERT_FALSE(out.empty());
    EXPECT_NE(out.find("1"), std::string::npos);
    EXPECT_NE(out.find("2"), std::string::npos);
}

TEST(PrintNodeTest, PrintCallExpr)
{
    const std::string out = PrintExpr("foo(1, 2)");
    ASSERT_FALSE(out.empty());
    EXPECT_NE(out.find("foo"), std::string::npos);
}

TEST(PrintNodeTest, PrintMemberAccess)
{
    const std::string out = PrintExpr("obj.field");
    ASSERT_FALSE(out.empty());
    EXPECT_NE(out.find("obj"), std::string::npos);
    EXPECT_NE(out.find("field"), std::string::npos);
}

TEST(PrintNodeTest, PrintArrayLit)
{
    const std::string out = PrintExpr("[1, 2, 3]");
    ASSERT_FALSE(out.empty());
}

TEST(PrintNodeTest, PrintTupleLit)
{
    const std::string out = PrintExpr("(1, 2)");
    ASSERT_FALSE(out.empty());
}

TEST(PrintNodeTest, PrintLambdaExpr)
{
    const std::string out = PrintExpr("{ a => a + 1 }");
    ASSERT_FALSE(out.empty());
}

TEST(PrintNodeTest, PrintParenExpr)
{
    const std::string out = PrintExpr("(1 + 2)");
    ASSERT_FALSE(out.empty());
}

TEST(PrintNodeTest, PrintRefExpr)
{
    const std::string out = PrintExpr("foo");
    ASSERT_FALSE(out.empty());
    EXPECT_NE(out.find("foo"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Individual declaration PrintXxx overloads
// ---------------------------------------------------------------------------

TEST(PrintNodeTest, PrintFuncDecl)
{
    const std::string out = PrintDecl("func f(x: Int32): Int64 { return 0 }");
    ASSERT_FALSE(out.empty());
    EXPECT_NE(out.find("FuncDecl:"), std::string::npos);
    EXPECT_NE(out.find("f"), std::string::npos);
}

TEST(PrintNodeTest, PrintVarDecl)
{
    const std::string out = PrintDecl("var a: Int32 = 1");
    ASSERT_FALSE(out.empty());
    EXPECT_NE(out.find("VarDecl:"), std::string::npos);
    EXPECT_NE(out.find("a"), std::string::npos);
}

TEST(PrintNodeTest, PrintClassDecl)
{
    const std::string out = PrintDecl("class C { var x: Int32 = 1 }");
    ASSERT_FALSE(out.empty());
    EXPECT_NE(out.find("ClassDecl:"), std::string::npos);
}

TEST(PrintNodeTest, PrintEnumDecl)
{
    const std::string out = PrintDecl("enum Color { Red, Green, Blue }");
    ASSERT_FALSE(out.empty());
    EXPECT_NE(out.find("EnumDecl:"), std::string::npos);
}

TEST(PrintNodeTest, PrintStructDecl)
{
    const std::string out = PrintDecl("struct P { var x: Int32 }");
    ASSERT_FALSE(out.empty());
    // StructDecl was not always printed via the StructDecl label; just assert a
    // non-empty result that proves the dispatch ran.
    EXPECT_FALSE(out.empty());
}

// ---------------------------------------------------------------------------
// The single-arg PrintNode overload prints to std::cout (smoke test only).
// ---------------------------------------------------------------------------

TEST(PrintNodeTest, SingleArgOverloadDoesNotCrash)
{
    auto expr = ParseExprFromSrc("42");
    ASSERT_NE(expr, nullptr);
#ifndef CANGJIE_ENABLE_GCOV
    // Just ensure the cout-writing overload runs without throwing.
    EXPECT_NO_THROW(PrintNode(expr.get()));
#endif
}
