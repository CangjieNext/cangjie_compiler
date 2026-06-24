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
#include "cangjie/CHIR/Utils/Visitor/Visitor.h"
#include "cangjie/Utils/CastingTemplate.h"
#include "cangjie/Utils/ProfileRecorder.h"

using namespace Cangjie::CHIR;

namespace {
bool IsJavaMirror(const ClassDef& classDef)
{
    return classDef.TestAttr(Attribute::JAVA_MIRROR);
}

bool IsObjCMirror(const ClassDef& classDef)
{
    return classDef.TestAttr(Attribute::OBJ_C_MIRROR);
}

bool IsPlatformMirrorClass(const ClassDef& classDef)
{
    return IsJavaMirror(classDef) || IsObjCMirror(classDef);
}
}

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

Function* PatchCJMPSuperCalls::FindPlatformParentEmptyConstructor(const ClassDef& platformParent)
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
    auto platformConstructor = FindPlatformParentEmptyConstructor(platformParent);
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
        if (!expr) continue;
        if (auto apply = DynamicCast<Apply*>(expr)) {
            if (apply->IsSuperCall()) {
                existingSuperCall = apply;
                break;
            }
        }
    }

    if (existingSuperCall) {
        auto callee = existingSuperCall->GetCallee();
        if (callee && callee->IsFunc()) {
            auto calleeFunc = StaticCast<Function*>(callee);
            auto calleeParent = calleeFunc->GetParentCustomTypeDef();
            if (calleeParent && calleeParent->IsClass()) {
                auto calleeParentClass = StaticCast<ClassDef*>(calleeParent);
                if (calleeParentClass->GetSuperClassDef() == nullptr) {
                    existingSuperCall->ReplaceOperand(static_cast<size_t>(0), StaticCast<Value*>(platformConstructor));
                }
            }
        }
        return;
    }
    
    auto thisParam = constructor.GetParams().empty() ? nullptr : constructor.GetParams()[0];
    if (!thisParam) {
        return;
    }

    FuncCallContext callContext {
        .args = {thisParam},
        .instTypeArgs = {},
        .thisType = thisParam->GetType()
    };

    auto resultTy = builder.GetUnitTy();
    auto superCall = builder.CreateExpression<Apply>(
        resultTy, StaticCast<Value*>(platformConstructor), callContext, entryBlock);
    superCall->SetSuperCall();

    entryBlock->InsertExprIntoHead(*superCall);
}
