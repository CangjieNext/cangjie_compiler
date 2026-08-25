// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/Analysis/TypeAnalysis.h"
#include "cangjie/CHIR/Utils/Utils.h"
#include "cangjie/CHIR/Utils/Visitor/Visitor.h"

#include <queue>

namespace Cangjie::CHIR {

TypeValue::TypeValue(DevirtualTyKind kind, Type* baseLineType) : kind(kind), baseLineType(baseLineType)
{
}

std::optional<std::unique_ptr<TypeValue>> TypeValue::Join(const TypeValue& rhs) const
{
    auto rhsClassTy = StaticCast<const TypeValue&>(rhs).GetSpecificType();
    if (this->baseLineType != rhsClassTy) {
        if (!this->baseLineType->IsClass() || !rhsClassTy->IsClass()) {
            return nullptr;
        }
        auto fatherTy = LeastCommonSuperClass(
            StaticCast<ClassType*>(this->baseLineType), StaticCast<ClassType*>(rhsClassTy), builder);
        if (fatherTy) {
            return std::make_unique<TypeValue>(DevirtualTyKind::SUBTYPE_OF, fatherTy);
        } else {
            return nullptr;
        }
    } else {
        if (this->kind == DevirtualTyKind::EXACTLY && rhs.kind == DevirtualTyKind::SUBTYPE_OF) {
            return std::make_unique<TypeValue>(DevirtualTyKind::SUBTYPE_OF, this->baseLineType); 
        } else {
            return std::nullopt;
        }
    }
}

std::string TypeValue::ToString() const
{
    return "{ " + GetKindString(kind) + ", " + baseLineType->ToString() + " }";
}

std::unique_ptr<TypeValue> TypeValue::Clone() const
{
    return std::make_unique<TypeValue>(kind, baseLineType);
}

DevirtualTyKind TypeValue::GetTypeKind() const
{
    return kind;
}

Type* TypeValue::GetSpecificType() const
{
    return baseLineType;
}

void TypeValue::SetCHIRBuilder(CHIRBuilder* chirBuilder)
{
    builder = chirBuilder;
}

std::string TypeValue::GetKindString(DevirtualTyKind clsTyKind) const
{
    switch (clsTyKind) {
        case DevirtualTyKind::SUBTYPE_OF:
            return "SUBTYPE_OF";
        case DevirtualTyKind::EXACTLY:
            return "EXACTLY";
        default:
            return "Unknown";
    }
}

CHIRBuilder* TypeValue::builder{nullptr};
template <> const std::string Analysis<TypeDomain>::name = "type-analysis";
template <> const std::optional<unsigned> Analysis<TypeDomain>::blockLimit = std::nullopt;
template <> TypeDomain::ChildrenMap ValueAnalysis<TypeValueDomain>::globalChildrenMap{};
template <> TypeDomain::AllocatedRefMap ValueAnalysis<TypeValueDomain>::globalAllocatedRefMap{};
template <> TypeDomain::AllocatedObjMap ValueAnalysis<TypeValueDomain>::globalAllocatedObjMap{};
template <> std::vector<std::unique_ptr<Ref>> ValueAnalysis<TypeValueDomain>::globalRefPool{};
template <> std::vector<std::unique_ptr<AbstractObject>> ValueAnalysis<TypeValueDomain>::globalAbsObjPool{};
template <>
TypeDomain ValueAnalysis<TypeValueDomain>::globalState{&globalChildrenMap, &globalAllocatedRefMap,
    nullptr, &globalAllocatedObjMap, &globalRefPool, &globalAbsObjPool};

template <> bool IsTrackedGV<ValueDomain<TypeValue>>(const GlobalVar& gv)
{
    Type* baseTy = gv.GetType();
    while (baseTy->IsRef()) {
        auto ty = StaticCast<RefType*>(baseTy)->GetBaseType();
        baseTy = ty;
    }
    return baseTy->IsClass();
}

TypeAnalysis::TypeAnalysis(
    const Function* func, CHIRBuilder& builder, bool isDebug, const DevirtualizationInfo& devirtInfo)
    : ValueAnalysis(func, builder, isDebug), realRetTyMap(devirtInfo.returnTypeMap),
      constMemberTypeMap(devirtInfo.constMemberTypeMap)
{
}

bool TypeAnalysis::CheckFuncHasInvoke(const Function& func)
{
    bool hasInvoke = false;
    auto preVisit = [&hasInvoke](Expression& expr) {
        if (Is<InvokeBase>(&expr)) {
            hasInvoke = true;
            return VisitResult::STOP;
        }
        return VisitResult::CONTINUE;
    };
    Visitor::Visit(func, preVisit);
    return hasInvoke;
}

bool TypeAnalysis::Filter(const Function& method)
{
    if (!CheckFuncHasInvoke(method)) {
        return false;
    }
    if (IsSTDFunction(method)) {
        return true;
    }
    const static size_t OVERHEAD_BLOCK_SIZE = 300U;
    return method.GetBody()->GetBlocks().size() <= OVERHEAD_BLOCK_SIZE;
}

void TypeAnalysis::PrintDebugMessage(const Expression* expr, const TypeValue* absVal) const
{
    std::string message = "The value of " + expr->GetResult()->GetIdentifier() + " = " + expr->ToString(0) +
        " has been set to " + absVal->ToString();
    std::cout << message << std::endl;
}

void TypeAnalysis::HandleNormalExpressionEffect(TypeDomain& state, const Expression* expression)
{
    switch (expression->GetExprKind()) {
        case ExprKind::CLASS_STATIC_CAST:
            return HandleClassStaticCastExpr(state, StaticCast<const ClassStaticCast*>(expression));
        case ExprKind::NUMERIC_CAST:
            // NumericCast does not contribute class-type facts for de-virtualization.
            state.TrySetToTopOrTopRef(
                expression->GetResult(), expression->GetResult()->GetType()->IsRef());
            return;
        case ExprKind::BOX:
            return HandleBoxExpr(state, StaticCast<const Box*>(expression));
        case ExprKind::UNBOX_TO_VALUE:
        case ExprKind::UNBOX_TO_REF:
            // UnBox: no class-type fact for de-virtualization.
            // Fall through to default (Top / EXACTLY for primitives).
        default: {
            auto res = expression->GetResult();
            auto domain = state.GetAbstractDomain(res);
            if ((!domain || domain->IsTop()) && (res->GetType()->IsPrimitive())) {
                state.Update(res, std::make_unique<TypeValue>(DevirtualTyKind::EXACTLY, res->GetType()));
            } else {
                state.TrySetToTopOrTopRef(res, res->GetType()->IsRef());
            }
            break;
        }
    }
    auto resultType = expression->GetResult()->GetType();
    if (isDebug && !resultType->IsRef() && !resultType->IsGeneric()) {
        if (auto absVal = state.CheckAbstractValue(expression->GetResult()); absVal) {
            PrintDebugMessage(expression, absVal);
        }
    }
}

void TypeAnalysis::HandleAllocateExpr(TypeDomain& state, const Allocate* expression, Value* newObj)
{
    if (!newObj) {
        return;
    }

    auto baseTy = expression->GetType();
    if (baseTy->IsClass()) {
        state.Update(newObj, std::make_unique<TypeValue>(DevirtualTyKind::EXACTLY, StaticCast<ClassType*>(baseTy)));
    }
}

std::optional<Block*> TypeAnalysis::HandleTerminatorEffect(TypeDomain& state, const Expression* terminator)
{
    switch (terminator->GetExprKind()) {
        case ExprKind::TRY_NUMERIC_CAST: {
            // Same as NumericCast: no class-type fact.
            auto dest = terminator->GetResult();
            if (dest) {
                state.SetToTopOrTopRef(dest, dest->GetType()->IsRef());
            }
            break;
        }
        case ExprKind::GOTO:
        case ExprKind::EXIT:
        case ExprKind::BRANCH:
        case ExprKind::MULTIBRANCH:
        case ExprKind::TRY_NEG:
        case ExprKind::TRY_ADD:
        case ExprKind::TRY_SUB:
        case ExprKind::TRY_MUL:
        case ExprKind::TRY_DIV:
        case ExprKind::TRY_MOD:
        case ExprKind::TRY_EXP:
        case ExprKind::TRY_LSHIFT:
        case ExprKind::TRY_RSHIFT:
        case ExprKind::TRY_INTRINSIC:
        default: {
            auto dest = terminator->GetResult();
            if (dest) {
                state.SetToTopOrTopRef(dest, dest->GetType()->IsRef());
            }
            break;
        }
    }

    return std::nullopt;
}

void TypeAnalysis::HandleInvokeExpr(TypeDomain& state, const Invoke* invoke, Value* refObj)
{
    auto resTy = invoke->GetResult()->GetType()->StripAllRefs();
    if (!resTy->IsClass()) {
        return;
    }
    return state.Update(
        refObj, std::make_unique<TypeValue>(DevirtualTyKind::SUBTYPE_OF, StaticCast<ClassType*>(resTy)));
}

std::optional<Block*> TypeAnalysis::HandleTryInvokeTerminator(
    TypeDomain& state, const TryInvoke* invoke, Value* refObj)
{
    auto resTy = invoke->GetResult()->GetType()->StripAllRefs();
    if (!resTy->IsClass()) {
        return std::nullopt;
    }
    state.Update(
        refObj, std::make_unique<TypeValue>(DevirtualTyKind::SUBTYPE_OF, StaticCast<ClassType*>(resTy)));
    return std::nullopt;
}

void TypeAnalysis::HandleApplyExpr(TypeDomain& state, const Apply* apply, Value* refObj)
{
    HandleApplyBase(state, apply, refObj);
}

std::optional<Block*> TypeAnalysis::HandleTryApplyTerminator(
    TypeDomain& state, const TryApply* apply, Value* refObj)
{
    HandleApplyBase(state, apply, refObj);
    return std::nullopt;
}

void TypeAnalysis::HandleApplyBase(TypeDomain& state, const ApplyBase* apply, Value* refObj) const
{
    auto callee = apply->GetCallee();
    if (!callee->IsFuncWithBody()) {
        HandleDefaultExpr(state, apply);
        return;
    }

    auto calleeFunc = StaticCast<Function*>(callee);
    auto it = realRetTyMap.find(calleeFunc);
    if (it == realRetTyMap.end()) {
        HandleDefaultExpr(state, apply);
        return;
    }
    auto realRetTy = it->second;
    if (realRetTy->IsGenericRelated()) {
        auto genericMap = GetInstMapFromApply(*apply, builder);
        realRetTy = ReplaceRawGenericArgType(*realRetTy, genericMap, builder);
        if (realRetTy == nullptr) {
            // cannot get real generic return type, return.
            return;
        }
    }
    return UpdateDefaultValue(state, apply->GetResult(), refObj, realRetTy);
}

// =============== Transfer functions for TypeCast expression =============== //

void TypeAnalysis::HandleBoxExpr(TypeDomain& state, const Box* boxExpr) const
{
    auto result = boxExpr->GetResult();
    auto obj = state.GetReferencedObjAndSetToTop(result, boxExpr);
    state.Propagate(boxExpr->GetSourceValue(), obj);
}

void TypeAnalysis::HandleClassStaticCastExpr(TypeDomain& state, const ClassStaticCast* typecast) const
{
    // Keep abstract type as precise as possible:
    // - never widen to a parent type;
    // - on upcast (or when abs type is already <= target): preserve kind and type (Propagate);
    // - on true downcast (target < abs type): narrow to SUBTYPE_OF(target).
    Type* srcTy = typecast->GetSourceType()->StripAllRefs();
    Type* tgtTy = typecast->GetTargetType()->StripAllRefs();
    LocalVar* result = typecast->GetResult();

    // Set an initial state; refined below when possible.
    if (result->GetType()->IsRef()) {
        state.GetReferencedObjAndSetToTop(result, typecast);
    } else {
        state.SetToTopOrTopRef(result, false);
    }

    // Skip non-class casts (e.g. NumericCast).
    if (!srcTy->IsClass() || !tgtTy->IsClass()) {
        return;
    }

    auto srcAbsObj = state.CheckAbstractObjectRefBy(typecast->GetSourceValue());
    if (!srcAbsObj) {
        return;
    }
    auto srcVal = state.CheckAbstractValue(srcAbsObj);
    if (!srcVal) {
        return;
    }

    Type* absTy = srcVal->GetSpecificType();
    if (!absTy->IsClass()) {
        return;
    }
    auto absClsTy = StaticCast<ClassType*>(absTy);
    auto tgtClsTy = StaticCast<ClassType*>(tgtTy);
    // absType <= target: upcast, identity, or already finer than downcast target -> keep.
    if (absClsTy->IsEqualOrSubTypeOf(*tgtClsTy, builder)) {
        return state.Propagate(typecast->GetSourceValue(), result);
    }

    // target < absType: true downcast -> narrow. Always SUBTYPE_OF(target): cast success
    // only proves "Child or its subclass", not EXACTLY Child.
    if (tgtClsTy->IsEqualOrSubTypeOf(*absClsTy, builder)) {
        Value* resVal = result;
        if (result->GetType()->IsRef()) {
            resVal = state.CheckAbstractObjectRefBy(result);
        }
        return state.Update(resVal, std::make_unique<TypeValue>(DevirtualTyKind::SUBTYPE_OF, tgtClsTy));
    }

    // Unrelated types: leave Top.
}

template <class MemberAccess>
static Type* GetInnerTypeFromMemberAccess(const MemberAccess& ma, CHIRBuilder& builder)
{
    const auto& path = ma.GetPath();
    auto locationType = ma.GetBase()->GetType();
    for (size_t i = 0; i < path.size() - 1; ++i) {
        CJC_NULLPTR_CHECK(locationType);
        auto index = path[i];
        locationType = GetFieldOfType(*locationType, index, builder);
    }
    CJC_NULLPTR_CHECK(locationType);
    return locationType;
}

static const CustomTypeDef* CheckMemberDefineInWhichParent(CustomType& type, size_t index)
{
    auto def = type.GetCustomTypeDef();
    auto memberInfo = def->GetInstanceVar(index);
    return memberInfo.outerDef == nullptr ? def : memberInfo.outerDef;
}

template <typename TMemberAccess>
void TypeAnalysis::HandleMemberAccess(TypeDomain& state, const TMemberAccess* memberAccess) const
{
    auto locationType = GetInnerTypeFromMemberAccess(*memberAccess, builder)->StripAllRefs();
    if (!locationType->IsClassOrStruct()) {
        return HandleDefaultExpr(state, memberAccess);
    }
    auto path = memberAccess->GetPath();
    auto index = path[path.size() - 1];
    auto customType = StaticCast<CustomType*>(locationType);
    // get member define in which parent.
    auto def = CheckMemberDefineInWhichParent(*customType, index);
    auto it = constMemberTypeMap.find(def);
    if (it == constMemberTypeMap.end()) {
        return HandleDefaultExpr(state, memberAccess);
    }
    auto it2 = it->second.find(index);
    if (it2 == it->second.end()) {
        return HandleDefaultExpr(state, memberAccess);
    }
    auto resType = it2->second->StripAllRefs();
    // member define:
    //   class XX<T> {
    //     let member: I<T> = CA<T>()
    //     ..
    //   }
    // user of XX: getElementRef or field:
    //     let x = XX<Int64>()
    //     x.member.func()
    //       => type of x.member will be a inst parent type I<Int64>. To Get CA<Int64>, we need inst sub type from
    //          parent XX's generic type params.
    if (resType->IsGenericRelated()) {
        auto [res, mapping] = def->GetType()->CalculateGenericTyMapping(*locationType);
        CJC_ASSERT(res);
        resType = ReplaceRawGenericArgType(*resType, mapping, builder);
    }
    AbstractObject* refObj;
    if constexpr (std::is_same_v<GetElementRef, TMemberAccess>) {
        refObj = state.GetTwoLevelRefAndSetToTop(memberAccess->GetResult(), memberAccess);
    } else {
        // else is field
        refObj = state.GetReferencedObjAndSetToTop(memberAccess->GetResult(), memberAccess);
    }
    UpdateDefaultValue(state, memberAccess->GetResult(), refObj, resType);
}

void TypeAnalysis::PreHandleGetElementRefExpr(TypeDomain& state, const GetElementRef* getElemRef)
{
    HandleMemberAccess(state, getElemRef);
}

void TypeAnalysis::PreHandleFieldExpr(TypeDomain& state, const Field* field)
{
    HandleMemberAccess(state, field);
}

void TypeAnalysis::HandleDefaultExpr(TypeDomain& state, const Expression* expr) const
{
    auto refObj = PreHandleDefaultExpr(state, expr);
    UpdateDefaultValue(state, expr->GetResult(), refObj, nullptr);
}

Value* TypeAnalysis::PreHandleDefaultExpr(TypeDomain& state, const Expression* expr) const
{
    auto result = expr->GetResult();
    auto resType = result->GetType();
    if (!resType->IsRef()) {
        state.SetToTopOrTopRef(result, false);
        return nullptr;
    }
    auto firstRefBaseType = StaticCast<RefType*>(resType)->GetBaseType();
    if (!firstRefBaseType->IsRef()) {
        auto refObj = state.GetReferencedObjAndSetToTop(result, expr);
        return refObj;
    }
    auto refObj = state.GetTwoLevelRefAndSetToTop(result, expr);
    return refObj;
}

void TypeAnalysis::UpdateDefaultValue(
    TypeDomain& state, Value* value, Value* refObj, Type* relType) const
{
    if (value == nullptr) {
        return;
    }
    auto dest = refObj ? refObj : value;
    auto type = relType == nullptr ? value->GetType()->StripAllRefs() : relType;
    if (type->IsBox()) {
        type = StaticCast<BoxType*>(type)->GetBaseType();
    }
    if (type->IsPrimitive() || type->IsStruct() || type->IsEnum()) {
        state.Update(dest, std::make_unique<TypeValue>(DevirtualTyKind::EXACTLY, type));
    }
    if (type->IsClass()) {
        auto classDef = StaticCast<ClassType*>(type)->GetClassDef();
        auto kind =
            !classDef->IsInterface() && !classDef->TestAttr(Attribute::VIRTUAL) && !classDef->IsAbstract() ?
            DevirtualTyKind::EXACTLY : DevirtualTyKind::SUBTYPE_OF;
        state.Update(dest, std::make_unique<TypeValue>(kind, type));
    }
}

void TypeAnalysis::HandleFuncParam(TypeDomain& state, Parameter* param, Value* refObj)
{
    if (!refObj) {
        return;
    }
    UpdateDefaultValue(state, param, refObj, nullptr);
}
}  // namespace Cangjie::CHIR