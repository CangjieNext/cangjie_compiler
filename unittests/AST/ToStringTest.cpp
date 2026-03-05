// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <gtest/gtest.h>

#include "cangjie/AST/Node.h"

using namespace Cangjie;
using namespace AST;

namespace {
Position Pos(int line, int column)
{
    return Position{0, line, column};
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
    MemberAccess member;
    member.baseExpr = MakeRefExpr("a", 1, 1);
    member.dotPos = Pos(1, 2);
    InitIdentifier(member.field, "b", Pos(1, 3), Pos(1, 4));
    EXPECT_EQ("a.b", member.ToString());

    CallExpr call;
    call.baseFunc = MakeRefExpr("foo", 2, 1);
    call.leftParenPos = Pos(2, 4);
    call.rightParenPos = Pos(2, 6);
    auto arg = MakeOwned<FuncArg>();
    arg->expr = MakeIntLiteral("1", 2, 5);
    arg->begin = arg->expr->begin;
    arg->end = arg->expr->end;
    call.args.emplace_back(std::move(arg));
    EXPECT_EQ("foo(1)", call.ToString());

    ArrayLit arrayLit;
    arrayLit.begin = Pos(3, 1);
    arrayLit.rightSquarePos = Pos(3, 4);
    arrayLit.children.emplace_back(MakeIntLiteral("1", 3, 2));
    EXPECT_EQ("[1 ]", arrayLit.ToString());
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
