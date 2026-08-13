// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/Optimization/ArrayListConstStartOpt.h"

#include "cangjie/CHIR/IR/Type/CustomTypeDef.h"
#include "cangjie/CHIR/IR/Type/Type.h"
#include "cangjie/CHIR/Utils/CHIRCasting.h"
#include "cangjie/CHIR/Utils/Utils.h"
#include "cangjie/Utils/ConstantsUtils.h"

namespace Cangjie::CHIR {
namespace {
constexpr const char* ARRAY_TYPE_NAME = "Array";
constexpr const char* ARRAYLIST_TYPE_NAME = "ArrayList";
constexpr const char* ARRAYLIST_ITERATOR_TYPE_NAME = "ArrayListIterator";
constexpr const char* MY_DATA_MEMBER_NAME = "myData";
constexpr const char* START_MEMBER_NAME = "start";

static const std::vector<FuncInfo> ARRAY_FUNC_INLINE_WHITE_LIST = {
    FuncInfo("getUnchecked", ARRAY_TYPE_NAME, {NOT_CARE}, ANY_TYPE, CORE_PACKAGE_NAME),
    FuncInfo("setUnchecked", ARRAY_TYPE_NAME, {NOT_CARE}, ANY_TYPE, CORE_PACKAGE_NAME),
};

static const std::vector<FuncInfo> ARRAYLIST_FUNC_LIST = {
    FuncInfo("[]", ARRAYLIST_TYPE_NAME, {NOT_CARE}, ANY_TYPE, COLLECTION_PACKAGE_NAME),
    FuncInfo("==", ARRAYLIST_TYPE_NAME, {NOT_CARE}, ANY_TYPE, COLLECTION_PACKAGE_NAME),
    FuncInfo("next", ARRAYLIST_ITERATOR_TYPE_NAME, {NOT_CARE}, ANY_TYPE, COLLECTION_PACKAGE_NAME),
};

bool InWhiteList(const Function& func, const std::vector<FuncInfo>& whiteList)
{
    for (auto element : whiteList) {
        if (IsExpectedFunction(func, element)) {
            return true;
        }
    }
    return false;
}

bool IsMemberAccess(const Type& baseTy, const std::vector<uint64_t>& path, const std::string& packageName,
    const std::string& typeName, const std::string& memberName)
{
    if (path.size() != 1) {
        return false;
    }
    auto customTy = DynamicCast<CustomType*>(baseTy.StripAllRefs());
    if (customTy == nullptr || !IsExpectedCustomType(*customTy, packageName, typeName)) {
        return false;
    }
    auto def = customTy->GetCustomTypeDef();
    if (path[0] >= def->GetAllInstanceVarNum()) {
        return false;
    }
    return def->GetInstanceVar(path[0]).name == memberName;
}

/** Match `value` as Load(GetElementRef(base, member)) on the given custom type.
 *  Returns `base` on success, otherwise nullptr.
 */
Value* MatchLoadOfMemberRef(
    Value& value, const std::string& packageName, const std::string& typeName, const std::string& memberName)
{
    auto local = DynamicCast<LocalVar*>(&value);
    if (local == nullptr) {
        return nullptr;
    }
    auto load = DynamicCast<Load*>(local->GetExpr());
    if (load == nullptr) {
        return nullptr;
    }
    auto locLocal = DynamicCast<LocalVar*>(load->GetLocation());
    if (locLocal == nullptr) {
        return nullptr;
    }
    auto getElementRef = DynamicCast<GetElementRef*>(locLocal->GetExpr());
    if (getElementRef == nullptr) {
        return nullptr;
    }
    if (!IsMemberAccess(
        *getElementRef->GetBase()->GetType(), getElementRef->GetPath(), packageName, typeName, memberName)) {
        return nullptr;
    }
    return getElementRef->GetBase();
}

/** True if `value` is a parameter of `func` with the expected custom type (e.g. this / that: ArrayList). */
bool IsFuncParamOfType(
    Value& value, const Function& func, const std::string& packageName, const std::string& typeName)
{
    if (!value.IsParameter()) {
        return false;
    }
    auto* param = StaticCast<Parameter*>(&value);
    bool belongsToFunc = false;
    for (auto* funcParam : func.GetParams()) {
        if (funcParam == param) {
            belongsToFunc = true;
            break;
        }
    }
    return belongsToFunc && IsExpectedCustomType(*param->GetType()->StripAllRefs(), packageName, typeName);
}
} // namespace

const OptEffectCHIRMap& ArrayListConstStartOpt::GetEffectMap() const
{
    return effectMap;
}

bool ArrayListConstStartOpt::CheckNeedInline(const Apply& apply) const
{
    if (!apply.GetCallee()->IsFuncWithBody()) {
        return false;
    }
    auto callee = StaticCast<Function*>(apply.GetCallee());
    return InWhiteList(*callee, ARRAY_FUNC_INLINE_WHITE_LIST);
}

/**
 * Recognize Field of `Array.start` loaded from `ArrayList.myData` (optionally via
 * `ArrayListIterator.myData`), using type and member names instead of layout indices.
 *
 * ArrayList:
 *   Field(Load(GetElementRef(arrayList, myData)), start)
 * ArrayListIterator:
 *   Field(Load(GetElementRef(Load(GetElementRef(arrayListIterator, myData)), myData)), start)
 */
bool ArrayListConstStartOpt::IsStartOfArrayListMyData(const Field& field, const Function& func) const
{
    if (!IsMemberAccess(
        *field.GetBase()->GetType(), field.GetPath(), CORE_PACKAGE_NAME, ARRAY_TYPE_NAME, START_MEMBER_NAME)) {
        return false;
    }

    // ArrayList: Field(Load(GetElementRef(arrayList, myData)), start)
    // arrayList may be `this` or `that` (e.g. in operator ==).
    auto arrayList = MatchLoadOfMemberRef(
        *field.GetBase(), COLLECTION_PACKAGE_NAME, ARRAYLIST_TYPE_NAME, MY_DATA_MEMBER_NAME);
    if (arrayList == nullptr) {
        return false;
    }
    if (IsFuncParamOfType(*arrayList, func, COLLECTION_PACKAGE_NAME, ARRAYLIST_TYPE_NAME)) {
        return true;
    }
    // ArrayListIterator: Field(Load(GetElementRef(Load(GetElementRef(arrayListIterator, myData)), myData)), start)
    auto arrayListIterator = MatchLoadOfMemberRef(
        *arrayList, COLLECTION_PACKAGE_NAME, ARRAYLIST_ITERATOR_TYPE_NAME, MY_DATA_MEMBER_NAME);
    if (arrayListIterator == nullptr) {
        return false;
    }
    return IsFuncParamOfType(
        *arrayListIterator, func, COLLECTION_PACKAGE_NAME, ARRAYLIST_ITERATOR_TYPE_NAME);
}

void ArrayListConstStartOpt::RewriteStartWithConstZero(Expression& oldExpr) const
{
    auto oldExprResult = oldExpr.GetResult();
    auto oldExprParent = oldExpr.GetParentBlock();
    auto literalValueZero = builder.CreateLiteralValue<IntLiteral>(builder.GetInt64Ty(), 0UL);
    auto newExpr = builder.CreateExpression<Constant>(oldExprResult->GetType(), literalValueZero, oldExprParent);
    newExpr->SetDebugLocation(oldExpr.GetDebugLocation());

    oldExpr.ReplaceWith(*newExpr);
}

void ArrayListConstStartOpt::RewriteStartAfterInline(Function& func) const
{
    /** after inline, the ir is like this:
        public class ArrayList<T> {
            ...
            var myData: Array<T>
            public operator func [](index: Int64, value!: T): Unit {
                ...
                arraySetUnchecked(this.myData.rawptr, this.myData.start + index, value)
            }
            public operator func ==(other: ArrayList<T>): Bool {
                ...
                for (i in this.size - 1..=0 : -1) {
                    ...
                    arrayGetUnchecked(this.myData.rawptr, this.myData.start + i)
                    arrayGetUnchecked(other.myData.rawptr, other.myData.start + i)
                }
            }
        }
        class ArrayListIterator<T> {
            ...
            private let myData: ArrayList<T>
            public func next(): Option<T> {
                ...
                arrayGetUnchecked(myData.myData.rawptr, myData.myData.start + this.myPosition)
            }
        }
    
    */
    // 1. collect `myData.myData.start` in ArrayListIterator and `myData.start` in ArrayList
    std::vector<Field*> startFields;
    auto collect = [&startFields, &func, this](Expression& e) {
        auto* field = DynamicCast<Field*>(&e);
        if (field != nullptr && IsStartOfArrayListMyData(*field, func)) {
            startFields.push_back(field);
        }
        return VisitResult::CONTINUE;
    };
    Visitor::Visit(func, collect);
    // 2. rewrite start with const zero
    for (auto* field : startFields) {
        RewriteStartWithConstZero(*field);
    }
}

void ArrayListConstStartOpt::RunOnPackage(const Package& package)
{
    for (auto func : package.GetGlobalFuncsWithBody()) {
        // only inline array func into arrayList and arrayListIterator func
        if (!InWhiteList(*func, ARRAYLIST_FUNC_LIST)) {
            continue;
        }

        std::vector<Apply*> applies;
        auto collectApplies = [&applies, this](Expression& e) {
            auto apply = DynamicCast<Apply*>(&e);
            if (apply != nullptr && CheckNeedInline(*apply)) {
                applies.push_back(apply);
            }
            return VisitResult::CONTINUE;
        };
        Visitor::Visit(*func, collectApplies);

        // Inline then rewrite start at each site, before other passes can reshape IR.
        for (auto apply : applies) {
            pass.DoFunctionInline(*apply, optPassName);
            MergeEffectMap(pass.GetEffectMap(), effectMap);
            RewriteStartAfterInline(*func);
        }
        // Also rewrite when there is no getUnchecked/setUnchecked to inline but
        // myData.start is already present in the whitelist function body.
        RewriteStartAfterInline(*func);
    }
}
} // namespace Cangjie::CHIR
