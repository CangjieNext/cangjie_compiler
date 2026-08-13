// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <algorithm>
#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>

#include "cangjie/CHIR/Analysis/CallGraphAnalysis.h"
#include "cangjie/CHIR/Analysis/DevirtualizationInfo.h"
#include "cangjie/CHIR/IR/CHIRBuilder.h"
#include "cangjie/CHIR/IR/Expression/Terminator.h"
#include "cangjie/CHIR/IR/Package.h"
#include "cangjie/Option/Option.h"

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
        opts = std::make_unique<GlobalOptions>();
        devirtInfo = std::make_unique<DevirtualizationInfo>(package, *opts);
    }

    std::unique_ptr<std::unordered_map<unsigned int, std::string>> nameMap;
    std::unique_ptr<CHIRContext> cctx;
    std::unique_ptr<CHIRBuilder> builder;
    Package* package{nullptr};
    std::unique_ptr<GlobalOptions> opts;
    std::unique_ptr<DevirtualizationInfo> devirtInfo;
};

TEST_F(CallGraphAnalysisTest, DirectCallChainPostOrder)
{
    // c <- b <- a (only a is reachable from entry)
    auto* c = CreateEmptyFunc(*builder, "c");
    auto* b = CreateCaller(*builder, "b", {c});
    auto* a = CreateCaller(*builder, "a", {b});
    (void)a;

    CallGraphAnalysis analysis(package, *devirtInfo);
    analysis.DoCallGraphAnalysis(true);

    auto names = CollectNonNullNames(analysis.postOrderSCCFunctionlist);
    ASSERT_EQ(FindIndex(names, "c") < FindIndex(names, "b"), true);
    ASSERT_EQ(FindIndex(names, "b") < FindIndex(names, "a"), true);
}

TEST_F(CallGraphAnalysisTest, MutualRecursionFormsOneSCC)
{
    // r -> a <-> b
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

    CallGraphAnalysis analysis(package, *devirtInfo);
    analysis.DoCallGraphAnalysis(true);

    auto names = CollectNonNullNames(analysis.postOrderSCCFunctionlist);
    auto idxA = FindIndex(names, "a");
    auto idxB = FindIndex(names, "b");
    auto idxR = FindIndex(names, "r");
    // a and b are in the same SCC, so they appear consecutively.
    EXPECT_EQ((idxA > idxB ? idxA - idxB : idxB - idxA), 1u);
    // Mutual SCC is processed before its caller r.
    EXPECT_LT(std::max(idxA, idxB), idxR);
}

TEST_F(CallGraphAnalysisTest, IndependentRootsReachableFromEntry)
{
    auto* leaf1 = CreateEmptyFunc(*builder, "leaf1");
    auto* leaf2 = CreateEmptyFunc(*builder, "leaf2");
    (void)leaf1;
    (void)leaf2;

    CallGraphAnalysis analysis(package, *devirtInfo);
    analysis.DoCallGraphAnalysis(false);

    auto names = CollectNonNullNames(analysis.postOrderSCCFunctionlist);
    EXPECT_NE(std::find(names.begin(), names.end(), "leaf1"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "leaf2"), names.end());
}
