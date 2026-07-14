// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/Transformation/PatchCJMPSuperCalls.h"

#include "cangjie/CHIR/IR/Expression/Expression.h"
#include "cangjie/CHIR/IR/Type/ClassDef.h"
#include "cangjie/CHIR/IR/Value/Value.h"
#include "cangjie/CHIR/Utils/CHIRCasting.h"
#include "cangjie/CHIR/Utils/PlatformFFIClass.h"
#include "cangjie/CHIR/Utils/Visitor/Visitor.h"
#include "cangjie/Utils/CastingTemplate.h"
#include "cangjie/Utils/ProfileRecorder.h"

using namespace Cangjie::CHIR;

PatchCJMPSuperCalls::PatchCJMPSuperCalls(Package& pkg, CHIRBuilder& builder)
    : pkg(pkg), builder(builder)
{
}

void PatchCJMPSuperCalls::Run()
{
    Utils::ProfileRecorder recorder("Canonicalization", "PatchCJMPSuperCalls");

    for (auto classDef : pkg.GetAllClassDef()) {
        if (!classDef->TestAttr(Attribute::FROM_COMMON_PART) || !classDef->TestAttr(Attribute::JAVA_IMPL)) {
            continue;
        }

        auto superClass = classDef->GetSuperClassDef();
        if (!superClass) {
            continue;
        }

        if (!IsPlatformMirrorClass(*superClass)) {
            continue;
        }

        auto superClassTy = superClass->GetType();
        if (superClassTy && superClassTy->IsClass()) {
            classDef->SetSuperClassTy(*static_cast<ClassType*>(superClassTy));
        }

        auto methods = classDef->GetMethods();
        for (auto method : methods) {
            if (method && method->IsConstructor() && method->IsFuncWithBody()) {
                PatchConstructorSuperCall(*method, *superClass);
            }
        }
    }
}

Function* PatchCJMPSuperCalls::FindPlatformParentParameterlessConstructor(const ClassDef& platformParent)
{
    auto methods = platformParent.GetMethods();

    for (auto method : methods) {
        if (method && method->IsConstructor() && !method->TestAttr(Attribute::STATIC)) {
            auto params = method->GetParams();
            if (params.size() == 1) {
                return method;
            }
        }
    }

    return nullptr;
}

void PatchCJMPSuperCalls::PatchConstructorSuperCall(Function& constructor, ClassDef& platformParent)
{
    auto platformConstructor = FindPlatformParentParameterlessConstructor(platformParent);
    if (!platformConstructor) {
        return;
    }

    auto body = constructor.GetBody();
    if (!body) {
        return;
    }

    auto entryBlock = body->GetEntryBlock();
    if (!entryBlock) {
        return;
    }

    Apply* existingSuperCall = nullptr;
    for (auto expr : entryBlock->GetExpressions()) {
        if (!expr) {
            continue;
        }
        auto apply = DynamicCast<Apply*>(expr);
        if (apply && apply->IsSuperCall()) {
            existingSuperCall = apply;
            break;
        }
    }

    if (existingSuperCall) {
        RepointExistingSuperCall(*existingSuperCall, *platformConstructor);
        return;
    }

    InsertSuperCall(constructor, *platformConstructor, *entryBlock);
}

void PatchCJMPSuperCalls::RepointExistingSuperCall(Apply& superCall, Function& platformConstructor)
{
    auto callee = superCall.GetCallee();
    if (!callee || !callee->IsFunc()) {
        return;
    }
    auto calleeFunc = StaticCast<Function*>(callee);
    auto calleeParent = calleeFunc->GetParentCustomTypeDef();
    if (!calleeParent || !calleeParent->IsClass()) {
        return;
    }
    auto calleeParentClass = StaticCast<ClassDef*>(calleeParent);
    if (calleeParentClass->GetSuperClassDef() != nullptr) {
        return;
    }
    superCall.ReplaceOperand(static_cast<size_t>(0), StaticCast<Value*>(&platformConstructor));
}

void PatchCJMPSuperCalls::InsertSuperCall(Function& constructor, Function& platformConstructor, Block& entryBlock)
{
    auto thisParam = constructor.GetParams().empty() ? nullptr : constructor.GetParams()[0];
    if (!thisParam) {
        return;
    }

    FuncCallContext callContext {
        .args = {thisParam},
        .instTypeArgs = {},
        .thisType = thisParam->GetType()
    };

    auto loc = constructor.GetDebugLocation();
    auto resultTy = builder.GetUnitTy();
    auto superCall = builder.CreateExpression<Apply>(
        loc, resultTy, StaticCast<Value*>(&platformConstructor), callContext, &entryBlock);
    superCall->SetSuperCall();
    entryBlock.InsertExprIntoHead(*superCall);
}
