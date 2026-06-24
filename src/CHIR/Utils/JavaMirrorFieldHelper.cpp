// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/Utils/JavaMirrorFieldHelper.h"
#include "cangjie/CHIR/IR/Type/ClassDef.h"

namespace Cangjie::CHIR {

constexpr size_t SETTER_PARAM_MIN_COUNT = 2;

bool IsJavaMirrorField(const MemberVarInfo& field, Attribute mirrorAttr)
{
    if (field.TestAttr(Attribute::COMPILER_ADD)) {
        return false;
    }
    return field.TestAttr(mirrorAttr);
}

Function* FindGetterForField(const ClassDef& def, const MemberVarInfo& field, Attribute mirrorAttr)
{
    if (!IsJavaMirrorField(field, mirrorAttr)) {
        return nullptr;
    }

    for (auto method : def.GetMethods()) {
        if (method->GetFuncKind() != FuncKind::GETTER) {
            continue;
        }
        if (!method->TestAttr(mirrorAttr)) {
            continue;
        }
        auto retType = method->GetReturnType();
        if (retType && field.type && *retType == *field.type) {
            return method;
        }
    }
    return nullptr;
}

Function* FindSetterForField(const ClassDef& def, const MemberVarInfo& field, Attribute mirrorAttr)
{
    if (!IsJavaMirrorField(field, mirrorAttr)) {
        return nullptr;
    }

    for (auto method : def.GetMethods()) {
        if (method->GetFuncKind() != FuncKind::SETTER) {
            continue;
        }
        if (!method->TestAttr(mirrorAttr)) {
            continue;
        }
        auto params = method->GetParams();
        if (params.size() < SETTER_PARAM_MIN_COUNT) {
            continue;
        }
        auto paramType = params[1]->GetType();
        if (paramType && field.type && *paramType == *field.type) {
            return method;
        }
    }
    return nullptr;
}

Function* FindStaticGetterForGlobalVar(const ClassDef& def, const GlobalVar& globalVar)
{
    auto varSrcCodeId = globalVar.GetSrcCodeIdentifier();

    for (auto method : def.GetMethods()) {
        if (method->GetFuncKind() != FuncKind::GETTER) {
            continue;
        }
        if (method->GetParams().size() != 0) {
            continue;
        }
        auto methodRawMangled = method->GetRawMangledName();
        auto methodSrcCodeId = method->GetSrcCodeIdentifier();
        if (methodRawMangled.find(varSrcCodeId) == std::string::npos &&
            methodSrcCodeId.find(varSrcCodeId) == std::string::npos) {
            continue;
        }
        return method;
    }
    return nullptr;
}

Function* FindStaticSetterForGlobalVar(const ClassDef& def, const GlobalVar& globalVar)
{
    auto varSrcCodeId = globalVar.GetSrcCodeIdentifier();

    for (auto method : def.GetMethods()) {
        if (method->GetFuncKind() != FuncKind::SETTER) {
            continue;
        }
        auto params = method->GetParams();
        if (params.size() != 1) {
            continue;
        }
        auto methodRawMangled = method->GetRawMangledName();
        auto methodSrcCodeId = method->GetSrcCodeIdentifier();
        if (methodRawMangled.find(varSrcCodeId) == std::string::npos &&
            methodSrcCodeId.find(varSrcCodeId) == std::string::npos) {
            continue;
        }
        return method;
    }
    return nullptr;
}

Function* FindInstanceGetterByName(const ClassDef& def, const std::string& fieldName)
{
    for (auto method : def.GetMethods()) {
        if (method->GetFuncKind() != FuncKind::GETTER) {
            continue;
        }
        if (!method->IsMemberFunc()) {
            continue;
        }
        auto methodParams = method->GetParams();
        if (methodParams.size() < 1) {
            continue;
        }
        auto methodSrcCodeId = method->GetSrcCodeIdentifier();
        auto methodRawMangled = method->GetRawMangledName();
        if (methodSrcCodeId.find(fieldName) == std::string::npos &&
            methodRawMangled.find(fieldName) == std::string::npos) {
            continue;
        }
        return method;
    }
    return nullptr;
}

Function* FindInstanceSetterByName(const ClassDef& def, const std::string& fieldName)
{
    for (auto method : def.GetMethods()) {
        if (method->GetFuncKind() != FuncKind::SETTER) {
            continue;
        }
        if (!method->IsMemberFunc()) {
            continue;
        }
        auto methodParams = method->GetParams();
        if (methodParams.size() < SETTER_PARAM_MIN_COUNT) {
            continue;
        }
        auto methodSrcCodeId = method->GetSrcCodeIdentifier();
        auto methodRawMangled = method->GetRawMangledName();
        if (methodSrcCodeId.find(fieldName) == std::string::npos &&
            methodRawMangled.find(fieldName) == std::string::npos) {
            continue;
        }
        return method;
    }
    return nullptr;
}

} // namespace Cangjie::CHIR