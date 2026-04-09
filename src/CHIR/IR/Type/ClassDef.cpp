// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/IR/Type/ClassDef.h"

#include "cangjie/CHIR/Utils/CHIRCasting.h"
#include "cangjie/CHIR/IR/Expression/Terminator.h"
#include "cangjie/CHIR/Utils/ToStringUtils.h"
#include "cangjie/CHIR/IR/Type/Type.h"
#include "cangjie/CHIR/IR/Value/Value.h"
#include "cangjie/Utils/CheckUtils.h"

#include <iostream>
#include <sstream>

using namespace Cangjie::CHIR;

ClassDef::ClassDef(std::string srcCodeIdentifier, std::string identifier,
    std::string pkgName, bool isClass)
    : CustomTypeDef(srcCodeIdentifier, identifier, pkgName, CustomDefKind::TYPE_CLASS), isClass(isClass)
{
}

ClassDef* ClassDef::GetSuperClassDef() const
{
    return superClassTy ? superClassTy->GetClassDef() : nullptr;
}

bool ClassDef::HasSuperClass() const
{
    return GetSuperClassDef() != nullptr;
}

void ClassDef::SetSuperClassTy(ClassType& ty)
{
    superClassTy = &ty;
}

std::string ClassDef::ToString() const
{
    std::stringstream ss;
    PrintAttrAndTitle(ss);
    ss << " {";
    PrintComment(ss);
    ss << "\n";

    PrintLocalVar(ss);
    PrintStaticVar(ss);
    PrintMethod(ss);
    PrintVTable(ss);
    ss << "}";
    return ss.str();
}

bool ClassDef::IsAbstract() const
{
    return TestAttr(CHIR::Attribute::ABSTRACT);
}

bool ClassDef::IsInterface() const
{
    return !isClass;
}

bool ClassDef::IsClass() const
{
    return isClass;
}

void ClassDef::SetAnnotation(bool value)
{
    isAnnotation = value;
}

bool ClassDef::IsAnnotation() const
{
    return isAnnotation;
}

ClassType* ClassDef::GetSuperClassTy() const
{
    return superClassTy;
}

Function* ClassDef::GetFinalizer() const
{
    for (auto m : methods) {
        if (m->GetFuncKind() == FuncKind::FINALIZER) {
            return m;
        }
    }
    return nullptr;
}

void ClassDef::SetType(CustomType& ty)
{
    CJC_ASSERT(ty.GetTypeKind() == Type::TypeKind::TYPE_CLASS);
    type = &ty;
}

ClassType* ClassDef::GetType() const
{
    return StaticCast<ClassType>(type);
}

void ClassDef::PrintComment(std::stringstream& ss) const
{
    CustomTypeDef::PrintComment(ss);
    AddCommaOrNot(ss);
    if (ss.str().empty()) {
        ss << " // ";
    }
    ss << "isAnnotation: " << BoolToString(isAnnotation);
}