// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <algorithm>
#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>

#include "cangjie/CHIR/Analysis/CallGraphAnalysis.h"
#include "cangjie/CHIR/IR/CHIRBuilder.h"
#include "cangjie/CHIR/IR/DebugLocation.h"
#include "cangjie/CHIR/IR/Expression/Terminator.h"
#include "cangjie/CHIR/IR/Package.h"
#include "cangjie/CHIR/IR/Type/CustomTypeDef.h"

using namespace Cangjie;
using namespace Cangjie::CHIR;

namespace {
Function* CreateEmptyFunc(CHIRBuilder& builder, const std::string& name)
{
    auto* unitTy = builder.GetUnitTy();
    auto* funcTy = builder.GetType<FuncType>(std::vector<Type*>{}, unitTy);
    auto* func = builder.CreateFunction(funcTy, name, name, "", "default");
    auto* body = builder.CreateBlockGroup(*func);
    func->InitBody(*body);
    auto* block = builder.CreateBlock(body);
    body->SetEntryBlock(block);
    block->AppendExpression(builder.CreateTerminator<Exit>(block));
    return func;
}

void AppendDirectCall(CHIRBuilder& builder, Block* block, Function* callee)
{
    auto* apply = builder.CreateExpression<Apply>(builder.GetUnitTy(), callee, FuncCallContext{}, block);
    block->AppendExpression(apply);
}

Function* CreateCaller(CHIRBuilder& builder, const std::string& name, const std::vector<Function*>& callees)
{
    auto* unitTy = builder.GetUnitTy();
    auto* funcTy = builder.GetType<FuncType>(std::vector<Type*>{}, unitTy);
    auto* func = builder.CreateFunction(funcTy, name, name, "", "default");
    auto* body = builder.CreateBlockGroup(*func);
    func->InitBody(*body);
    auto* block = builder.CreateBlock(body);
    body->SetEntryBlock(block);
    for (auto* callee : callees) {
        AppendDirectCall(builder, block, callee);
    }
    block->AppendExpression(builder.CreateTerminator<Exit>(block));
    return func;
}

// Function declared but without a body -> Apply should create DIRECT edge to exit.
Function* CreateDeclOnlyFunc(CHIRBuilder& builder, const std::string& name)
{
    auto* unitTy = builder.GetUnitTy();
    auto* funcTy = builder.GetType<FuncType>(std::vector<Type*>{}, unitTy);
    return builder.CreateFunction(funcTy, name, name, "", "default");
}

std::vector<std::string> CollectNonNullNames(const std::vector<Function*>& funcs)
{
    std::vector<std::string> names;
    for (auto* func : funcs) {
        if (func != nullptr) {
            names.emplace_back(func->GetSrcCodeIdentifier());
        }
    }
    return names;
}

size_t FindIndex(const std::vector<std::string>& names, const std::string& name)
{
    auto it = std::find(names.begin(), names.end(), name);
    EXPECT_NE(it, names.end()) << "function not found in SCC list: " << name;
    return static_cast<size_t>(std::distance(names.begin(), it));
}
} // namespace

class CallGraphAnalysisTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        nameMap = std::make_unique<std::unordered_map<unsigned int, std::string>>();
        cctx = std::make_unique<CHIRContext>(nameMap.get());
        builder = std::make_unique<CHIRBuilder>(*cctx);
        package = builder->CreatePackage("default");
    }

    std::unique_ptr<std::unordered_map<unsigned int, std::string>> nameMap;
    std::unique_ptr<CHIRContext> cctx;
    std::unique_ptr<CHIRBuilder> builder;
    Package* package{nullptr};
};

TEST_F(CallGraphAnalysisTest, DirectCallChainPostOrder)
{
    auto* c = CreateEmptyFunc(*builder, "c");
    auto* b = CreateCaller(*builder, "b", {c});
    auto* a = CreateCaller(*builder, "a", {b});
    (void)a;

    CallGraphAnalysis analysis(package);
    analysis.DoCallGraphAnalysis(true);

    auto names = CollectNonNullNames(analysis.postOrderSCCFunctionlist);
    ASSERT_EQ(FindIndex(names, "c") < FindIndex(names, "b"), true);
    ASSERT_EQ(FindIndex(names, "b") < FindIndex(names, "a"), true);
}

TEST_F(CallGraphAnalysisTest, MutualRecursionFormsOneSCC)
{
    auto* unitTy = builder->GetUnitTy();
    auto* funcTy = builder->GetType<FuncType>(std::vector<Type*>{}, unitTy);

    auto* a = builder->CreateFunction(funcTy, "a", "a", "", "default");
    auto* b = builder->CreateFunction(funcTy, "b", "b", "", "default");

    auto InitWithCallees = [this](Function* func, const std::vector<Function*>& callees) {
        auto* body = builder->CreateBlockGroup(*func);
        func->InitBody(*body);
        auto* block = builder->CreateBlock(body);
        body->SetEntryBlock(block);
        for (auto* callee : callees) {
            AppendDirectCall(*builder, block, callee);
        }
        block->AppendExpression(builder->CreateTerminator<Exit>(block));
    };
    InitWithCallees(a, {b});
    InitWithCallees(b, {a});
    auto* r = CreateCaller(*builder, "r", {a});
    (void)r;

    CallGraphAnalysis analysis(package);
    analysis.DoCallGraphAnalysis(true);

    auto names = CollectNonNullNames(analysis.postOrderSCCFunctionlist);
    auto idxA = FindIndex(names, "a");
    auto idxB = FindIndex(names, "b");
    auto idxR = FindIndex(names, "r");
    EXPECT_EQ((idxA > idxB ? idxA - idxB : idxB - idxA), 1u);
    EXPECT_LT(std::max(idxA, idxB), idxR);
}

TEST_F(CallGraphAnalysisTest, IndependentRootsReachableFromEntry)
{
    auto* leaf1 = CreateEmptyFunc(*builder, "leaf1");
    auto* leaf2 = CreateEmptyFunc(*builder, "leaf2");
    (void)leaf1;
    (void)leaf2;

    CallGraphAnalysis analysis(package);
    analysis.DoCallGraphAnalysis(false);

    auto names = CollectNonNullNames(analysis.postOrderSCCFunctionlist);
    EXPECT_NE(std::find(names.begin(), names.end(), "leaf1"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "leaf2"), names.end());
}

TEST_F(CallGraphAnalysisTest, ApplyToDeclWithoutBodyGoesToExit)
{
    auto* external = CreateDeclOnlyFunc(*builder, "external");
    auto* caller = CreateCaller(*builder, "caller", {external});
    (void)caller;

    CallGraphAnalysis analysis(package);
    analysis.DoCallGraphAnalysis(true);

    auto names = CollectNonNullNames(analysis.postOrderSCCFunctionlist);
    EXPECT_NE(std::find(names.begin(), names.end(), "caller"), names.end());
}

TEST_F(CallGraphAnalysisTest, InvokeOnClassTypeEntersVirtualEdgePath)
{
    // GetAllPossibleCalleeOfInvoke is currently a stub (always empty), but Invoke on a
    // Class-typed object still exercises AddVirtualEdgeToNode up to the empty-callee loop.
    auto* classDef = builder->CreateClass(INVALID_LOCATION, "C", "C", "default", true, false);
    auto* classTy = builder->GetType<ClassType>(classDef);
    classDef->SetType(*classTy);
    auto* classRefTy = builder->GetType<RefType>(classTy);

    auto* methodTy =
        builder->GetType<FuncType>(std::vector<Type*>{classRefTy, builder->GetInt64Ty()}, builder->GetUnitTy());
    auto* method = builder->CreateFunction(methodTy, "m", "m", "", "default");
    builder->CreateParameter(classRefTy, INVALID_LOCATION, *method);
    builder->CreateParameter(builder->GetInt64Ty(), INVALID_LOCATION, *method);
    auto* methodBody = builder->CreateBlockGroup(*method);
    method->InitBody(*methodBody);
    auto* methodBlock = builder->CreateBlock(methodBody);
    methodBody->SetEntryBlock(methodBlock);
    methodBlock->AppendExpression(builder->CreateTerminator<Exit>(methodBlock));
    classDef->AddMethod(method);

    auto* callerTy = builder->GetType<FuncType>(std::vector<Type*>{}, builder->GetUnitTy());
    auto* caller = builder->CreateFunction(callerTy, "invoker", "invoker", "", "default");
    auto* body = builder->CreateBlockGroup(*caller);
    caller->InitBody(*body);
    auto* block = builder->CreateBlock(body);
    body->SetEntryBlock(block);

    auto* objAlloc = builder->CreateExpression<Allocate>(classRefTy, classTy, block);
    block->AppendExpression(objAlloc);
    auto* arg = builder->CreateConstantExpression<IntLiteral>(builder->GetInt64Ty(), block, 1);
    block->AppendExpression(arg);
    InvokeCallContext invokeCtx{
        .method = method,
        .caller = objAlloc->GetResult(),
        .funcCallCtx = FuncCallContext{.args = {arg->GetResult()}, .thisType = classTy},
    };
    auto* invoke = builder->CreateExpression<Invoke>(builder->GetUnitTy(), invokeCtx, block);
    block->AppendExpression(invoke);
    block->AppendExpression(builder->CreateTerminator<Exit>(block));

    CallGraph cg(package);
    auto callees = cg.GetAllPossibleCalleeOfInvoke(std::make_pair(std::string("m"), std::vector<Type*>{}));
    EXPECT_TRUE(callees.empty());

    auto* node = cg.GetOrCreateNode(*caller);
    // Virtual callee stub yields no edges; exercise DeleteCalledEdge / Edge== / const iterators manually.
    auto* calleeNode = cg.GetOrCreateNode(*method);
    CallGraph::Edge e1(calleeNode, CallGraph::Edge::Kind::VIRTUAL);
    CallGraph::Edge e2(calleeNode, CallGraph::Edge::Kind::DIRECT);
    EXPECT_TRUE(e1 == e2); // equality ignores kind
    node->AddCalledEdge(e1);
    const CallGraph::Node* cnode = node;
    EXPECT_NE(cnode->Begin(), cnode->End());
    EXPECT_FALSE(cnode->Empty());
    node->DeleteCalledEdge(e2); // erase by node equality
    node->DeleteCalledEdge(CallGraph::Edge(cg.GetEntryNode(), CallGraph::Edge::Kind::VIRTUAL)); // no-op erase

    CallGraphAnalysis analysis(package);
    analysis.DoCallGraphAnalysis(true);
    auto names = CollectNonNullNames(analysis.postOrderSCCFunctionlist);
    EXPECT_NE(std::find(names.begin(), names.end(), "invoker"), names.end());
}

TEST_F(CallGraphAnalysisTest, DiamondCallGraphAndBackEdge)
{
    // entryRoot (no users) -> root -> {left,right} -> sink2 -> root (SCC).
    auto* unitTy = builder->GetUnitTy();
    auto* funcTy = builder->GetType<FuncType>(std::vector<Type*>{}, unitTy);

    auto* left = builder->CreateFunction(funcTy, "left", "left", "", "default");
    auto* right = builder->CreateFunction(funcTy, "right", "right", "", "default");
    auto* root = builder->CreateFunction(funcTy, "root", "root", "", "default");
    auto* sink2 = builder->CreateFunction(funcTy, "sink2", "sink2", "", "default");
    auto* entryRoot = builder->CreateFunction(funcTy, "entryRoot", "entryRoot", "", "default");

    auto Init = [this](Function* func, const std::vector<Function*>& callees) {
        auto* body = builder->CreateBlockGroup(*func);
        func->InitBody(*body);
        auto* block = builder->CreateBlock(body);
        body->SetEntryBlock(block);
        for (auto* c : callees) {
            AppendDirectCall(*builder, block, c);
        }
        block->AppendExpression(builder->CreateTerminator<Exit>(block));
    };
    Init(left, {sink2});
    Init(right, {sink2});
    Init(sink2, {root});
    Init(root, {left, right});
    Init(entryRoot, {root});

    CallGraphAnalysis analysis(package);
    analysis.DoCallGraphAnalysis(false);
    auto names = CollectNonNullNames(analysis.postOrderSCCFunctionlist);
    EXPECT_NE(std::find(names.begin(), names.end(), "root"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "sink2"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "entryRoot"), names.end());
}
