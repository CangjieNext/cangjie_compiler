// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <gtest/gtest.h>

#include "cangjie/AST/Node.h"

#include "cangjie/Parse/Parser.h"
#include "cangjie/Basic/DiagnosticEngine.h"

using namespace Cangjie;
using namespace AST;

namespace {
Position Pos(int line, int column)
{
    return Position{0, line, column};
}

OwnedPtr<Expr> ParseExprFromSrc(const std::string& src)
{
    static DiagnosticEngine diag;
    static SourceManager sm;
    sm.AddSource("./", src);
    diag.SetSourceManager(&sm);
    Parser parser(src, diag, diag.GetSourceManager(), {});
    return parser.ParseExpr();
}

void InitIdentifier(Identifier& id, std::string value, const Position& begin, const Position& end)
{
    id = value;
    id.SetPos(begin, end);
}

OwnedPtr<RefExpr> MakeRefExpr(const std::string& name, int line, int column)
{
    auto ref = MakeOwned<RefExpr>();
    auto begin = Pos(line, column);
    auto end = Pos(line, column + static_cast<int>(name.size()));
    InitIdentifier(ref->ref.identifier, name, begin, end);
    ref->begin = begin;
    ref->end = end;
    return ref;
}

OwnedPtr<LitConstExpr> MakeIntLiteral(const std::string& value, int line, int column)
{
    auto lit = MakeOwned<LitConstExpr>(LitConstKind::INTEGER, value);
    lit->begin = Pos(line, column);
    lit->end = Pos(line, column + static_cast<int>(value.size()));
    return lit;
}
} // namespace

TEST(ToStringTest, BasicNodeSourceRestore)
{
    auto ExpectExprToString = [](const std::string& src) {
        auto expr = ParseExprFromSrc(src);
        ASSERT_NE(expr, nullptr) << "Failed to parse: " << src;
        EXPECT_EQ(src, expr->ToString());
    };

    ExpectExprToString("a.b");
    ExpectExprToString("foo()");
    ExpectExprToString("foo(1)");
    ExpectExprToString("foo(1, 2)");
    ExpectExprToString("[1]");
    ExpectExprToString("[1, 2]");
}

TEST(ToStringTest, BasicNodeNullSafety)
{
    CallExpr callWithoutBase;
    auto arg = MakeOwned<FuncArg>();
    arg->expr = MakeIntLiteral("1", 4, 2);
    callWithoutBase.args.emplace_back(std::move(arg));
    EXPECT_EQ("(1)", callWithoutBase.ToString());

    FuncArg argWithoutExpr;
    argWithoutExpr.name = SrcIdentifier("name");
    argWithoutExpr.colonPos = Pos(5, 5);
    EXPECT_EQ("name:", argWithoutExpr.ToString());

    MemberAccess memberWithoutBase;
    memberWithoutBase.field = SrcIdentifier("field");
    EXPECT_EQ("field", memberWithoutBase.ToString());

    ArrayExpr arrayWithoutType;
    auto arrayArg = MakeOwned<FuncArg>();
    arrayArg->expr = MakeIntLiteral("2", 6, 2);
    arrayWithoutType.args.emplace_back(std::move(arrayArg));
    EXPECT_EQ("(2)", arrayWithoutType.ToString());

    EXPECT_NO_THROW({
        (void)callWithoutBase.ToString();
        (void)argWithoutExpr.ToString();
        (void)memberWithoutBase.ToString();
        (void)arrayWithoutType.ToString();
    });
}
