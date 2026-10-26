// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
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
#include "cangjie/AST/NodeX.h"
#include "cangjie/AST/PrintNode.h"
#include "cangjie/Parse/Parser.h"
#include "cangjie/Basic/DiagnosticEngine.h"
#include "cangjie/Basic/Position.h"

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
    // Hold a long-lived std::string so the Parser/Lexer keeps a valid pointer
    // to the source buffer for the duration of parsing. Passing RICH_SRC (a
    // const char*) directly binds it to `const std::string&`, producing a
    // temporary whose c_str() the Lexer dangles after.
    static const std::string src(RICH_SRC);
    sm.AddSource("./", src);
    diag.SetSourceManager(&sm);
    Parser parser(src, diag, diag.GetSourceManager());
    return parser.ParseTopLevel();
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

// Print a directly-constructed node into a fresh stream and return the text.
// Used to exercise the PrintXxx dispatch for node kinds that the raw Parser
// either cannot produce (interface/typealias/macro/feature directives) or that
// are simpler to construct by hand than to parse. Each PrintXxx first emits a
// distinctive label ("InterfaceDecl:", "ResumeExpr {", ...), so the caller
// asserts on that label to prove the dispatch ran end-to-end.
template <typename T>
std::string PrintConstructedNode(OwnedPtr<T> node)
{
    std::ostringstream os;
    PrintNode(node.get(), 0, "", os);
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
    auto file = ParseRichFile();
    std::ostringstream os;
    PrintNode(file.get(), 0, "", os);
    const std::string out = os.str();
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
    auto file = ParseRichFile();
    std::ostringstream os;
    PrintNode(file.get(), 0, "", os);
    const std::string out = os.str();
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
    EXPECT_NE(out.find("StructDecl:"), std::string::npos);
}

// ---------------------------------------------------------------------------
// The single-arg PrintNode overload prints to std::cout (smoke test only).
// ---------------------------------------------------------------------------

TEST(PrintNodeTest, SingleArgOverloadDoesNotCrash)
{
    auto expr = ParseExprFromSrc("42");
    ASSERT_NE(expr, nullptr);
    // Just ensure the cout-writing overload runs. (An earlier build disabled
    // this under CANGJIE_ENABLE_GCOV because of a -fno-exceptions vs throw
    // conflict in the owning-pointer path; that root cause is gone, so the
    // guard is removed. We avoid EXPECT_NO_THROW because it expands to a try/
    // block, which is forbidden under -fno-exceptions.)
    PrintNode(expr.get());
    SUCCEED();
}

// ---------------------------------------------------------------------------
// Directly constructed nodes: cover the PrintXxx overloads that the Parser
// cannot reach from the RICH_SRC fixtures (interface / typealias / macro /
// feature directives and several expression/type kinds). Each node is built with
// MakeOwned and handed to PrintNode, which dispatches via match(*node) to the
// matching PrintXxx. The label each PrintXxx emits on its first line proves the
// dispatch and the formatting body both executed.
// ---------------------------------------------------------------------------

TEST(PrintNodeTest, PrintConstructedInterfaceDecl)
{
    auto node = MakeOwned<InterfaceDecl>();
    const std::string out = PrintConstructedNode(std::move(node));
    EXPECT_NE(out.find("InterfaceDecl:"), std::string::npos);
}

TEST(PrintNodeTest, PrintConstructedTypeAliasDecl)
{
    auto node = MakeOwned<TypeAliasDecl>();
    const std::string out = PrintConstructedNode(std::move(node));
    EXPECT_NE(out.find("TypeAliasDecl:"), std::string::npos);
}

TEST(PrintNodeTest, PrintConstructedInterfaceBody)
{
    auto node = MakeOwned<InterfaceBody>();
    const std::string out = PrintConstructedNode(std::move(node));
    EXPECT_NE(out.find("InterfaceBody {"), std::string::npos);
}

TEST(PrintNodeTest, PrintConstructedMacroDecls)
{
    {
        auto node = MakeOwned<MacroDecl>();
        const std::string out = PrintConstructedNode(std::move(node));
        EXPECT_NE(out.find("MacroDecl:"), std::string::npos);
    }
    {
        auto node = MakeOwned<MacroExpandDecl>();
        const std::string out = PrintConstructedNode(std::move(node));
        EXPECT_NE(out.find("MacroExpand:"), std::string::npos);
    }
    {
        auto node = MakeOwned<MacroExpandExpr>();
        const std::string out = PrintConstructedNode(std::move(node));
        EXPECT_NE(out.find("MacroExpand:"), std::string::npos);
    }
}

TEST(PrintNodeTest, PrintConstructedPrimaryCtorDecl)
{
    auto node = MakeOwned<PrimaryCtorDecl>();
    const std::string out = PrintConstructedNode(std::move(node));
    EXPECT_NE(out.find("PrimaryCtorDecl:"), std::string::npos);
}

TEST(PrintNodeTest, PrintConstructedFeatureNodes)
{
    {
        auto node = MakeOwned<FeatureId>();
        const std::string out = PrintConstructedNode(std::move(node));
        EXPECT_NE(out.find("FeatureId "), std::string::npos);
    }
    {
        auto node = MakeOwned<FeaturesSet>();
        const std::string out = PrintConstructedNode(std::move(node));
        EXPECT_NE(out.find("FeaturesSet {"), std::string::npos);
    }
    {
        auto node = MakeOwned<FeaturesDirective>();
        const std::string out = PrintConstructedNode(std::move(node));
        EXPECT_NE(out.find("FeaturesDirective:"), std::string::npos);
    }
}

TEST(PrintNodeTest, PrintConstructedExprNodes)
{
    const auto expectLabel = [](const std::string &out, const std::string &label) {
        EXPECT_NE(out.find(label), std::string::npos) << "missing label: " << label;
    };
    expectLabel(PrintConstructedNode(MakeOwned<PerformExpr>()), "PerformExpr");
    expectLabel(PrintConstructedNode(MakeOwned<ResumeExpr>()), "ResumeExpr");
    expectLabel(PrintConstructedNode(MakeOwned<IncOrDecExpr>()), "IncOrDecExpr");
    expectLabel(PrintConstructedNode(MakeOwned<OptionalExpr>()), "OptionalExpr");
    expectLabel(PrintConstructedNode(MakeOwned<OptionalChainExpr>()), "OptionalChainExpr");
    expectLabel(PrintConstructedNode(MakeOwned<InterpolationExpr>()), "InterpolationExpr:");
    expectLabel(PrintConstructedNode(MakeOwned<StrInterpolationExpr>()), "StrInterpolationExpr:");
    expectLabel(PrintConstructedNode(MakeOwned<SpawnExpr>()), "SpawnExpr");
    expectLabel(PrintConstructedNode(MakeOwned<SynchronizedExpr>()), "SynchronizedExpr");
    expectLabel(PrintConstructedNode(MakeOwned<IsExpr>()), "IsExpr");
    expectLabel(PrintConstructedNode(MakeOwned<AsExpr>()), "AsExpr");
}

TEST(PrintNodeTest, PrintConstructedIfAvailableExpr)
{
    // IfAvailableExpr is the one node that is not default-constructible: it
    // takes a FuncArg plus two LambdaExpr. Build them like CastASTTests does.
    auto arg = MakeOwned<FuncArg>();
    arg->name = "APILevel";
    arg->expr = MakeOwned<LitConstExpr>();
    auto node = MakeOwned<IfAvailableExpr>(
        std::move(arg), MakeOwned<LambdaExpr>(), MakeOwned<LambdaExpr>());
    const std::string out = PrintConstructedNode(std::move(node));
    EXPECT_NE(out.find("IfAvailableExpr"), std::string::npos);
}

TEST(PrintNodeTest, PrintConstructedTypeNodes)
{
    const auto expectLabel = [](const std::string &out, const std::string &label) {
        EXPECT_NE(out.find(label), std::string::npos) << "missing label: " << label;
    };
    expectLabel(PrintConstructedNode(MakeOwned<ParenType>()), "ParenType {");
    expectLabel(PrintConstructedNode(MakeOwned<ThisType>(Position{})), "ThisType {");
    expectLabel(PrintConstructedNode(MakeOwned<VArrayType>()), "VArrayType {");
    expectLabel(PrintConstructedNode(MakeOwned<QualifiedType>()), "QualifiedType {");
    expectLabel(PrintConstructedNode(MakeOwned<TypePattern>()), "TypePattern {");
    // CommandTypePattern prints only the (empty) inner pattern + types list;
    // it has no header label, so just assert it ran without crashing. Avoid
    // EXPECT_NO_THROW (it expands to try{}, forbidden under -fno-exceptions).
    PrintConstructedNode(MakeOwned<CommandTypePattern>());
    SUCCEED();
}
