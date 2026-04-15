// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <gtest/gtest.h>

#include "cangjie/AST/Node.h"

#include "cangjie/AST/Match.h"

#include "cangjie/Parse/Parser.h"
#include "cangjie/Basic/DiagnosticEngine.h"

using namespace Cangjie;
using namespace AST;

namespace {
OwnedPtr<Expr> ParseExprFromSrc(const std::string& src)
{
    static DiagnosticEngine diag;
    static SourceManager sm;
    sm.AddSource("./", src);
    diag.SetSourceManager(&sm);
    Parser parser(src, diag, diag.GetSourceManager(), {});
    return parser.ParseExpr();
}

OwnedPtr<Type> ParseTypeFromSrc(const std::string& src)
{
    static DiagnosticEngine diag;
    static SourceManager sm;
    sm.AddSource("./", src);
    diag.SetSourceManager(&sm);
    Parser parser(src, diag, diag.GetSourceManager(), {});
    return parser.ParseType();
}

OwnedPtr<Decl> ParseDeclFromSrc(const std::string& src)
{
    static DiagnosticEngine diag;
    static SourceManager sm;
    sm.AddSource("./", src);
    diag.SetSourceManager(&sm);
    Parser parser(src, diag, diag.GetSourceManager(), {});
    return parser.ParseDecl(ScopeKind::TOPLEVEL);
}

OwnedPtr<File> ParseFileFromSrc(const std::string& src)
{
    static DiagnosticEngine diag;
    static SourceManager sm;
    sm.AddSource("./", src);
    diag.SetSourceManager(&sm);
    Parser parser(src, diag, diag.GetSourceManager(), {});
    return parser.ParseTopLevel();
}

template <typename TypeNode>
void ExpectTypeToStringAndKind(const std::string& src)
{
    auto type = ParseTypeFromSrc(src);
    ASSERT_NE(type, nullptr) << "Failed to parse type: " << src;
    EXPECT_TRUE(Is<TypeNode>(type.get())) << src;
    EXPECT_EQ(src, type->ToString());
}

template <typename ExprNode>
void ExpectExprToStringAndKind(const std::string& src)
{
    auto expr = ParseExprFromSrc(src);
    ASSERT_NE(expr, nullptr) << "Failed to parse expr: " << src;
    EXPECT_TRUE(Is<ExprNode>(expr.get())) << src;
    EXPECT_EQ(src, expr->ToString());
}
} // namespace

TEST(ToStringTest, BasicNodeSourceRestore)
{
    auto expectExprToString = [](const std::string& src) {
        auto expr = ParseExprFromSrc(src);
        ASSERT_NE(expr, nullptr) << "Failed to parse: " << src;
        EXPECT_EQ(src, expr->ToString());
    };

    expectExprToString("a.b");
    expectExprToString("foo()");
    expectExprToString("foo(1)");
    expectExprToString("foo(1, 2)");
    expectExprToString("[1]");
    expectExprToString("[1, 2]");
}

TEST(ToStringTest, BasicNodeNullSafety)
{
    auto callExpr = ParseExprFromSrc("foo()");
    ASSERT_NE(callExpr, nullptr) << "Failed to parse expr: foo()";
    ASSERT_TRUE(Is<CallExpr>(callExpr.get()));
    auto parsedCallExpr = As<ASTKind::CALL_EXPR>(callExpr.get());
    EXPECT_TRUE(parsedCallExpr->args.empty());
    EXPECT_EQ("foo()", parsedCallExpr->ToString());

    auto namedArgCallExpr = ParseExprFromSrc("foo(name: 1)");
    ASSERT_NE(namedArgCallExpr, nullptr) << "Failed to parse expr: foo(name: 1)";
    ASSERT_TRUE(Is<CallExpr>(namedArgCallExpr.get()));
    auto parsedNamedArgCallExpr = As<ASTKind::CALL_EXPR>(namedArgCallExpr.get());
    ASSERT_EQ(parsedNamedArgCallExpr->args.size(), 1);
    ASSERT_NE(parsedNamedArgCallExpr->args.front(), nullptr);
    EXPECT_EQ("name: 1", parsedNamedArgCallExpr->args.front()->ToString());
    EXPECT_EQ("foo(name: 1)", parsedNamedArgCallExpr->ToString());

    auto memberAccessExpr = ParseExprFromSrc("obj.field");
    ASSERT_NE(memberAccessExpr, nullptr) << "Failed to parse expr: obj.field";
    ASSERT_TRUE(Is<MemberAccess>(memberAccessExpr.get()));
    auto parsedMemberAccessExpr = As<ASTKind::MEMBER_ACCESS>(memberAccessExpr.get());
    ASSERT_NE(parsedMemberAccessExpr->baseExpr, nullptr);
    EXPECT_EQ("obj.field", parsedMemberAccessExpr->ToString());

    (void)parsedCallExpr->ToString();
    (void)parsedNamedArgCallExpr->args.front()->ToString();
    (void)parsedNamedArgCallExpr->ToString();
    (void)parsedMemberAccessExpr->ToString();
}

TEST(ToStringTest, ExtendedNodesParsing)
{
    auto expectTypeToString = [](const std::string& src) {
        auto type = ParseTypeFromSrc(src);
        ASSERT_NE(type, nullptr) << "Failed to parse type: " << src;
        (void)type->ToString();
    };

    auto expectDeclToString = [](const std::string& src) {
        auto decl = ParseDeclFromSrc(src);
        ASSERT_NE(decl, nullptr) << "Failed to parse decl: " << src;
        (void)decl->ToString();
    };

    auto expectFileToString = [](const std::string& src) {
        auto file = ParseFileFromSrc(src);
        ASSERT_NE(file, nullptr) << "Failed to parse file: " << src;
        (void)file->ToString();
    };

    // Test parser coverage over RefType, VarDecl, MultiModifiers, etc.
    expectTypeToString("Int64");
    expectTypeToString("Array<Int64>");
    expectTypeToString("*Int64");

    expectDeclToString("let x = 1");
    expectDeclToString("var myVar: Int64 = 0");
    expectDeclToString("public mut var x: Int64 = 0");

    expectFileToString("package my_pkg\n");
    expectFileToString("import some_pkg.*\n");
    auto foreignFuncFile = ParseFileFromSrc("@C\nfunc c_foo() {}\n");
    ASSERT_NE(foreignFuncFile, nullptr) << "Failed to parse file: @C\\nfunc c_foo() {}\\n";
    (void)foreignFuncFile->ToString();
}

TEST(ToStringTest, TypeAndSpecialTypeRestore)
{
    ExpectTypeToStringAndKind<PrimitiveType>("Int64");
    ExpectTypeToStringAndKind<ParenType>("(Int64)");
    ExpectTypeToStringAndKind<OptionType>("?Int64");
    ExpectTypeToStringAndKind<OptionType>("??Int64");
    ExpectTypeToStringAndKind<TupleType>("(Int64, Int64)");
    ExpectTypeToStringAndKind<FuncType>("() -> Unit");
    ExpectTypeToStringAndKind<FuncType>("(Int64) -> Int64");
    ExpectTypeToStringAndKind<FuncType>("(Int64, Float64) -> Unit");
    ExpectTypeToStringAndKind<QualifiedType>("My.Type<Int64>");
    ExpectTypeToStringAndKind<VArrayType>("VArray<Int64, $3>");

    auto classDecl = ParseDeclFromSrc("class Data { func f(): This { this } }");
    ASSERT_NE(classDecl, nullptr) << "Failed to parse decl: class Data { func f(): This { this } }";
    ASSERT_TRUE(Is<ClassDecl>(classDecl.get()));
    auto parsedClassDecl = As<ASTKind::CLASS_DECL>(classDecl.get());
    ASSERT_NE(parsedClassDecl->body, nullptr);
    ASSERT_FALSE(parsedClassDecl->body->decls.empty());
    ASSERT_TRUE(Is<FuncDecl>(parsedClassDecl->body->decls.front().get()));
    auto funcDecl = As<ASTKind::FUNC_DECL>(parsedClassDecl->body->decls.front().get());
    ASSERT_NE(funcDecl->funcBody, nullptr);
    ASSERT_NE(funcDecl->funcBody->retType, nullptr);
    ASSERT_TRUE(Is<ThisType>(funcDecl->funcBody->retType.get()));
    EXPECT_EQ("This", funcDecl->funcBody->retType->ToString());

    auto varrayType = ParseTypeFromSrc("VArray<Int64, $3>");
    ASSERT_NE(varrayType, nullptr) << "Failed to parse type: VArray<Int64, $3>";
    ASSERT_TRUE(Is<VArrayType>(varrayType.get()));
    auto parsedVArrayType = As<ASTKind::VARRAY_TYPE>(varrayType.get());
    ASSERT_NE(parsedVArrayType->constantType, nullptr);
    ASSERT_TRUE(Is<ConstantType>(parsedVArrayType->constantType.get()));
    EXPECT_EQ("$3", parsedVArrayType->constantType->ToString());
}

TEST(ToStringTest, PrimitiveTypeToStringPrefersSourceSpelling)
{
    auto primitiveType = MakeOwned<PrimitiveType>();
    primitiveType->str = "Int32";
    primitiveType->kind = TypeKind::TYPE_INVALID;

    EXPECT_EQ("Int32", primitiveType->ToString());
}

TEST(ToStringTest, TypeAndOptionalExprRestore)
{
    ExpectExprToStringAndKind<ParenExpr>("(1)");
    ExpectExprToStringAndKind<AsExpr>("1 as Int64");
    ExpectExprToStringAndKind<IsExpr>("1 is Int64");
    ExpectExprToStringAndKind<TypeConvExpr>("Int64(1)");
    ExpectExprToStringAndKind<OptionalChainExpr>("a?.b");

    auto optionalExprChain = ParseExprFromSrc("x?.b?[0]");
    ASSERT_NE(optionalExprChain, nullptr) << "Failed to parse expr: x?.b?[0]";
    ASSERT_TRUE(Is<OptionalChainExpr>(optionalExprChain.get()));
    auto optionalChainExpr = As<ASTKind::OPTIONAL_CHAIN_EXPR>(optionalExprChain.get());
    ASSERT_NE(optionalChainExpr->expr, nullptr);
    ASSERT_TRUE(Is<SubscriptExpr>(optionalChainExpr->expr.get()));
    auto subscriptExpr = As<ASTKind::SUBSCRIPT_EXPR>(optionalChainExpr->expr.get());
    ASSERT_NE(subscriptExpr->baseExpr, nullptr);
    ASSERT_TRUE(Is<OptionalExpr>(subscriptExpr->baseExpr.get()));
    EXPECT_EQ("x?.b?", subscriptExpr->baseExpr->ToString());

    auto primitiveTypeExprCall = ParseExprFromSrc("Int64.foo()");
    ASSERT_NE(primitiveTypeExprCall, nullptr) << "Failed to parse expr: Int64.foo()";
    ASSERT_TRUE(Is<CallExpr>(primitiveTypeExprCall.get()));
    auto callExpr = As<ASTKind::CALL_EXPR>(primitiveTypeExprCall.get());
    ASSERT_NE(callExpr->baseFunc, nullptr);
    ASSERT_TRUE(Is<MemberAccess>(callExpr->baseFunc.get()));
    auto memberAccess = As<ASTKind::MEMBER_ACCESS>(callExpr->baseFunc.get());
    ASSERT_NE(memberAccess->baseExpr, nullptr);
    ASSERT_TRUE(Is<PrimitiveTypeExpr>(memberAccess->baseExpr.get()));
    EXPECT_EQ("Int64", memberAccess->baseExpr->ToString());
    EXPECT_EQ("Int64.foo()", primitiveTypeExprCall->ToString());
}

TEST(ToStringTest, PatternLeafAndLetPatternRestore)
{
    auto wildcardPattern = MakeOwned<WildcardPattern>();
    EXPECT_EQ("_", wildcardPattern->ToString());

    auto constPattern = MakeOwned<ConstPattern>();
    constPattern->literal = MakeOwned<LitConstExpr>(LitConstKind::INTEGER, "1");
    EXPECT_EQ("1", constPattern->ToString());

    auto varPattern = MakeOwned<VarPattern>(SrcIdentifier{"value"});
    EXPECT_EQ("value", varPattern->ToString());

    auto ifLetExpr = ParseExprFromSrc("if(let value <- x) {}");
    ASSERT_NE(ifLetExpr, nullptr) << "Failed to parse expr: if(let value <- x) {}";
    ASSERT_TRUE(Is<IfExpr>(ifLetExpr.get()));
    auto ifExpr = As<ASTKind::IF_EXPR>(ifLetExpr.get());
    ASSERT_NE(ifExpr->condExpr, nullptr);
    ASSERT_TRUE(Is<LetPatternDestructor>(ifExpr->condExpr.get()));
    auto letPattern = As<ASTKind::LET_PATTERN_DESTRUCTOR>(ifExpr->condExpr.get());
    ASSERT_EQ(letPattern->patterns.size(), 1);
    ASSERT_NE(letPattern->patterns[0], nullptr);
    ASSERT_TRUE(Is<VarPattern>(letPattern->patterns[0].get()) || Is<VarOrEnumPattern>(letPattern->patterns[0].get()));
    EXPECT_EQ("value", letPattern->patterns[0]->ToString());
    EXPECT_EQ("let value <- x", letPattern->ToString());
}

namespace {
constexpr size_t kTuplePatternElementCount = 2;
constexpr size_t kCatchTypeCount = 2;

void VerifyTuplePatternRestore()
{
    auto tupleExpr = ParseExprFromSrc("match (x) { case (Color.Red, _) => 1 }");
    ASSERT_NE(tupleExpr, nullptr) << "Failed to parse expr: match (x) { case (Color.Red, _) => 1 }";
    ASSERT_TRUE(Is<MatchExpr>(tupleExpr.get()));
    auto tupleMatchExpr = As<ASTKind::MATCH_EXPR>(tupleExpr.get());
    ASSERT_FALSE(tupleMatchExpr->matchCases.empty());
    ASSERT_EQ(tupleMatchExpr->matchCases[0]->patterns.size(), 1);
    ASSERT_TRUE(Is<TuplePattern>(tupleMatchExpr->matchCases[0]->patterns[0].get()));
    auto tuplePattern = As<ASTKind::TUPLE_PATTERN>(tupleMatchExpr->matchCases[0]->patterns[0].get());
    ASSERT_EQ(tuplePattern->patterns.size(), kTuplePatternElementCount);
    ASSERT_NE(tuplePattern->patterns[0], nullptr);
    ASSERT_NE(tuplePattern->patterns[1], nullptr);
    ASSERT_TRUE(Is<EnumPattern>(tuplePattern->patterns[0].get()));
    ASSERT_TRUE(Is<WildcardPattern>(tuplePattern->patterns[1].get()));
    EXPECT_EQ("Color.Red", tuplePattern->patterns[0]->ToString());
    EXPECT_EQ("_", tuplePattern->patterns[1]->ToString());
    EXPECT_EQ("(Color.Red, _)", tuplePattern->ToString());
}

void VerifyTypePatternRestore()
{
    auto typeExpr = ParseExprFromSrc("match (x) { case y: Int64 => 1 }");
    ASSERT_NE(typeExpr, nullptr) << "Failed to parse expr: match (x) { case y: Int64 => 1 }";
    ASSERT_TRUE(Is<MatchExpr>(typeExpr.get()));
    auto typeMatchExpr = As<ASTKind::MATCH_EXPR>(typeExpr.get());
    ASSERT_FALSE(typeMatchExpr->matchCases.empty());
    ASSERT_EQ(typeMatchExpr->matchCases[0]->patterns.size(), 1);
    ASSERT_TRUE(Is<TypePattern>(typeMatchExpr->matchCases[0]->patterns[0].get()));
    auto typePattern = As<ASTKind::TYPE_PATTERN>(typeMatchExpr->matchCases[0]->patterns[0].get());
    ASSERT_NE(typePattern->pattern, nullptr);
    ASSERT_NE(typePattern->type, nullptr);
    ASSERT_TRUE(Is<VarPattern>(typePattern->pattern.get()));
    EXPECT_EQ("y", typePattern->pattern->ToString());
    EXPECT_EQ("Int64", typePattern->type->ToString());
    EXPECT_EQ("y: Int64", typePattern->ToString());
}

void VerifyEnumPatternRestore()
{
    auto enumExpr = ParseExprFromSrc("match (x) { case Year(y) => 1 }");
    ASSERT_NE(enumExpr, nullptr) << "Failed to parse expr: match (x) { case Year(y) => 1 }";
    ASSERT_TRUE(Is<MatchExpr>(enumExpr.get()));
    auto enumMatchExpr = As<ASTKind::MATCH_EXPR>(enumExpr.get());
    ASSERT_FALSE(enumMatchExpr->matchCases.empty());
    ASSERT_EQ(enumMatchExpr->matchCases[0]->patterns.size(), 1);
    ASSERT_TRUE(Is<EnumPattern>(enumMatchExpr->matchCases[0]->patterns[0].get()));
    auto enumPattern = As<ASTKind::ENUM_PATTERN>(enumMatchExpr->matchCases[0]->patterns[0].get());
    ASSERT_NE(enumPattern->constructor, nullptr);
    ASSERT_EQ(enumPattern->patterns.size(), 1);
    EXPECT_EQ("Year(y)", enumPattern->ToString());
}

void VerifyVarOrEnumPatternRestore()
{
    auto varOrEnumExpr = ParseExprFromSrc("match (x) { case who => 1 }");
    ASSERT_NE(varOrEnumExpr, nullptr) << "Failed to parse expr: match (x) { case who => 1 }";
    ASSERT_TRUE(Is<MatchExpr>(varOrEnumExpr.get()));
    auto varOrEnumMatchExpr = As<ASTKind::MATCH_EXPR>(varOrEnumExpr.get());
    ASSERT_FALSE(varOrEnumMatchExpr->matchCases.empty());
    ASSERT_EQ(varOrEnumMatchExpr->matchCases[0]->patterns.size(), 1);
    ASSERT_TRUE(Is<VarOrEnumPattern>(varOrEnumMatchExpr->matchCases[0]->patterns[0].get()));
    auto varOrEnumPattern = As<ASTKind::VAR_OR_ENUM_PATTERN>(varOrEnumMatchExpr->matchCases[0]->patterns[0].get());
    EXPECT_EQ("who", varOrEnumPattern->ToString());
}

void VerifyExceptTypePatternRestore()
{
    auto tryExpr = ParseExprFromSrc("try {} catch(e: Exception1 | Exception2) {}");
    ASSERT_NE(tryExpr, nullptr) << "Failed to parse expr: try {} catch(e: Exception1 | Exception2) {}";
    ASSERT_TRUE(Is<TryExpr>(tryExpr.get()));
    auto parsedTryExpr = As<ASTKind::TRY_EXPR>(tryExpr.get());
    ASSERT_EQ(parsedTryExpr->catchPatterns.size(), 1);
    ASSERT_NE(parsedTryExpr->catchPatterns[0], nullptr);
    ASSERT_TRUE(Is<ExceptTypePattern>(parsedTryExpr->catchPatterns[0].get()));
    auto catchPattern = As<ASTKind::EXCEPT_TYPE_PATTERN>(parsedTryExpr->catchPatterns[0].get());
    ASSERT_NE(catchPattern->pattern, nullptr);
    ASSERT_TRUE(Is<VarPattern>(catchPattern->pattern.get()));
    ASSERT_EQ(catchPattern->types.size(), kCatchTypeCount);
    EXPECT_EQ("e: Exception1 | Exception2", catchPattern->ToString());
}

void VerifyCommandTypePatternRestore()
{
    auto commandPattern = MakeOwned<CommandTypePattern>();
    commandPattern->pattern = MakeOwned<VarPattern>(SrcIdentifier{"effect"});
    commandPattern->types.emplace_back(ParseTypeFromSrc("Effect1"));
    commandPattern->types.emplace_back(ParseTypeFromSrc("Effect2"));
    EXPECT_EQ("effect: Effect1 | Effect2", commandPattern->ToString());
}
} // namespace

TEST(ToStringTest, PatternCompositeRestore)
{
    VerifyTuplePatternRestore();
    VerifyTypePatternRestore();
    VerifyEnumPatternRestore();
    VerifyVarOrEnumPatternRestore();
    VerifyExceptTypePatternRestore();
    VerifyCommandTypePatternRestore();
}
