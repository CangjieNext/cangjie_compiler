// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/Transformation/GenerateVTable/WrapMutFunc.h"

#include "cangjie/CHIR/Utils/CHIRCasting.h"
#include "cangjie/CHIR/IR/Type/ExtendDef.h"
#include "cangjie/CHIR/Utils/UserDefinedType.h"
#include "cangjie/CHIR/Utils/Utils.h"
#include "cangjie/Utils/FileUtil.h"
#include "cangjie/Mangle/CHIRManglingUtils.h"

using namespace Cangjie::CHIR;
using namespace Cangjie;

namespace {
std::unordered_map<const GenericType*, Type*> CollectReplaceTable(
    const CustomTypeDef& curDef, const Function& rawFunc, CHIRBuilder& builder)
{
    std::vector<ClassType*> inheritanceList;
    if (curDef.IsExtend()) {
        inheritanceList = curDef.GetType()->GetSuperTypesRecusively(builder);
    } else {
        inheritanceList = curDef.GetSuperTypesRecusively(builder);
    }
    auto targetParent = rawFunc.GetParentCustomTypeDef();
    CJC_ASSERT(!targetParent->IsExtend());
    std::unordered_map<const GenericType*, Type*> replaceTable;
    bool found = false;
    for (auto parent : inheritanceList) {
        if (parent->GetCustomTypeDef() != targetParent) {
            continue;
        }
        std::tie(found, replaceTable) = parent->GetCustomTypeDef()->GetType()->CalculateGenericTyMapping(*parent);
        CJC_ASSERT(found);
        break;
    }
    return replaceTable;
}
} // namespace

void WrapMutFunc::CreateMutFuncWrapper(Function& rawFunc, CustomTypeDef& curDef, ClassType& srcClassTy)
{
    // create the wrapper func
    auto replaceTable = CollectReplaceTable(curDef, rawFunc, builder);

    auto instFuncTy = StaticCast<FuncType*>(ReplaceRawGenericArgType(*rawFunc.GetFuncType(), replaceTable, builder));
    auto wrapperParamsTy = instFuncTy->GetParamTypes();
    auto parentDefType = curDef.GetType();
    wrapperParamsTy[0] = builder.GetType<RefType>(parentDefType);
    auto retTy = instFuncTy->GetReturnType();
    auto wrapperFuncTy = builder.GetType<FuncType>(wrapperParamsTy, retTy);

    auto funcIdentifier = CHIRMangling::GenerateVirtualFuncMangleName(&rawFunc, curDef, &srcClassTy, false);
    auto pkgName = curDef.GetPackageName();

    bool isImported = curDef.TestAttr(Attribute::IMPORTED);
    Function* func = nullptr;
    if (isImported) {
        func = builder.CreateImportedFuncSig(wrapperFuncTy, funcIdentifier, "", "", pkgName);
    } else {
        func = builder.CreateFuncWithBody(INVALID_LOCATION, wrapperFuncTy, funcIdentifier, "", "", pkgName);
    }
    wrapperFuncs.emplace(funcIdentifier, func);
    CJC_NULLPTR_CHECK(func);

    func->Set<WrappedRawMethod>(&rawFunc);
    func->AppendAttributeInfo(rawFunc.GetAttributeInfo());
    func->DisableAttr(Attribute::VIRTUAL);
    func->EnableAttr(Attribute::NO_REFLECT_INFO);
    curDef.AddMethod(func);

    if (isImported) {
        return;
    }

    // create the func body
    BlockGroup* body = builder.CreateBlockGroup(*func);
    func->InitBody(*body);

    std::vector<Value*> args;
    for (auto paramTy : wrapperParamsTy) {
        args.emplace_back(builder.CreateParameter(paramTy, INVALID_LOCATION, *func));
    }

    auto entry = builder.CreateBlock(body);
    body->SetEntryBlock(entry);
    auto ret =
        Cangjie::CHIR::CreateAndAppendExpression<Allocate>(builder, builder.GetType<RefType>(retTy), retTy, entry);
    func->SetReturnValue(*ret->GetResult());

    auto rawFuncFirstArgType = rawFunc.GetFuncType()->GetParamTypes()[0]->StripAllRefs();
    auto firstArgType = GetInstSubType(*rawFuncFirstArgType, srcClassTy, builder);
    if (!firstArgType->IsValueType() || rawFunc.TestAttr(Attribute::MUT)) {
        firstArgType = builder.GetType<RefType>(firstArgType);
    }
    args[0] = Cangjie::CHIR::TypeCastOrBoxIfNeeded(*args[0], *firstArgType, builder, *entry, INVALID_LOCATION);

    auto apply = Cangjie::CHIR::CreateAndAppendExpression<Apply>(builder, retTy, &rawFunc, FuncCallContext{
        .args = args,
        .thisType = curDef.GetType()}, entry);
    Cangjie::CHIR::CreateAndAppendExpression<Store>(
        builder, builder.GetUnitTy(), apply->GetResult(), func->GetReturnValue(), entry);

    auto tempThis =
        Cangjie::CHIR::TypeCastOrBoxIfNeeded(*args[0], *wrapperParamsTy[0], builder, *entry, INVALID_LOCATION);
    auto load = Cangjie::CHIR::CreateAndAppendExpression<Load>(builder, parentDefType, tempThis, entry)->GetResult();
    auto structMemberTypes = StaticCast<StructType*>(parentDefType)->GetInstantiatedMemberTys(builder);

    for (size_t i = 0; i < structMemberTypes.size(); ++i) {
        auto path = std::vector<uint64_t>{i};
        auto field = Cangjie::CHIR::CreateAndAppendExpression<Field>(builder, structMemberTypes[i], load, path, entry)
            ->GetResult();
        Cangjie::CHIR::CreateAndAppendExpression<StoreElementRef>(
            builder, builder.GetUnitTy(), field, func->GetParam(0), path, entry);
    }

    entry->AppendExpression(builder.CreateTerminator<Exit>(entry));
}

void WrapMutFunc::Run(CustomTypeDef& customTypeDef)
{
    Type* structTy = nullptr;
    if (customTypeDef.GetCustomKind() == CustomDefKind::TYPE_EXTEND &&
        StaticCast<ExtendDef>(customTypeDef).GetExtendedType()->IsStruct()) {
        structTy = StaticCast<ExtendDef>(customTypeDef).GetExtendedType();
    } else if (customTypeDef.GetCustomKind() == CustomDefKind::TYPE_STRUCT &&
        StaticCast<StructDef>(customTypeDef).GetImplementedInterfacesNum() > 0) {
        structTy = customTypeDef.GetType();
    }
    if (!structTy) {
        return;
    }
    for (auto& vtableIt : customTypeDef.GetDefVTable().GetTypeVTables()) {
        for (auto& methodInfo : vtableIt.GetVirtualMethods()) {
            if (methodInfo.GetVirtualMethod() == nullptr) {
                continue;
            }
            auto rawFunc = methodInfo.GetVirtualMethod();
            while (auto base = rawFunc->Get<WrappedRawMethod>()) {
                rawFunc = base;
            }
            if (!rawFunc->TestAttr(Attribute::MUT) || rawFunc->GetParentCustomTypeDef() == &customTypeDef) {
                continue;
            }
            if (auto ex = DynamicCast<ExtendDef*>(&customTypeDef);
                ex && ex->GetExtendedCustomTypeDef() == rawFunc->GetParentCustomTypeDef()) {
                continue;
            }
            CreateMutFuncWrapper(*rawFunc, customTypeDef, *vtableIt.GetSrcParentType());
        }
    }
}

WrapMutFunc::WrapMutFunc(CHIRBuilder& b) : builder(b)
{
}

std::unordered_map<std::string, Function*>&& WrapMutFunc::GetWrappers()
{
    return std::move(wrapperFuncs);
}
