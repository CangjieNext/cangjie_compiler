// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/Checker/AnnotationMetadata.h"
#include "cangjie/AST/Utils.h"
#include "cangjie/AST/ASTCasting.h"
#include "cangjie/CHIR/Utils/CHIRCasting.h"
#include "cangjie/CHIR/IR/Expression/Expression.h"
#include "cangjie/CHIR/IR/Type/CustomTypeDef.h"
#include "cangjie/CHIR/IR/Type/ClassDef.h"

namespace Cangjie::CHIR {
namespace {
const Allocate* FindInstanceAllocation(const GlobalVar& instance)
{
    const Value* stored = nullptr;
    for (auto user : instance.GetUsers()) {
        auto store = DynamicCast<Store>(user);
        if (!store || store->GetLocation() != &instance) {
            continue;
        }
        // A folded initializer has one assignment. More than one is not a
        // scalar snapshot, regardless of the users' iteration order.
        if (stored) {
            return nullptr;
        }
        stored = store->GetValue();
    }
    auto local = DynamicCast<LocalVar>(stored);
    if (!local) {
        return nullptr;
    }
    return DynamicCast<Allocate>(local->GetExpr());
}

const LiteralValue* GetScalarLiteral(const Value* value)
{
    if (auto literal = DynamicCast<LiteralValue>(value)) {
        return literal;
    }
    if (auto local = DynamicCast<LocalVar>(value)) {
        if (auto constant = DynamicCast<Constant>(local->GetExpr())) {
            return constant->GetValue();
        }
    }
    return nullptr;
}

std::optional<EvaluatedAnnotationField> ReadScalarField(
    const Value* value, const std::string& name, const Type& type)
{
    const auto literal = GetScalarLiteral(value);
    if (auto str = DynamicCast<StringLiteral>(literal); str && type.IsString()) {
        return EvaluatedAnnotationField{name, str->GetVal()};
    }
    if (auto boolean = DynamicCast<BoolLiteral>(literal); boolean && type.IsBoolean()) {
        return EvaluatedAnnotationField{name, boolean->GetVal()};
    }
    // Do not recursively interpret arbitrary business annotation object
    // graphs. Preserve their identity, but mark scalar fields unavailable.
    return std::nullopt;
}

std::string QualifiedAnnotationName(const AST::Decl& annotationClass)
{
    std::vector<std::string> names;
    for (auto decl = &annotationClass; decl; decl = decl->outerDecl) {
        names.emplace_back(decl->identifier.Val());
    }
    auto result = annotationClass.fullPackageName;
    for (auto name = names.rbegin(); name != names.rend(); ++name) {
        result += "." + *name;
    }
    return result;
}

std::optional<ResolvedAnnotationData> ReadResolvedAnnotation(const AST::Node* node, const GlobalVar& instance,
    const std::unordered_map<unsigned, std::string>& fileNames, bool scalarsEvaluated)
{
    const auto call = DynamicCast<AST::CallExpr>(node);
    if (!call || !call->resolvedFunction || !call->resolvedFunction->funcBody) {
        return std::nullopt;
    }
    const auto type = call->resolvedFunction->funcBody->parentClassLike;
    if (!type || type->fullPackageName.empty() || type->mangledName.empty()) {
        return std::nullopt;
    }
    const auto file = fileNames.find(call->begin.fileID);
    ResolvedAnnotationData item;
    item.qualifiedClassName = QualifiedAnnotationName(*type);
    item.declarationSymbol = type->mangledName;
    item.sourceFile = file == fileNames.end() ? "" : file->second;
    item.fileId = call->begin.fileID;
    item.beginLine = call->begin.line > 0 ? static_cast<unsigned>(call->begin.line) : 0;
    item.beginColumn = call->begin.column > 0 ? static_cast<unsigned>(call->begin.column) : 0;
    item.endLine = call->end.line > 0 ? static_cast<unsigned>(call->end.line) : 0;
    item.endColumn = call->end.column > 0 ? static_cast<unsigned>(call->end.column) : 0;
    item.scalarFields = scalarsEvaluated ? TryReadAnnotationScalarFields(instance) : std::nullopt;
    return item;
}
} // namespace

std::optional<std::vector<EvaluatedAnnotationField>> TryReadAnnotationScalarFields(const GlobalVar& instance)
{
    const auto allocation = FindInstanceAllocation(instance);
    if (!allocation) {
        return std::nullopt;
    }
    auto type = DynamicCast<ClassType>(allocation->GetType()->StripAllRefs());
    if (!type) {
        return std::nullopt;
    }
    const auto fields = type->GetClassDef()->GetAllInstanceVars();
    std::vector<std::optional<EvaluatedAnnotationField>> values(fields.size());
    std::unordered_set<std::string> names;
    for (const auto& field : fields) {
        if (!field.type || (!field.type->IsString() && !field.type->IsBoolean()) ||
            field.name.empty() || !names.emplace(field.name).second) {
            return std::nullopt;
        }
    }
    for (auto user : allocation->GetResult()->GetUsers()) {
        auto store = DynamicCast<StoreElementRef>(user);
        if (!store || store->GetBase() != allocation->GetResult()) {
            continue;
        }
        const auto& path = store->GetPath();
        if (path.size() != 1 || path.front() >= fields.size() || values[path.front()].has_value()) {
            return std::nullopt;
        }
        const auto index = path.front();
        values[index] = ReadScalarField(store->GetValue(), fields[index].name, *fields[index].type);
        if (!values[index]) {
            return std::nullopt;
        }
    }
    std::vector<EvaluatedAnnotationField> result;
    for (auto& value : values) {
        if (!value) {
            return std::nullopt;
        }
        result.emplace_back(std::move(*value));
    }
    return result;
}

std::optional<ResolvedAnnotationMap> ReadResolvedAnnotationMetadata(
    const std::vector<std::pair<const AST::Decl*, Function*>>& factories,
    const std::unordered_map<unsigned, std::string>& fileNames, bool scalarsEvaluated)
{
    ResolvedAnnotationMap result;
    for (const auto& [decl, factory] : factories) {
        if (!decl || !factory || !decl->annotationsArray) {
            return std::nullopt;
        }
        const auto& children = decl->annotationsArray->children;
        const auto& instances = factory->Get<AnnoFactoryInfo>();
        if (children.size() != instances.size()) {
            return std::nullopt;
        }
        std::vector<ResolvedAnnotationData> annotations;
        for (size_t i = 0; i < children.size(); ++i) {
            if (!instances[i]) {
                return std::nullopt;
            }
            auto annotation =
                ReadResolvedAnnotation(children[i].get().get(), *instances[i], fileNames, scalarsEvaluated);
            if (!annotation) {
                return std::nullopt;
            }
            annotations.emplace_back(std::move(*annotation));
        }
        if (!result.emplace(decl, std::move(annotations)).second) {
            return std::nullopt;
        }
    }
    return result;
}
} // namespace Cangjie::CHIR
