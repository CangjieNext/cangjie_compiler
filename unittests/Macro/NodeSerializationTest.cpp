// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <cstdlib>
#include <string>
#include "gtest/gtest.h"
#include "cangjie/Macro/NodeSerialization.h"
#include "cangjie/AST/Clone.h"
#include "cangjie/Basic/DiagnosticEngine.h"
#include "cangjie/Frontend/CompilerInstance.h"
#include "cangjie/Parse/Parser.h"

using namespace Cangjie;
using namespace AST;
using namespace NodeSerialization;

class NodeSerializationTest : public testing::Test {
protected:
    void SetUp() override
    {
        binaryExpr = R"(2 * (3 + 4))";
        unaryExpr = R"(!2)";
        varDecl = R"(var a = 2 + 3)";
        funcDecl = R"(
            func MyComponent(aa: Int32, bb: Int32) : Int64 {
                // aa, bb has no meaning at all, just for testing
                var counter = @M(0) // MacroExpandExpr
                MyFoo()
                let b: Int64 = 3 * (2 + 4 - 2)
                MyBar(1, 2) // CallExpr with args
                var c : Int64
                c = b + 2020 // AssignExpr
                let d: VArray<Int64, $5> = VArray<Int64, $5>({i => i}) // VArrayType and ConstantType and ArrayExpr
                var e: (Int32) = 1 // ParenType
                let f = quote(a == b) // QuoteExpr
                return c // ReturnExpr
            }
        )";
        structDecl = R"(
            @Differentiable[except: [in_channels_, out_channels_, has_bias_, activation_]]
            public struct Dense<T> where T <: Evaluable { // GenericConstraint
              var in_channels_: Int32
              var out_channels_: Int32
              var has_bias_: Bool

              var activation_: ActivationType
              var weight_: Tensor
              var bias_: Tensor
            }
        )";

        classDecl = R"(
            class Data <: unittest.TestCases { // QualifiedType
                var a : Int32
                var b : Float32
                var c : Int32 = denseObj.in_channels_ // MemberAccess : RefExpr.field
                public func get(a: () -> Unit) : Int32 { // FuncType
                    synchronized(m) { foo() } // SynchronizedExpr
                    return 1
                }
                @M var d : Float32 // MacroExpandDecl
                type Class1<V> = GenericClassA<Int64, V> // TypeAliasDecl
                func f(): This { // ThisType
                    this
                }
            }
        )";
        interfaceDecl = R"(
            interface MyInterface {
                func foo() {}
            }
        )";
        ifExpr = R"(
            if (a) { // a > 0
                return 1
            } else {
                var a = 2021
                var b = a + 1
                let x = Int64.foo // PrimitiveTypeExpr
                return -1
            }
        )";
        lambdaExpr = R"(
            {a: Int32, b: Int32 => a + b}
        )";
    }

    std::string binaryExpr;
    std::string unaryExpr;
    std::string varDecl;
    std::string funcDecl;
    std::string structDecl;
    std::string classDecl;
    std::string interfaceDecl;
    std::string ifExpr;
    std::string lambdaExpr;
};

TEST_F(NodeSerializationTest, BinaryExpr_Serialization)
{
    SourceManager sm;
    sm.AddSource("./", binaryExpr);
    DiagnosticEngine diag;
    diag.SetSourceManager(&sm);
    Parser binaryExprParser{binaryExpr, diag, sm};
    // binaryExpr: 2 * (3 + 4)
    // litConstExpr and parenExpr are also tested
    // Check specific offset of BinaryExpr using flatc generated methods.
    auto ptr = binaryExprParser.ParseExpr();
    NodeWriter binaryExprWriter(ptr.get());
    uint8_t* rawBuffer = binaryExprWriter.ExportNode();
    ASSERT_NE(rawBuffer, nullptr);
    uint8_t* buffer = rawBuffer + 4; // move the pointer to the real position of the flatbuffer
    auto fbNode = ASTFormat::GetNode(buffer);
    EXPECT_EQ(fbNode->root_type(), ASTFormat::AnyNode_EXPR);
    auto fbExpr = fbNode->root_as_EXPR();
    EXPECT_EQ(fbExpr->expr_type(), ASTFormat::AnyExpr_BINARY_EXPR);
    auto fbBianryExpr = fbExpr->expr_as_BINARY_EXPR();
    auto leftExpr = fbBianryExpr->left_expr();
    EXPECT_EQ(leftExpr->expr_type(), ASTFormat::AnyExpr_LIT_CONST_EXPR);
    auto leftExprStr = fbBianryExpr->left_expr()->expr_as_LIT_CONST_EXPR()->literal();
    EXPECT_EQ(leftExprStr->str(), "2");
    auto rightExpr = fbBianryExpr->right_expr();
    EXPECT_EQ(rightExpr->expr_type(), ASTFormat::AnyExpr_PAREN_EXPR);
    auto onlyExpr = rightExpr->expr_as_PAREN_EXPR();
    EXPECT_EQ(onlyExpr->expr()->expr_type(), ASTFormat::AnyExpr_BINARY_EXPR);
    auto onlyExprAsBinary = onlyExpr->expr()->expr_as_BINARY_EXPR();
    auto parenLeftExprStr = onlyExprAsBinary->left_expr()->expr_as_LIT_CONST_EXPR()->literal();
    auto parenRightExprStr = onlyExprAsBinary->right_expr()->expr_as_LIT_CONST_EXPR()->literal();
    EXPECT_EQ(parenLeftExprStr->str(), "3");
    EXPECT_EQ(parenRightExprStr->str(), "4");
    free(rawBuffer);
}

TEST_F(NodeSerializationTest, UnaryExpr_Serialization)
{
    SourceManager sm;
    sm.AddSource("./", unaryExpr);
    DiagnosticEngine diag;
    diag.SetSourceManager(&sm);
    Parser unaryExprParser{unaryExpr, diag, sm};
    // unaryExpr: !2
    auto ptr = unaryExprParser.ParseExpr();
    NodeWriter unaryExprWriter(ptr.get());
    uint8_t* rawBuffer = unaryExprWriter.ExportNode();
    ASSERT_NE(rawBuffer, nullptr);
    uint8_t* buffer = rawBuffer + 4; // move the pointer to the real position of the flatbuffer
    auto fbNode = ASTFormat::GetNode(buffer);
    EXPECT_EQ(fbNode->root_type(), ASTFormat::AnyNode_EXPR);
    EXPECT_EQ(fbNode->root_as_EXPR()->expr_type(), ASTFormat::AnyExpr_UNARY_EXPR);
    auto fbUnaryExpr = fbNode->root_as_EXPR()->expr_as_UNARY_EXPR();
    auto onlyExpr = fbUnaryExpr->expr()->expr_as_LIT_CONST_EXPR()->literal();
    EXPECT_EQ(onlyExpr->str(), "2");
    free(rawBuffer);
}

TEST_F(NodeSerializationTest, VarDecl_Serialization)
{
    SourceManager sm;
    sm.AddSource("./", varDecl);
    DiagnosticEngine diag;
    diag.SetSourceManager(&sm);
    Parser varDeclParser{varDecl, diag, sm};
    // varDecl: var a = 2 + 3
    auto ptr = varDeclParser.ParseDecl(ScopeKind::FUNC_BODY);
    NodeWriter varDeclWriter(ptr.get());
    uint8_t* rawBuffer = varDeclWriter.ExportNode();
    ASSERT_NE(rawBuffer, nullptr);
    uint8_t* buffer = rawBuffer + 4; // move the pointer to the real position of the flatbuffer
    auto fbNode = ASTFormat::GetNode(buffer);
    EXPECT_EQ(fbNode->root_type(), ASTFormat::AnyNode_DECL);
    EXPECT_EQ(fbNode->root_as_DECL()->decl_type(), ASTFormat::AnyDecl_VAR_DECL);
    auto fbVarDecl = fbNode->root_as_DECL()->decl_as_VAR_DECL();
    EXPECT_EQ(fbVarDecl->is_var(), true);
    EXPECT_EQ(fbVarDecl->base()->identifier()->str(), "a");
    EXPECT_EQ(fbVarDecl->initializer()->expr_type(), ASTFormat::AnyExpr_BINARY_EXPR);
    free(rawBuffer);
}

TEST_F(NodeSerializationTest, FuncDecl_Serialization)
{
    SourceManager sm;
    sm.AddSource("./", funcDecl);
    DiagnosticEngine diag;
    diag.SetSourceManager(&sm);
    Parser funcDeclParser{funcDecl, diag, sm};
    auto ptr = funcDeclParser.ParseDecl(ScopeKind::TOPLEVEL);
    NodeWriter funcDeclWriter(ptr.get());
    uint8_t* rawBuffer = funcDeclWriter.ExportNode();
    ASSERT_NE(rawBuffer, nullptr);
    uint8_t* buffer = rawBuffer + 4; // move the pointer to the real position of the flatbuffer
    auto fbNode = ASTFormat::GetNode(buffer);
    EXPECT_EQ(fbNode->root_type(), ASTFormat::AnyNode_DECL);
    EXPECT_EQ(fbNode->root_as_DECL()->decl_type(), ASTFormat::AnyDecl_FUNC_DECL);
    auto fbFuncDecl = fbNode->root_as_DECL()->decl_as_FUNC_DECL();
    EXPECT_EQ(fbFuncDecl->base()->identifier()->str(), "MyComponent");
    auto fbFuncBody = fbFuncDecl->func_body();
    std::vector<std::string> expectIdVec{"aa", "bb"};
    std::vector<std::string> realIdVec;
    auto fbParams = fbFuncBody->param_list()->params(); // flatbuffers::Vector
    for (size_t i = 0; i < fbParams->size(); ++i) {
        // Test parameters
        auto fbParam = fbParams->Get(i);
        auto paramId = fbParam->base()->base()->identifier()->str();
        realIdVec.push_back(paramId);
    }
    EXPECT_EQ(realIdVec, expectIdVec);
    auto fbFuncBlock = fbFuncBody->body()->body();
    std::vector<ASTFormat::AnyNode> expectEnumVec{ASTFormat::AnyNode_DECL, ASTFormat::AnyNode_EXPR,
        ASTFormat::AnyNode_DECL, ASTFormat::AnyNode_EXPR, ASTFormat::AnyNode_DECL, ASTFormat::AnyNode_EXPR,
        ASTFormat::AnyNode_DECL, ASTFormat::AnyNode_DECL, ASTFormat::AnyNode_DECL, ASTFormat::AnyNode_EXPR};
    std::vector<ASTFormat::AnyNode> realEnumVec;
    for (size_t i = 0; i < fbFuncBlock->size(); ++i) {
        auto fbBlockNode = fbFuncBlock->Get(i);
        auto fbNodeType = fbBlockNode->root_type();
        realEnumVec.push_back(fbNodeType);
    }
    EXPECT_EQ(expectEnumVec, realEnumVec);
    free(rawBuffer);
}

TEST_F(NodeSerializationTest, ClassDecl_Serialization)
{
    SourceManager sm;
    sm.AddSource("./", classDecl);
    DiagnosticEngine diag;
    diag.SetSourceManager(&sm);
    Parser classDeclParser{classDecl, diag, sm};
    auto ptr = classDeclParser.ParseDecl(ScopeKind::TOPLEVEL);
    NodeWriter classDeclWriter(ptr.get());
    uint8_t* rawBuffer = classDeclWriter.ExportNode();
    ASSERT_NE(rawBuffer, nullptr);
    uint8_t* buffer = rawBuffer + 4; // point to the real pos of buffer
    auto fbNode = ASTFormat::GetNode(buffer);
    EXPECT_EQ(fbNode->root_type(), ASTFormat::AnyNode_DECL);
    EXPECT_EQ(fbNode->root_as_DECL()->decl_type(), ASTFormat::AnyDecl_CLASS_DECL);
    // test class name
    auto fbClassDecl = fbNode->root_as_DECL()->decl_as_CLASS_DECL();
    EXPECT_EQ(fbClassDecl->base()->identifier()->str(), "Data");
    // test class body
    auto fbClassBody = fbClassDecl->body()->decls();
    std::vector<ASTFormat::AnyDecl> expectDeclType{ASTFormat::AnyDecl_VAR_DECL, ASTFormat::AnyDecl_VAR_DECL,
        ASTFormat::AnyDecl_VAR_DECL, ASTFormat::AnyDecl_FUNC_DECL, ASTFormat::AnyDecl_MACRO_EXPAND_DECL,
        ASTFormat::AnyDecl_TYPE_ALIAS_DECL, ASTFormat::AnyDecl_FUNC_DECL};
    std::vector<ASTFormat::AnyDecl> realDeclType;
    std::vector<std::string> expectDeclId{"a", "b", "c", "get", "M", "", "f"};
    std::vector<std::string> realDeclId;
    for (size_t i = 0; i < fbClassBody->size(); i++) {
        auto fbDecl = fbClassBody->Get(i);
        auto declType = fbDecl->decl_type();
        realDeclType.push_back(declType);
        std::string id;
        if (declType == ASTFormat::AnyDecl_VAR_DECL) {
            auto fbVarDecl = fbDecl->decl_as_VAR_DECL();
            id = fbVarDecl->base()->identifier()->str();
        }
        if (declType == ASTFormat::AnyDecl_FUNC_DECL) {
            auto fbFuncDecl = fbDecl->decl_as_FUNC_DECL();
            id = fbFuncDecl->base()->identifier()->str();
        }
        if (declType == ASTFormat::AnyDecl_MACRO_EXPAND_DECL) {
            auto fbFuncDecl = fbDecl->decl_as_MACRO_EXPAND_DECL();
            id = fbFuncDecl->base()->identifier()->str();
        }
        realDeclId.push_back(id);
    }
    EXPECT_EQ(realDeclType, expectDeclType);
    EXPECT_EQ(realDeclId, expectDeclId);
    auto VarMemAcc = fbClassBody->Get(2)->decl_as_VAR_DECL();
    auto initExpr = VarMemAcc->initializer();
    EXPECT_EQ(initExpr->expr_type(), ASTFormat::AnyExpr_MEMBER_ACCESS);
    auto memAccExpr = initExpr->expr_as_MEMBER_ACCESS();
    auto firstPart = memAccExpr->base_expr()->expr_as_REF_EXPR()->ref()->identifier()->str();
    auto secondPart = memAccExpr->field()->str();
    EXPECT_EQ(firstPart, "denseObj");
    EXPECT_EQ(secondPart, "in_channels_");
    // test superClassType, interfaceType, sub_decls
    // test generic
    free(rawBuffer);
}

TEST_F(NodeSerializationTest, InterfaceDecl_Serialization)
{
    SourceManager sm;
    sm.AddSource("./", interfaceDecl);
    DiagnosticEngine diag;
    diag.SetSourceManager(&sm);
    Parser interfaceDeclParser{interfaceDecl, diag, sm};
    auto ptr = interfaceDeclParser.ParseDecl(ScopeKind::TOPLEVEL);
    NodeWriter interfaceDeclWriter(ptr.get());
    uint8_t* rawBuffer = interfaceDeclWriter.ExportNode();
    ASSERT_NE(rawBuffer, nullptr);
    uint8_t* buffer = rawBuffer + 4; // point to the real pos of buffer
    auto fbNode = ASTFormat::GetNode(buffer);
    EXPECT_EQ(fbNode->root_type(), ASTFormat::AnyNode_DECL);
    EXPECT_EQ(fbNode->root_as_DECL()->decl_type(), ASTFormat::AnyDecl_INTERFACE_DECL);
    // test interface name
    auto fbInterfaceDecl = fbNode->root_as_DECL()->decl_as_INTERFACE_DECL();
    EXPECT_EQ(fbInterfaceDecl->base()->identifier()->str(), "MyInterface");
    free(rawBuffer);
}

TEST_F(NodeSerializationTest, IfExpr_Serialization)
{
    SourceManager sm;
    sm.AddSource("./", ifExpr);
    DiagnosticEngine diag;
    diag.SetSourceManager(&sm);
    Parser ifExprParser{ifExpr, diag, sm};
    auto ptr = ifExprParser.ParseExpr();
    NodeWriter ifExprWriter(ptr.get());
    uint8_t* rawBuffer = ifExprWriter.ExportNode();
    ASSERT_NE(rawBuffer, nullptr);
    uint8_t* buffer = rawBuffer + 4; // move the pointer to the real position of the flatbuffer
    auto fbNode = ASTFormat::GetNode(buffer);
    EXPECT_EQ(fbNode->root_type(), ASTFormat::AnyNode_EXPR);
    EXPECT_EQ(fbNode->root_as_EXPR()->expr_type(), ASTFormat::AnyExpr_IF_EXPR);
    free(rawBuffer);
}

TEST_F(NodeSerializationTest, LambdaExpr_Serialization)
{
    SourceManager sm;
    sm.AddSource("./", lambdaExpr);
    DiagnosticEngine diag;
    diag.SetSourceManager(&sm);
    Parser lambdaExprParser{lambdaExpr, diag, sm};
    // {a: Int32, b: Int32 => a + b}
    // For lambaExpr, it's only part of the funcBody.
    // Some fields of FuncBody are set to default(null) in lambdaExpr
    auto ptr = lambdaExprParser.ParseExpr();
    NodeWriter lambdaWriter(ptr.get());
    uint8_t* rawBuffer = lambdaWriter.ExportNode();
    ASSERT_NE(rawBuffer, nullptr);
    uint8_t* buffer = rawBuffer + 4;
    auto fbNode = ASTFormat::GetNode(buffer);
    EXPECT_EQ(fbNode->root_type(), ASTFormat::AnyNode_EXPR);
    EXPECT_EQ(fbNode->root_as_EXPR()->expr_type(), ASTFormat::AnyExpr_LAMBDA_EXPR);

    // check parameter
    auto fbLambdaExpr = fbNode->root_as_EXPR()->expr_as_LAMBDA_EXPR();
    auto fbLambdaBody = fbLambdaExpr->body();
    auto lbdParams = fbLambdaBody->param_list()->params();
    std::vector<std::string> expectParamVec{"a", "b"};
    std::vector<std::string> realParamVec{};
    for (size_t i = 0; i < lbdParams->size(); i++) {
        auto fbParam = lbdParams->Get(i);
        auto paramId = fbParam->base()->base()->identifier()->str();
        realParamVec.push_back(paramId);
    }
    EXPECT_EQ(expectParamVec, realParamVec);

    // check => pos
    auto arrowPos = fbLambdaBody->arrow_pos();
    auto line = arrowPos->line();
    auto column = arrowPos->column();
    EXPECT_NE(line, 0);
    EXPECT_NE(column, 0);

    // check lambda body
    auto lbdBlock = fbLambdaBody->body()->body(); // flatbuffers::Vector<flatbuffers::Offset<Node>>
    std::vector<ASTFormat::AnyNode> expectNodeType{ASTFormat::AnyNode_EXPR};
    std::vector<ASTFormat::AnyNode> realNodeType{};
    for (size_t i = 0; i < lbdBlock->size(); i++) {
        auto fbNode = lbdBlock->Get(i);
        realNodeType.push_back(fbNode->root_type());
    }
    EXPECT_EQ(expectNodeType, realNodeType);
    free(rawBuffer);
}

// ---------------------------------------------------------------------------
// Macro import node tests (UT-A-01/02 in MacroImportDT.md §3.2, UT-S-01/02 in
// §3.3). Macro imports hang off PackageSpec (file->package->macroImports), so
// the NodeFormat round-trip serializes the File and reads back the package.
// ---------------------------------------------------------------------------

namespace {
/// Parse a `macro package` source and return the parsed File.
OwnedPtr<File> ParseMacroImportSource(const std::string& code)
{
    SourceManager sm;
    DiagnosticEngine diag;
    diag.SetSourceManager(&sm);
    Parser parser(code, diag, sm);
    return parser.ParseTopLevel();
}
} // namespace

// UT-A-01: CloneMacroImportList deep-copies items, @When annotations and
// positions; mutating the clone must not affect the source (independent ownership).
TEST_F(NodeSerializationTest, MacroImportListClone)
{
    auto file = ParseMacroImportSource(
        "macro package p\n"
        "macro {\n"
        "    import a.b.c\n"
        "    @When[os == \"Linux\"] import d.e.*\n"
        "}\n");
    ASSERT_NE(file->package, nullptr);
    const auto& src = file->package->macroImports;
    ASSERT_EQ(src.size(), 1U);
    const auto& list = *src.front();
    ASSERT_EQ(list.items.size(), 2U);

    auto cloned = ASTCloner::CloneMacroImportList(list);
    EXPECT_EQ(cloned->macroPos, list.macroPos);
    EXPECT_EQ(cloned->lcurlPos, list.lcurlPos);
    EXPECT_EQ(cloned->rcurlPos, list.rcurlPos);
    ASSERT_EQ(cloned->items.size(), list.items.size());
    for (size_t i = 0; i < list.items.size(); ++i) {
        EXPECT_EQ(cloned->items[i]->importPos, list.items[i]->importPos);
        EXPECT_EQ(cloned->items[i]->content.kind, list.items[i]->content.kind);
        EXPECT_EQ(cloned->items[i]->ToString(), list.items[i]->ToString());
    }
    // The @When annotation survives with its condExpr (BINARY_EXPR for os == "Linux").
    ASSERT_EQ(cloned->items[1]->annotations.size(), 1U);
    ASSERT_NE(cloned->items[1]->annotations.front()->condExpr.get(), nullptr);
    EXPECT_EQ(cloned->items[1]->annotations.front()->condExpr->astKind, ASTKind::BINARY_EXPR);
    // Ownership is independent: mutating the clone leaves the source untouched.
    cloned->items.clear();
    cloned->lcurlPos = INVALID_POSITION;
    EXPECT_EQ(list.items.size(), 2U);
    EXPECT_NE(list.lcurlPos, INVALID_POSITION);
}

// UT-A-02: ToString covers single form, block form (one statement per line) and
// the degenerate empty single form.
TEST_F(NodeSerializationTest, MacroImportListToString)
{
    auto single = ParseMacroImportSource("macro package p\nmacro import a.b.c as d\n");
    ASSERT_EQ(single->package->macroImports.size(), 1U);
    EXPECT_EQ(single->package->macroImports.front()->ToString(), "macro import a.b.c as d");

    auto block = ParseMacroImportSource(
        "macro package p\n"
        "macro {\n"
        "    import a.b.c\n"
        "    import d.e.*\n"
        "}\n");
    ASSERT_EQ(block->package->macroImports.size(), 1U);
    EXPECT_EQ(block->package->macroImports.front()->ToString(),
        "{\nmacro import a.b.c\nmacro import d.e.*\n}");

    MacroImportList emptyList;
    EXPECT_EQ(emptyList.ToString(), "macro import");
}

// UT-S-01: MacroImportSpec NodeFormat round-trip — content kind, prefix paths,
// identifier/alias, per-item @When annotations and importPos all survive.
TEST_F(NodeSerializationTest, MacroImportSpec_Serialization)
{
    auto file = ParseMacroImportSource(
        "macro package p\n"
        "macro {\n"
        "    import a.b.c\n"
        "    import d.e.f as g\n"
        "    import h.i.*\n"
        "    import j.k.{m, n}\n"
        "    @When[os == \"Linux\"] import x.y\n"
        "}\n");
    NodeWriter writer(file.get());
    uint8_t* rawBuffer = writer.ExportNode();
    ASSERT_NE(rawBuffer, nullptr);
    uint8_t* buffer = rawBuffer + 4; // move the pointer to the real position of the flatbuffer
    auto fbNode = ASTFormat::GetNode(buffer);
    ASSERT_EQ(fbNode->root_type(), ASTFormat::AnyNode_FILE);
    auto fbItems = fbNode->root_as_FILE()->package()->macroImports()->Get(0)->items();
    ASSERT_EQ(fbItems->size(), 5U);

    // single: a.b.c
    auto single = fbItems->Get(0)->content();
    EXPECT_EQ(single->kind(), ASTFormat::ImportKind_IMPORT_SINGLE);
    ASSERT_EQ(single->prefixPaths()->size(), 2U);
    EXPECT_EQ(single->prefixPaths()->Get(0)->str(), "a");
    EXPECT_EQ(single->prefixPaths()->Get(1)->str(), "b");
    EXPECT_EQ(single->identifier()->str(), "c");
    EXPECT_EQ(single->asIdentifier()->str(), "");
    EXPECT_EQ(fbItems->Get(0)->annotations()->size(), 0U);
    // importPos on source line 3.
    EXPECT_EQ(fbItems->Get(0)->importPos()->line(), 3U);

    // alias: d.e.f as g
    auto alias = fbItems->Get(1)->content();
    EXPECT_EQ(alias->kind(), ASTFormat::ImportKind_IMPORT_ALIAS);
    EXPECT_EQ(alias->identifier()->str(), "f");
    EXPECT_EQ(alias->asIdentifier()->str(), "g");

    // all: h.i.* — the star lands in identifier, prefix holds h.i
    auto all = fbItems->Get(2)->content();
    EXPECT_EQ(all->kind(), ASTFormat::ImportKind_IMPORT_ALL);
    ASSERT_EQ(all->prefixPaths()->size(), 2U);
    EXPECT_EQ(all->identifier()->str(), "*");

    // multi: j.k.{m, n}
    auto multi = fbItems->Get(3)->content();
    EXPECT_EQ(multi->kind(), ASTFormat::ImportKind_IMPORT_MULTI);
    ASSERT_EQ(multi->items()->size(), 2U);
    EXPECT_EQ(multi->items()->Get(0)->identifier()->str(), "m");
    EXPECT_EQ(multi->items()->Get(1)->identifier()->str(), "n");

    // @When item: annotation with a condExpr survives the round-trip.
    auto whenAnno = fbItems->Get(4)->annotations()->Get(0);
    EXPECT_EQ(whenAnno->identifier()->str(), "When");
    ASSERT_NE(whenAnno->condExpr(), nullptr);
    EXPECT_EQ(whenAnno->condExpr()->expr_type(), ASTFormat::AnyExpr_BINARY_EXPR);
    free(rawBuffer);
}

// UT-S-02: MacroImportList NodeFormat round-trip — macroPos/braces for the
// block form; the single form keeps default (INVALID) brace positions.
TEST_F(NodeSerializationTest, MacroImportList_Serialization)
{
    // Block form: brace positions round-trip.
    auto block = ParseMacroImportSource(
        "macro package p\n"
        "macro {\n"
        "    import a.b.c\n"
        "}\n");
    NodeWriter blockWriter(block.get());
    uint8_t* rawBuffer = blockWriter.ExportNode();
    ASSERT_NE(rawBuffer, nullptr);
    uint8_t* buffer = rawBuffer + 4;
    auto fbBlock = ASTFormat::GetNode(buffer)->root_as_FILE()->package()->macroImports()->Get(0);
    EXPECT_EQ(fbBlock->macroPos()->line(), 2U);
    EXPECT_EQ(fbBlock->lcurlPos()->line(), 2U);
    EXPECT_NE(fbBlock->lcurlPos()->column(), 0U);
    EXPECT_EQ(fbBlock->rcurlPos()->line(), 4U);
    ASSERT_EQ(fbBlock->items()->size(), 1U);
    free(rawBuffer);

    // Single form: lcurl/rcurl are default (INVALID_POSITION -> all zeros).
    auto single = ParseMacroImportSource("macro package p\nmacro import a.b.c\n");
    NodeWriter singleWriter(single.get());
    uint8_t* rawSingle = singleWriter.ExportNode();
    ASSERT_NE(rawSingle, nullptr);
    auto fbSingle = ASTFormat::GetNode(rawSingle + 4)->root_as_FILE()->package()->macroImports()->Get(0);
    EXPECT_EQ(fbSingle->lcurlPos()->line(), 0U);
    EXPECT_EQ(fbSingle->lcurlPos()->column(), 0U);
    EXPECT_EQ(fbSingle->rcurlPos()->line(), 0U);
    EXPECT_EQ(fbSingle->rcurlPos()->column(), 0U);
    ASSERT_EQ(fbSingle->items()->size(), 1U);
    free(rawSingle);
}
