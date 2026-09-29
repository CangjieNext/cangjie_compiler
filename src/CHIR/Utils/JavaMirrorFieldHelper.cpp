// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/Utils/JavaMirrorFieldHelper.h"
#include "cangjie/CHIR/IR/Type/ClassDef.h"

namespace Cangjie::CHIR {

bool IsJavaMirrorField(const MemberVarInfo& field, Attribute mirrorAttr)
{
    if (field.TestAttr(Attribute::COMPILER_ADD)) {
        return false;
    }
    return field.TestAttr(mirrorAttr);
}

bool IsPatchableMirrorField(const MemberVarInfo& field)
{
    if (field.TestAttr(Attribute::COMPILER_ADD)) {
        return false;
    }
    if (field.TestAttr(Attribute::HAS_INITED_FIELD)) {
        return false;
    }
    if (field.name.empty() || field.name.front() == '$') {
        return false;
    }
    return true;
}

bool IsDirectPatchableMirrorField(const ClassDef& def, const std::string& name)
{
    for (const auto& var : def.GetDirectInstanceVars()) {
        if (var.name == name && IsPatchableMirrorField(var)) {
            return true;
        }
    }
    return false;
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
        constexpr size_t minParams = 2;
        if (params.size() < minParams) {
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
    const auto expected = "$" + globalVar.GetSrcCodeIdentifier() + "get";

    for (auto method : def.GetMethods()) {
        if (method->GetFuncKind() != FuncKind::GETTER) {
            continue;
        }
        if (method->GetParams().size() != 0) {
            continue;
        }
        if (method->GetSrcCodeIdentifier() == expected) {
            return method;
        }
    }
    return nullptr;
}

Function* FindStaticSetterForGlobalVar(const ClassDef& def, const GlobalVar& globalVar)
{
    const auto expected = "$" + globalVar.GetSrcCodeIdentifier() + "set";

    for (auto method : def.GetMethods()) {
        if (method->GetFuncKind() != FuncKind::SETTER) {
            continue;
        }
        auto params = method->GetParams();
        if (params.size() != 1) {
            continue;
        }
        if (method->GetSrcCodeIdentifier() == expected) {
            return method;
        }
    }
    return nullptr;
}

Function* FindInstanceGetterByName(const ClassDef& def, const std::string& fieldName)
{
    const auto expected = "$" + fieldName + "get";

    for (auto cur = &def; cur != nullptr; cur = cur->GetSuperClassDef()) {
        for (auto method : cur->GetMethods()) {
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
            if (method->GetSrcCodeIdentifier() == expected) {
                return method;
            }
        }
    }
    return nullptr;
}

Function* FindInstanceSetterByName(const ClassDef& def, const std::string& fieldName)
{
    const auto expected = "$" + fieldName + "set";

    for (auto cur = &def; cur != nullptr; cur = cur->GetSuperClassDef()) {
        for (auto method : cur->GetMethods()) {
            if (method->GetFuncKind() != FuncKind::SETTER) {
                continue;
            }
            if (!method->IsMemberFunc()) {
                continue;
            }
            auto methodParams = method->GetParams();
            constexpr size_t minParams = 2;
            if (methodParams.size() < minParams) {
                continue;
            }
            if (method->GetSrcCodeIdentifier() == expected) {
                return method;
            }
        }
    }
    return nullptr;
}

} // namespace Cangjie::CHIR