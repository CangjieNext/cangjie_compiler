// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/Optimization/RedundantLoadElimination.h"

#include "cangjie/CHIR/Analysis/Engine.h"
#include "cangjie/CHIR/Analysis/ReachingDefinitionAnalysis.h"
#include "cangjie/CHIR/Analysis/Utils.h"

#include <iostream>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace Cangjie::CHIR {

namespace {
// Issue 1088: after inlining mut methods, READONLY GetElementRef does not kill
// reaching-def of the parent struct, so RLE may reuse a stale whole-object value.
std::string SrcLoc(const DebugLocation& loc)
{
    auto [line, column] = loc.GetBeginPos();
    if (line == 0) {
        return "<no-src-loc>";
    }
    auto path = loc.GetAbsPath();
    if (path.empty()) {
        path = loc.GetFileName();
    }
    return path + ":" + std::to_string(line) + ":" + std::to_string(column);
}

std::optional<size_t> ExprIndexInBlock(const Expression& expr)
{
    auto bb = expr.GetParentBlock();
    if (!bb) {
        return std::nullopt;
    }
    auto exprs = bb->GetExpressions();
    for (size_t i = 0; i < exprs.size(); ++i) {
        if (exprs[i] == &expr) {
            return i;
        }
    }
    return std::nullopt;
}

// Nested write is only a stale-load bug if it happens before this Load.
// Same block: require write index < load index. Different block: keep it
// (inlined mut body may be split across blocks).
bool NestedWriteMayAffectLoad(const Expression& write, const Load& load)
{
    if (write.GetParentBlock() != load.GetParentBlock()) {
        return true;
    }
    auto writeIdx = ExprIndexInBlock(write);
    auto loadIdx = ExprIndexInBlock(load);
    if (!writeIdx.has_value() || !loadIdx.has_value()) {
        return true;
    }
    return *writeIdx < *loadIdx;
}

bool IsStructRef(const Value& location)
{
    auto ty = location.GetType();
    return ty && ty->IsRef() && StaticCast<RefType*>(ty)->GetBaseType()->IsStruct();
}

struct Issue1088Write {
    const Expression* write{nullptr};       // StoreElementRef or mut Apply
    const Expression* readonlyAddr{nullptr}; // GetElementRef/GetElementByName on the path
};

std::vector<Issue1088Write> CollectStaleNestedWrites(const Value* location)
{
    struct WorkItem {
        const Value* val;
        bool seenReadonly;
        const Expression* readonlyAddr;
    };
    std::vector<Issue1088Write> writes;
    std::vector<WorkItem> worklist{{location, false, nullptr}};
    std::unordered_set<const Value*> visited;
    while (!worklist.empty()) {
        auto item = worklist.back();
        worklist.pop_back();
        if (!visited.insert(item.val).second) {
            continue;
        }
        for (auto user : item.val->GetUsers()) {
            if (auto ger = DynamicCast<GetElementRef*>(user)) {
                bool readonly = ger->GetResult()->TestAttr(Attribute::READONLY);
                worklist.push_back({ger->GetResult(), item.seenReadonly || readonly,
                    readonly ? ger : item.readonlyAddr});
            } else if (auto gebn = DynamicCast<GetElementByName*>(user)) {
                bool readonly = gebn->GetResult()->TestAttr(Attribute::READONLY);
                worklist.push_back({gebn->GetResult(), item.seenReadonly || readonly,
                    readonly ? gebn : item.readonlyAddr});
            } else if (item.val == location) {
                // Direct write to the allocation is handled by reaching-def.
                continue;
            } else if (auto ser = DynamicCast<StoreElementRef*>(user)) {
                if (ser->GetBase() == item.val && item.seenReadonly) {
                    writes.push_back({ser, item.readonlyAddr});
                }
            } else if (auto apply = DynamicCast<ApplyBase*>(user)) {
                auto callee = apply->GetCallee();
                auto args = apply->GetArgs();
                if (callee && callee->TestAttr(Attribute::MUT) && !args.empty() && args[0] == item.val &&
                    item.seenReadonly) {
                    writes.push_back({apply, item.readonlyAddr});
                }
            }
        }
    }
    return writes;
}

void PrintIssue1088Rle(const Function& func, const Load& load, const Expression& reused)
{
    auto location = load.GetLocation();
    if (!location || !IsStructRef(*location)) {
        return;
    }
    auto writes = CollectStaleNestedWrites(location);
    std::vector<Issue1088Write> affecting;
    for (auto& w : writes) {
        if (w.write && NestedWriteMayAffectLoad(*w.write, load)) {
            affecting.push_back(w);
        }
    }
    if (affecting.empty()) {
        return;
    }

    std::string objName = location->GetSrcCodeIdentifier();
    const Expression* alloc = nullptr;
    if (location->IsLocalVar()) {
        alloc = StaticCast<const LocalVar*>(location)->GetExpr();
    }

    std::cout << "[ISSUE-1088] RLE reuses a stale whole-struct value after nested mut\n";
    std::cout << "  func: " << func.GetSrcCodeIdentifier()
              << "  pkg: " << func.GetPackageName()
              << "  mangled: " << func.GetIdentifier() << "\n";
    std::cout << "  object: " << location->GetIdentifier();
    if (!objName.empty()) {
        std::cout << "[" << objName << "]";
    }
    std::cout << "  type: " << location->GetType()->ToString();
    if (alloc) {
        std::cout << "  created at: " << SrcLoc(alloc->GetDebugLocation());
    } else {
        std::cout << "  created at: " << SrcLoc(location->GetDebugLocation());
    }
    std::cout << "\n";
    for (auto& w : affecting) {
        std::cout << "  nested assignment lost: " << w.write->GetExprKindName()
                  << " at " << SrcLoc(w.write->GetDebugLocation());
        if (w.readonlyAddr) {
            std::cout << "  via READONLY " << w.readonlyAddr->GetExprKindName()
                      << " at " << SrcLoc(w.readonlyAddr->GetDebugLocation());
        }
        std::cout << "\n";
    }
    std::cout << "  stale load replaced at: " << SrcLoc(load.GetDebugLocation())
              << "  reused " << reused.GetExprKindName()
              << " at " << SrcLoc(reused.GetDebugLocation()) << "\n";
    for (auto user : load.GetResult()->GetUsers()) {
        std::cout << "  stale value used by: " << user->GetExprKindName()
                  << " at " << SrcLoc(user->GetDebugLocation()) << "\n";
    }
}
} // namespace

RedundantLoadElimination::RedundantLoadElimination()
{
}

void RedundantLoadElimination::RunOnPackage(const Ptr<const Package>& package, bool isDebug) const
{
    for (auto func : package->GetGlobalFuncsWithBody()) {
        RunOnFunc(func, isDebug);
    }
}

static void ModifyApplyCalleeInfo(const LocalVar& loadResult, Value& storeValue)
{
    if (!storeValue.IsFunc()) {
        return;
    }
    auto funcBase = StaticCast<Function*>(&storeValue);
    if (!funcBase->IsMemberFunc() || !funcBase->TestAttr(Attribute::STATIC)) {
        return;
    }
    // If the direct value func call comes from a load,
    //   it is invalid if func is generic(must from a GetInstantiateValue), so this type is its parent directly.
    Type* thisType = funcBase->GetParentCustomTypeOrExtendedType();
    std::vector<Expression*> oldUsers = loadResult.GetUsers();
    for (auto user : oldUsers) {
        if (auto apply = DynamicCast<ApplyBase*>(user)) {
            apply->SetThisType(thisType);
        }
    }
}

void RedundantLoadElimination::RunOnFunc(const Ptr<const Function>& func, bool isDebug) const
{
    if (func->TestAttr(Attribute::SKIP_ANALYSIS)) {
        return;
    }
    auto analysis = std::make_unique<ReachingDefinitionAnalysis>(func);
    auto engine = Engine<ReachingDefinitionDomain>(func, std::move(analysis));
    auto result = engine.IterateToFixpoint();
    CJC_NULLPTR_CHECK(result);

    const auto actionBeforeVisitExpr = [](const ReachingDefinitionDomain&, Expression*, size_t) {};

    std::unordered_map<Block*, std::vector<size_t>> toBeRemoved;
    const auto actionAfterVisitExpr = [func, isDebug, &toBeRemoved](
                                          const ReachingDefinitionDomain& state, Expression* expr, size_t index) {
        CJC_NULLPTR_CHECK(expr);
        if (expr->GetExprKind() != ExprKind::LOAD) {
            return;
        }
        auto load = StaticCast<Load*>(expr);
        if (auto def = state.CheckReachingDef(load->GetLocation()); def) {
            /*
             * adjust value call if load's result is a call
             * %1: () -> Int64& = Allocate(() -> Int64)
             * %2: Unit = Store(@StaticFunc, %1)  // static func in custom type
             * %3: () -> Int64 = Load(%1)
             * %8: Int64 = Apply(%3 : () -> Int64)  // if %3 is opt by RLE, this apply need adjustment
             */
            ModifyApplyCalleeInfo(*load->GetResult(), *def->GetValue());

            PrintIssue1088Rle(*func, *load, *def);

            // replace load with its exact value
            load->GetResult()->ReplaceWith(*def->GetValue(), func->GetBody());
            toBeRemoved[load->GetParentBlock()].emplace_back(index);
            if (isDebug) {
                std::string message = "[RLE] The usages of the result of Load" +
                    ToPosInfo(load->GetDebugLocation()) + " have been replaced by the value" +
                    ToPosInfo(def->GetDebugLocation()) + "\n";
                std::cout << message;
            }
            return;
        }

        if (auto validLoad = state.CheckReachingLoadDef(load->GetLocation()); validLoad) {
            if (validLoad == load) {
                return;
            }
            PrintIssue1088Rle(*func, *load, *validLoad);
            load->GetResult()->ReplaceWith(*validLoad->GetResult(), func->GetBody());
            toBeRemoved[load->GetParentBlock()].emplace_back(index);
            if (isDebug) {
                std::string message = "[RLE] The usages of the result of Load" +
                    ToPosInfo(load->GetDebugLocation()) + " have been replaced by the value" +
                    ToPosInfo(validLoad->GetDebugLocation()) + "\n";
                std::cout << message;
            }
        }
    };

    const auto actionOnTerminator = [](const ReachingDefinitionDomain&, Expression*, std::optional<Block*>) {};

    result->VisitWith(actionBeforeVisitExpr, actionAfterVisitExpr, actionOnTerminator);

    for (auto& [bb, indexes] : toBeRemoved) {
        std::vector<Expression*> exprs;
        for (auto i : indexes) {
            exprs.emplace_back(bb->GetExpressionByIdx(i));
        }
        for (auto e : exprs) {
            e->RemoveSelfFromBlock();
        }
    }
}
}
