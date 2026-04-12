// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/Checker/AnnotationChecker.h"

#include "cangjie/CHIR/Analysis/Utils.h"
#include "cangjie/CHIR/Utils/CHIRCasting.h"
#include "cangjie/Utils/CastingTemplate.h"

using namespace Cangjie::CHIR;

AnnotationChecker::AnnotationChecker(const Package& pkg, DiagnosticEngine& diag)
    : pkg(pkg), diag(diag)
{
    targetToErrMsg.emplace("Type", "type");
    targetToErrMsg.emplace("Parameter", "parameter");
    targetToErrMsg.emplace("Init", "init");
    targetToErrMsg.emplace("MemberProperty", "member property");
    targetToErrMsg.emplace("MemberFunction", "member function");
    targetToErrMsg.emplace("MemberVariable", "member variable");
    targetToErrMsg.emplace("EnumConstructor", "enum constructor");
    targetToErrMsg.emplace("GlobalFunction", "global function");
    targetToErrMsg.emplace("GlobalVariable", "global variable");
    targetToErrMsg.emplace("Extension", "extend");
}

bool AnnotationChecker::Run()
{
    CollectAnnotationTargets();
    CheckAnnotationTargets();
    return diag.GetErrorCount() == 0;
}

void AnnotationChecker::CollectAnnotationTargets()
{
    for (auto def : pkg.GetClasses()) {
        if (!def->IsAnnotation()) {
            continue;
        }
        auto targets = def->GetAnnotationTargets();
        if (targets.empty()) {
            annotationTargets.emplace(def->GetSrcCodeIdentifier(), std::unordered_set<std::string>{});
            continue;
        }
        std::unordered_set<std::string> result;
        for (auto target : targets) {
            result.emplace(CalculateTarget(*target));
        }
        annotationTargets.emplace(def->GetSrcCodeIdentifier(), result);
    }
}

std::string AnnotationChecker::CalculateTarget(const GlobalVar& var)
{
    auto users = var.GetUsers();
    CJC_ASSERT(users.size() == 1);
    auto store = StaticCast<Store*>(users[0]);
    auto annotationKindDef = StaticCast<EnumType*>(store->GetValue()->GetType())->GetEnumDef();
    CJC_ASSERT(annotationKindDef->GetSrcCodeIdentifier() == "AnnotationKind");
    CJC_ASSERT(annotationKindDef->GetPackageName() == "std.core");
    auto typecast = StaticCast<TypeCast*>(StaticCast<LocalVar*>(store->GetValue())->GetExpr());
    auto constExpr = StaticCast<Constant*>(StaticCast<LocalVar*>(typecast->GetSourceValue())->GetExpr());
    auto constValue = constExpr->GetValue();
    CJC_ASSERT(constValue->GetType()->GetTypeKind() == Type::TypeKind::TYPE_UINT32);
    auto enumCtorIdx = StaticCast<IntLiteral*>(constValue)->GetUnsignedVal();
    return annotationKindDef->GetCtor(enumCtorIdx).name;
}

void AnnotationChecker::CheckAnnotationTargets()
{
    for (auto def : pkg.GetCurPkgCustomTypeDef()) {
        if (def->IsExtend()) {
            CheckTargetsOnDef(*def, "Extension");
        } else {
            CheckTargetsOnDef(*def, "Type");
        }
    }
    for (auto var : pkg.GetGlobalVarsWithInit(false)) {
        CheckTargetsOnGlobalVar(*var);
    }
    for (auto func : pkg.GetGlobalFuncsWithBody()) {
        CheckTargetsOnGlobalFunc(*func);
    }
}

void AnnotationChecker::CheckTargetsOnGlobalVar(const GlobalVar& var)
{
    if (var.GetParentCustomTypeDef() != nullptr) {
        CheckTargets(var.GetAnnoInfo(), "MemberVariable");
    } else {
        CheckTargets(var.GetAnnoInfo(), "GlobalVariable");
    }
}

void AnnotationChecker::CheckTargetsOnGlobalFunc(const Function& func)
{
    if (func.GetParentCustomTypeDef() != nullptr) {
        if (func.IsConstructor()) {
            CheckTargets(func.GetAnnoInfo(), "Init");
        } else if (func.GetFuncKind() == FuncKind::GETTER || func.GetFuncKind() == FuncKind::SETTER) {
            CheckTargets(func.GetAnnoInfo(), "MemberProperty");
        } else {
            CheckTargets(func.GetAnnoInfo(), "MemberFunction");
        }
    } else {
        CheckTargets(func.GetAnnoInfo(), "GlobalFunction");
    }
    for (auto param : func.GetParams()) {
        CheckTargets(param->GetAnnoInfo(), "Parameter");
    }
}

void AnnotationChecker::CheckTargets(const AnnoInfo& annoInfo, const std::string& target)
{
    for (const auto& annoPair : annoInfo.annoPairs) {
        auto annoClassName = annoPair.annoClassName;
        auto it = annotationTargets.find(annoClassName);
        CJC_ASSERT(it != annotationTargets.end());
        if (it->second.empty()) {
            continue;
        }
        if (it->second.find(target) != it->second.end()) {
            continue;
        }
        auto [_, loc] = ToRangeIfNotZero(annoPair.loc);
        diag.DiagnoseRefactor(
            DiagKindRefactor::chir_annotation_not_applicable, loc, annoClassName, targetToErrMsg.at(target));
    }
}

void AnnotationChecker::CheckTargetsOnDef(const CustomTypeDef& def, const std::string& defTarget)
{
    CheckTargets(def.GetAnnoInfo(), defTarget);
    for (const auto& memberVar : def.GetDirectInstanceVars()) {
        CheckTargets(memberVar.annoInfo, "MemberVariable");
    }
    if (auto enumDef = DynamicCast<const EnumDef*>(&def)) {
        for (const auto& ctor : enumDef->GetCtors()) {
            CheckTargets(ctor.annoInfo, "EnumConstructor");
        }
    }
}