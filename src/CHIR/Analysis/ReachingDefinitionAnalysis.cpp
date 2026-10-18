// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/Analysis/ReachingDefinitionAnalysis.h"

#include "cangjie/CHIR/Analysis/Utils.h"
#include "cangjie/CHIR/IR/Type/StructDef.h"
#include "cangjie/CHIR/Utils/Visitor/Visitor.h"
#include "cangjie/Utils/CheckUtils.h"

using namespace Cangjie;
using namespace Cangjie::CHIR;

namespace {
const Value* GetRootRefLocation(const Value* val)
{
    // Nested field refs use GetElementRef/GetElementByName results as the address.
    // allocateIdxMap only tracks Allocate results, so walk back to the root ref.
    while (val != nullptr) {
        auto localVar = DynamicCast<const LocalVar*>(val);
        if (localVar == nullptr) {
            break;
        }
        auto expr = localVar->GetExpr();
        if (auto getElemRef = DynamicCast<const GetElementRef*>(expr)) {
            val = getElemRef->GetBase();
            continue;
        }
        if (auto getByName = DynamicCast<const GetElementByName*>(expr)) {
            val = getByName->GetLocation();
            continue;
        }
        break;
    }
    return val;
}

/**
 * Whether the GetElementRef path targets an immutable member (`let`/`const`).
 * Do not use GetElementRef result's READONLY: it is also set when the base SSA
 * value is readonly (e.g. `this` parameter), which does not mean the field is immutable.
 * Match CustomType::GetInstMemberTypeByPathCheckingReadOnly: any readonly member on the path
 * makes the whole path readonly.
 */
bool IsPathToReadOnlyMember(Type* baseTy, const std::vector<uint64_t>& path)
{
    auto pureTy = baseTy->StripAllRefs();
    if (!pureTy->IsNominal() || path.empty()) {
        return false;
    }
    auto def = StaticCast<CustomType*>(pureTy)->GetCustomTypeDef();
    for (size_t i = 0; i < path.size(); ++i) {
        const auto& members = def->GetAllInstanceVars();
        CJC_ASSERT(path[i] < members.size());
        if (members[path[i]].IsImmutable()) {
            return true;
        }
        if (i + 1 == path.size()) {
            break;
        }
        auto memberTy = members[path[i]].type->StripAllRefs();
        if (!memberTy->IsNominal()) {
            return false;
        }
        def = StaticCast<CustomType*>(memberTy)->GetCustomTypeDef();
    }
    return false;
}
} // namespace

ReachingDefinitionDomain::ReachingDefinitionDomain(std::unordered_map<const Value*, size_t>* allocateIdxMap)
    : AbstractDomain(),
      reachingDefs(std::vector<FlatSet<const Store*>>(allocateIdxMap->size(), FlatSet<const Store*>(false))),
      reachingLoadDefs(std::vector<FlatSet<const Load*>>(allocateIdxMap->size(), FlatSet<const Load*>(false))),
      allocateIdxMap(allocateIdxMap)
{
}

bool ReachingDefinitionDomain::Join(const ReachingDefinitionDomain& rhs)
{
    this->kind = ReachableKind::REACHABLE;
    // should not use ||, cannot be short-circuited
    auto res1 = VectorJoin(reachingDefs, rhs.reachingDefs);
    auto res2 = VectorJoin(reachingLoadDefs, rhs.reachingLoadDefs);
    return res1 || res2;
}

std::string ReachingDefinitionDomain::ToString() const
{
    if (this->kind == ReachableKind::UNREACHABLE) {
        return "Unreachable";
    } else {
        std::stringstream ss;
        ss << "{ ";
        for (auto& def : reachingDefs) {
            ss << def.ToString() << ", ";
        }
        ss << "} ";
        ss << " { ";
        for (auto& def : reachingLoadDefs) {
            ss << def.ToString() << ", ";
        }
        ss << "}";
        return ss.str();
    }
}

const Store* ReachingDefinitionDomain::CheckReachingDef(const Value* location) const
{
    if (auto it = allocateIdxMap->find(location); it != allocateIdxMap->end()) {
        return reachingDefs[it->second].GetElem().value_or(nullptr);
    } else {
        return nullptr;
    }
}

const Load* ReachingDefinitionDomain::CheckReachingLoadDef(const Value* location) const
{
    if (auto it = allocateIdxMap->find(location); it != allocateIdxMap->end()) {
        return reachingLoadDefs[it->second].GetElem().value_or(nullptr);
    } else {
        return nullptr;
    }
}

template <> const std::string Analysis<ReachingDefinitionDomain>::name = "reaching-definition";
template <> const std::optional<unsigned> Analysis<ReachingDefinitionDomain>::blockLimit = std::nullopt;

ReachingDefinitionAnalysis::ReachingDefinitionAnalysis(const Function* func) : Analysis(func)
{
    size_t allocateIdx = 0;
    size_t worklistIdx = 0;
    auto worklist = func->GetBody()->GetBlocks();
    while (worklistIdx != worklist.size()) {
        auto bb = worklist[worklistIdx];
        for (auto expr : bb->GetExpressions()) {
            Type* allocatedTy = nullptr;
            if (Is<AllocateBase>(expr)) {
                allocatedTy = StaticCast<AllocateBase*>(expr)->GetType();
            } else if (Is<Lambda>(expr)) {
                auto blocks = StaticCast<const Lambda*>(expr)->GetBody()->GetBlocks();
                worklist.insert(worklist.end(), blocks.begin(), blocks.end());
            }
            const auto canBeTracked = [](const Type* allocatedTy) {
                if (allocatedTy->IsClass()) {
                    // It's useless to track an allocation like Allocate(Class-CA).
                    // We will never directly store to a Class-CA&.
                    return false;
                }
                if (allocatedTy->IsVArray()) {
                    // A VArray can be passed into a CFunc.
                    return false;
                }
                if (allocatedTy->IsStruct()) {
                    auto structDef = StaticCast<const StructType*>(allocatedTy)->GetStructDef();
                    if (structDef->IsCStruct()) {
                        return false;
                    }
                }
                if (allocatedTy->IsGeneric()) {
                    return false;
                }
                return true;
            };
            if (allocatedTy && canBeTracked(allocatedTy)) {
                allocateIdxMap.emplace(expr->GetResult(), allocateIdx++);
            }
        }
        ++worklistIdx;
    }
}

ReachingDefinitionDomain ReachingDefinitionAnalysis::Bottom()
{
    return ReachingDefinitionDomain(&allocateIdxMap);
}

void ReachingDefinitionAnalysis::InitializeFuncEntryState(ReachingDefinitionDomain& state)
{
    state.kind = ReachableKind::REACHABLE;
    for (size_t i = 0; i < state.allocateIdxMap->size(); ++i) {
        state.reachingDefs[i].SetToBound(/* isTop = */ true);
        state.reachingLoadDefs[i].SetToBound(/* isTop = */ true);
    }
}

void ReachingDefinitionAnalysis::HandleVarStateCapturedByLambda(ReachingDefinitionDomain& state, const Lambda* lambda)
{
    for (auto var : GetLambdaCapturedVarsRecursively(*lambda)) {
        if (auto it = state.allocateIdxMap->find(var); it != state.allocateIdxMap->end()) {
            state.reachingDefs[it->second].SetToBound(/* isTop = */ true);
            state.reachingLoadDefs[it->second].SetToBound(/* isTop = */ true);
        }
    }
}

void ReachingDefinitionAnalysis::PropagateExpressionEffect(
    ReachingDefinitionDomain& state, const Expression* expression)
{
    switch (expression->GetExprKind()) {
        case ExprKind::STORE:
            return HandleStoreExpr(state, StaticCast<const Store*>(expression));
        case ExprKind::GET_ELEMENT_REF:
            return HandleGetElemRefExpr(state, StaticCast<const GetElementRef*>(expression));
        case ExprKind::STORE_ELEMENT_REF:
            return HanldeStoreElemRefExpr(state, *StaticCast<const StoreElementRef*>(expression));
        case ExprKind::APPLY:
            return HandleApplyExpr(state, StaticCast<const ApplyBase*>(expression));
        case ExprKind::INTRINSIC:
            return HandleIntrinsicExpr(state, StaticCast<const IntrinsicBase*>(expression));
        case ExprKind::LOAD:
            return HandleLoadExpr(state, StaticCast<const Load*>(expression));
        default:
            return;
    }
}

std::optional<Block*> ReachingDefinitionAnalysis::PropagateTerminatorEffect(
    ReachingDefinitionDomain& state, const Expression* terminator)
{
    if (auto apply = DynamicCast<const ApplyBase*>(terminator)) {
        HandleApplyExpr(state, apply);
    } else if (auto intrinsic = DynamicCast<const IntrinsicBase*>(terminator)) {
        HandleIntrinsicExpr(state, intrinsic);
    }
    return std::nullopt;
}

void ReachingDefinitionAnalysis::HandleStoreExpr(ReachingDefinitionDomain& state, const Store* store)
{
    // If it's a store to a tracked allocation.
    auto target = store->GetLocation();
    if (auto it = allocateIdxMap.find(target); it != allocateIdxMap.end()) {
        state.reachingDefs[it->second].UpdateElem(store);
        state.reachingLoadDefs[it->second].SetToBound(/* isTop = */ true);
    }
}

void ReachingDefinitionAnalysis::HandleApplyExpr(ReachingDefinitionDomain& state, const ApplyBase* apply)
{
    auto callee = DynamicCast<const Function*>(apply->GetCallee());
    if (callee == nullptr || !callee->IsMemberFunc() || callee->TestAttr(Attribute::STATIC)) {
        return;
    }
    CJC_ASSERT(!apply->GetArgs().empty());
    KillTrackedStructReachingDefs(state, GetRootRefLocation(apply->GetArg(0)));
}

void ReachingDefinitionAnalysis::HandleIntrinsicExpr(ReachingDefinitionDomain& state, const IntrinsicBase* intrinsic)
{
    // If the tracked object is the parameter of the inout intrinsic
    if (intrinsic->GetIntrinsicKind() == CHIR::IntrinsicKind::INOUT_PARAM) {
        auto args = intrinsic->GetArgs();
        CJC_ASSERT(args.size() == 1);
        KillTrackedReachingDefs(state, GetRootRefLocation(args[0]));
    }
}

void ReachingDefinitionAnalysis::KillTrackedReachingDefs(ReachingDefinitionDomain& state, const Value* location)
{
    if (auto it = allocateIdxMap.find(location); it != allocateIdxMap.end()) {
        state.reachingDefs[it->second].SetToBound(/* isTop = */ true);
        state.reachingLoadDefs[it->second].SetToBound(/* isTop = */ true);
    }
}

void ReachingDefinitionAnalysis::KillTrackedStructReachingDefs(
    ReachingDefinitionDomain& state, const Value* location)
{
    CJC_NULLPTR_CHECK(location);
    if (!location->GetType()->IsRef()) {
        return;
    }
    if (StaticCast<RefType*>(location->GetType())->GetBaseType()->IsStruct()) {
        KillTrackedReachingDefs(state, location);
    }
}

void ReachingDefinitionAnalysis::HandleGetElemRefExpr(ReachingDefinitionDomain& state, const GetElementRef* getElemRef)
{
    // Writable member refs may later be written; kill whole-object reaching defs.
    // Skip immutable members (`let`/`const`) so store-to-load forwarding like struct1 still works.
    // Decide by the member on the path, not GetElementRef result READONLY (which also comes from a
    // readonly base such as `this`).
    if (IsPathToReadOnlyMember(getElemRef->GetBase()->GetType(), getElemRef->GetPath())) {
        return;
    }
    KillTrackedStructReachingDefs(state, GetRootRefLocation(getElemRef->GetBase()));
}

void ReachingDefinitionAnalysis::HanldeStoreElemRefExpr(
    ReachingDefinitionDomain& state, const StoreElementRef& storeElemRef)
{
    // Base may be a nested field ref (e.g. GetElementRef(tmp1, 0)); kill the root Allocate.
    KillTrackedStructReachingDefs(state, GetRootRefLocation(storeElemRef.GetBase()));
}

void ReachingDefinitionAnalysis::HandleLoadExpr(ReachingDefinitionDomain& state, const Load* load)
{
    // If it's a load to a tracked allocation.
    auto target = load->GetLocation();
    if (auto it = allocateIdxMap.find(target); it != allocateIdxMap.end()) {
        // If there is a Top, it means that the current Load we are visiting is the first Load operation after the
        // value stored in an allocation has changed. If there is already a Load expression, it means that the
        // current Load we are visiting is redundant.
        if (state.reachingLoadDefs[it->second].IsTop()) {
            state.reachingLoadDefs[it->second].UpdateElem(load);
        }
    }
}