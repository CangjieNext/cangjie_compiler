// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// Licensed under Apache-2.0 with Runtime Library Exception.
#include "TypeCheckerImpl.h"
#include "Collector.h"
#include "cangjie/AST/Clone.h"
#include "cangjie/AST/Create.h"
#include "cangjie/AST/Utils.h"
#include "cangjie/Parse/Parser.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <unordered_map>
#include <unordered_set>

using namespace Cangjie;
using namespace Cangjie::AST;

namespace {
constexpr auto CONTEXT_PACKAGE = "cj_arkts_context_v1";

std::string ToJS(const std::string& type, const std::string& context, const std::string& argument)
{
    std::string value = argument;
    std::string method = "number";
    if (type == "String") {
        method = "string";
    } else if (type == "Bool") {
        method = "boolean";
    } else if (type == "Int64" || type == "UInt64" || type == "BigInt") {
        method = "bigint";
        if (type == "UInt64") {
            value = "__cj_arkts_BigInt.parse(" + argument + ".toString())";
        }
    } else if (type == "Unit") {
        return context + ".undefined().toJSValue()";
    } else if (type == "Rune") {
        value = "Float64(UInt32(" + argument + "))";
    } else if (type != "Float64") {
        value = "Float64(" + argument + ")";
    }
    return context + "." + method + "(" + value + ").toJSValue()";
}

std::string FromJS(const std::string& type, const std::string& value,
    const std::string& number, const std::string& result)
{
    auto failure = [&](const std::string& reason) {
        return "throw ArkTSConversionException(\"" + reason + "\", \"" + type + "\")\n";
    };
    if (type == "Nothing") {
        return "throw Exception(\"ArkTS never function returned\")\n";
    }
    if (type == "Unit") {
        return "if (" + value + ".isNull()) { return }\n" + value + ".asUndefined()\nreturn\n";
    }
    if (type == "String" || type == "Bool") {
        auto predicate = type == "String" ? "isString" : "isBoolean";
        auto conversion = type == "String" ? "asString().toString()" : "toBoolean()";
        return "if (!" + value + "." + predicate + "()) { " + failure("TypeMismatch") +
            "}\nreturn " + value + "." + conversion + "\n";
    }
    if (type == "BigInt" || type == "Int64" || type == "UInt64") {
        std::string code = "if (!" + value + ".isBigInt()) { " + failure("TypeMismatch") +
            "}\nlet " + number + " = " + value + ".asBigInt().toBigInt()\n";
        if (type == "BigInt") {
            return code + "return " + number + "\n";
        }
        auto minimum = type == "Int64" ? "-9223372036854775808" : "0";
        auto maximum = type == "Int64" ? "9223372036854775807" : "18446744073709551615";
        code += "if (" + number + " < __cj_arkts_BigInt.parse(\"" + minimum + "\") || " +
            number + " > __cj_arkts_BigInt.parse(\"" + maximum + "\")) { " + failure("OutOfRange") + "}\n";
        return code + "return " + type + ".parse(" + number + ".toString())\n";
    }
    std::string code = "if (!" + value + ".isNumber()) { " + failure("TypeMismatch") +
        "}\nlet " + number + " = " + value + ".toNumber()\n";
    if (type == "Float64") {
        return code + "return " + number + "\n";
    }
    if (type == "Float16" || type == "Float32") {
        return code + "let " + result + " = " + type + "(" + number + ")\nif (!" + number +
            ".isInf() && " + result + ".isInf()) { " + failure("OutOfRange") +
            "}\nreturn " + result + "\n";
    }
    code += "if (" + number + " != " + number + " || " + number + ".isInf()) { " +
        failure("NonFinite") + "}\n";
    static const std::unordered_map<std::string, std::pair<std::string, std::string>> bounds{
        {"Int8", {"-128.0", "127.0"}}, {"Int16", {"-32768.0", "32767.0"}},
        {"Int32", {"-2147483648.0", "2147483647.0"}}, {"UInt8", {"0.0", "255.0"}},
        {"UInt16", {"0.0", "65535.0"}}, {"UInt32", {"0.0", "4294967295.0"}},
        {"Rune", {"0.0", "1114111.0"}}};
    auto range = bounds.at(type);
    code += "if (" + number + " < " + range.first + " || " + number + " > " + range.second + ") { " +
        failure(type == "Rune" ? "InvalidCodePoint" : "OutOfRange") + "}\n";
    code += "if (" + number + " != Float64(Int64(" + number + "))) { " + failure("NotInteger") + "}\n";
    if (type == "Rune") {
        return code + "if (" + number + " >= 55296.0 && " + number + " <= 57343.0) { " +
            failure("InvalidCodePoint") + "}\nreturn Rune(UInt32(" + number + "))\n";
    }
    return code + "return " + type + "(" + number + ")\n";
}

std::string ScalarName(Ptr<Ty> ty)
{
    if (!Ty::IsTyCorrect(ty)) {
        return {};
    }
    if (ty->IsString()) {
        return "String";
    }
    if (ty->IsRune()) {
        return "Rune";
    }
    if (ty->IsInteger() || ty->IsFloating() || ty->IsBoolean() || ty->IsUnitOrNothing()) {
        auto name = Ty::ToString(ty);
        static const std::unordered_set<std::string> supported{
            "Int8", "Int16", "Int32", "Int64", "UInt8", "UInt16", "UInt32", "UInt64",
            "Float16", "Float32", "Float64", "Bool", "Unit", "Nothing"};
        return supported.count(name) ? name : std::string{};
    }
    auto decl = Ty::GetDeclPtrOfTy(ty);
    if (decl && decl->fullPackageName == "std.math.numeric" && decl->identifier == "BigInt") {
        return "BigInt";
    }
    return {};
}

std::string OptionValue(const Decl& decl, const std::string& name, const std::string& fallback)
{
    for (auto& anno : decl.annotations) {
        if (anno->kind != AnnotationKind::ARKTS) {
            continue;
        }
        for (auto& arg : anno->args) {
            if (arg->name == name) {
                auto literal = DynamicCast<LitConstExpr*>(arg->expr.get());
                return literal ? literal->stringValue : fallback;
            }
        }
    }
    return fallback;
}

// Normalize the stored numeric literal text into a Float64 literal so the
// generated glue never depends on integer-literal typing (e.g. `10` -> `10.0`).
std::string NumericLiteralText(std::string text)
{
    if (text.find('.') == std::string::npos && text.find('e') == std::string::npos &&
        text.find('E') == std::string::npos) {
        text += ".0";
    }
    return text;
}

// Render the auto-increment counter of a member that omits @ForeignValue.
std::string FormatEnumCounter(double value)
{
    if (std::floor(value) == value && std::fabs(value) < 1.0e15) {
        return std::to_string(static_cast<long long>(value));
    }
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.17g", value);
    return buffer;
}

// A resolved ArkTS enum member: constructor name plus its ArkTS-side value.
struct EnumValue {
    std::string ctor;
    bool isString;
    std::string value;
};

// Encode an enum-typed value: an exhaustive match over the enum constructors,
// each arm producing the JSValue for the member's ArkTS value. \p keyPrefix
// namespaces the literal placeholders so several enums in one signature never
// overwrite each other's string literals.
std::string EnumToJS(const std::string& name, const std::vector<EnumValue>& members, const std::string& context,
    const std::string& argument, const std::string& keyPrefix,
    std::unordered_map<std::string, std::string>& literals)
{
    std::string code = "match(" + argument + ") {";
    size_t index = 0;
    for (const auto& member : members) {
        code += " case " + name + "." + member.ctor + " => ";
        if (member.isString) {
            auto key = keyPrefix + "_to_str_" + std::to_string(index++) + "__";
            literals[key] = member.value;
            code += context + ".string(\"" + key + "\").toJSValue()";
        } else {
            code += context + ".number(" + NumericLiteralText(member.value) + ").toJSValue()";
        }
    }
    return code + " }";
}

// Decode a JSValue into an enum: members are probed in declaration order so
// the first matching constructor wins (duplicate ArkTS values resolve to the
// earliest declared member), mirroring ArkTS reverse mappings.
std::string EnumFromJS(const std::string& name, const std::string& foreignName, const std::vector<EnumValue>& members,
    const std::string& value, const std::string& number, const std::string& str, const std::string& keyPrefix,
    std::unordered_map<std::string, std::string>& literals)
{
    literals[keyPrefix + "_target__"] = foreignName;
    auto failure = "throw ArkTSConversionException(\"TypeMismatch\", \"" + keyPrefix + "_target__\")\n";
    std::string code = "if (" + value + ".isNumber()) {\nlet " + number + " = " + value + ".toNumber()\n";
    for (const auto& member : members) {
        if (!member.isString) {
            code += "if (" + number + " == " + NumericLiteralText(member.value) + ") { return " + name + "." +
                member.ctor + " }\n";
        }
    }
    code += "}\nif (" + value + ".isString()) {\nlet " + str + " = " + value + ".asString().toString()\n";
    size_t index = 0;
    for (const auto& member : members) {
        if (member.isString) {
            auto key = keyPrefix + "_from_str_" + std::to_string(index++) + "__";
            literals[key] = member.value;
            code += "if (" + str + " == \"" + key + "\") { return " + name + "." + member.ctor + " }\n";
        }
    }
    return code + "}\n" + failure;
}

// A resolved foreign-function signature slot: the supported scalar name, or
// an empty scalar with `enumeration` set when the type is a registered
// `foreign enum` convertible through its @ForeignValue mapping.
struct SigSlot {
    std::string scalar;
    const EnumDecl* enumeration{nullptr};
};
}

void TypeChecker::TypeCheckerImpl::PrepareArkTSFunctions(Package& pkg)
{
    if (pkg.TestAttr(Attribute::IMPORTED)) {
        return;
    }
    for (auto& file : pkg.files) {
        for (auto& decl : file->decls) {
            auto fd = DynamicCast<FuncDecl*>(decl.get());
            if (!fd || !fd->HasAnno(AnnotationKind::ARKTS) || !fd->TestAttr(Attribute::FOREIGN) ||
                fd->TestAttr(Attribute::PRIVATE) || !fd->funcBody || fd->funcBody->body || fd->GetGeneric()) {
                continue;
            }
            bool unsupported = false;
            for (auto& list : fd->funcBody->paramLists) {
                unsupported = unsupported || list->hasVariableLenArg;
                for (auto& param : list->params) {
                    unsupported = unsupported || param->isNamedParam || param->assignment != nullptr;
                }
            }
            if (unsupported) {
                continue;
            }
            // Resolve this signature as CJ, not C. Keep its annotation and a
            // serialized marker so imported callers retain the binding identity.
            fd->EnableAttr(Attribute::ARKTS_LOWERED);
            fd->DisableAttr(Attribute::FOREIGN, Attribute::C, Attribute::NO_MANGLE);
            fd->funcBody->DisableAttr(Attribute::C);
            for (auto it = fd->modifiers.begin(); it != fd->modifiers.end();) {
                if (it->modifier == TokenKind::FOREIGN) {
                    it = fd->modifiers.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }
}

void TypeChecker::TypeCheckerImpl::LowerArkTSHandwrittenFunctions(ASTContext& ctx)
{
    if (ctx.curPackage->TestAttr(Attribute::IMPORTED)) {
        return;
    }
    for (auto& file : ctx.curPackage->files) {
        for (auto& decl : file->decls) {
            auto fd = DynamicCast<FuncDecl*>(decl.get());
            if (!fd || !fd->TestAttr(Attribute::ARKTS_LOWERED) || fd->funcBody->body) {
                continue;
            }
            auto& fb = *fd->funcBody;
            // ResolveDecls creates declaration types, but does not populate the
            // AST types of function signatures. Resolve those through the same
            // semantic entry point used by pre-checking aliases and FFI types.
            auto resolveType = [&](Ptr<Type> type) {
                SigSlot slot;
                if (!type) {
                    return slot;
                }
                SetTypeTy(ctx, *type); // Also substitutes aliases and preserves aliasTy for cjo.
                slot.scalar = ScalarName(type->GetTy());
                if (!slot.scalar.empty()) {
                    return slot;
                }
                // Beyond scalars only registered `foreign enum`s convert: their
                // constructors map to ArkTS values through @ForeignValue.
                auto ty = type->GetTy();
                if (Ty::IsTyCorrect(ty)) {
                    auto enumeration = DynamicCast<EnumDecl*>(Ty::GetDeclPtrOfTy(ty));
                    if (enumeration != nullptr && arkTSEnums.count(enumeration) != 0) {
                        slot.enumeration = enumeration;
                    }
                }
                return slot;
            };
            auto ret = resolveType(fb.retType.get());
            std::vector<Ptr<FuncParam>> params;
            std::vector<SigSlot> types;
            bool supported = !ret.scalar.empty() || ret.enumeration != nullptr;
            bool explicitThis = false;
            for (auto& list : fb.paramLists) {
                for (auto& param : list->params) {
                    auto slot = resolveType(param->type.get());
                    supported = supported &&
                        ((!slot.scalar.empty() && slot.scalar != "Nothing") || slot.enumeration != nullptr);
                    params.push_back(param.get());
                    types.push_back(slot);
                    explicitThis = explicitThis || param->identifier == "this";
                }
            }
            if (explicitThis) {
                diag.DiagnoseRefactor(DiagKindRefactor::sema_arkts_unsupported_signature, *fd,
                    "explicit this parameter");
                continue;
            }
            if (!supported) {
                // Keep the marker to prevent generic C/annotation diagnostics from
                // obscuring the actual unsupported ArkTS signature. Compilation
                // stops on this error; no unsupported wrapper is emitted.
                diag.DiagnoseRefactor(DiagKindRefactor::sema_arkts_unsupported_signature, *fd,
                    "parameter or return type is outside the scalar mapping");
                continue;
            }
            if (!importManager.GetImportedDecl(CONTEXT_PACKAGE, "__cj_arkts_current")) {
                fd->DisableAttr(Attribute::ARKTS_LOWERED);
                diag.DiagnoseRefactor(DiagKindRefactor::sema_arkts_bridge_pending, *fd);
                continue;
            }
            std::unordered_set<std::string> allocatedNames;
            for (auto param : params) {
                allocatedNames.emplace(param->identifier.Val());
            }
            auto freshName = [&](std::string name) {
                while (allocatedNames.count(name)) {
                    name += "_";
                }
                allocatedNames.emplace(name);
                return name;
            };
            auto contextName = freshName("__cj_arkts_context");
            auto moduleName = freshName("__cj_module");
            std::string code = "func __generated() { let " + contextName + " = __cj_arkts_current()\nlet " + moduleName + " = " +
                contextName + ".requireArkModule(\"__module__\").asObject()\n";
            std::unordered_map<std::string, std::string> literals{
                {"__module__", OptionValue(*fd, "module", ctx.curPackage->fullPackageName)},
                {"__name__", OptionValue(*fd, "foreignName", fd->identifier.Val())}};
            auto ns = OptionValue(*fd, "namespace", "");
            std::string receiver = moduleName;
            size_t nsIndex = 0;
            size_t start = 0;
            while (start < ns.size()) {
                auto end = ns.find('.', start);
                auto key = "__namespace_" + std::to_string(nsIndex) + "__";
                auto next = freshName("__cj_namespace_" + std::to_string(nsIndex++));
                literals[key] = ns.substr(start, end == std::string::npos ? end : end - start);
                code += "let " + next + " = " + receiver + ".getProperty(\"" + key + "\").asObject()\n";
                receiver = next;
                if (end == std::string::npos) {
                    break;
                }
                start = end + 1;
            }
            // Expand one registered enum into the glue-facing member list.
            auto enumMembers = [&](const EnumDecl* ed) {
                std::vector<EnumValue> values;
                for (const auto& member : arkTSEnums.at(ed)) {
                    values.push_back({member.ctor->identifier.Val(), member.isString, member.value});
                }
                return values;
            };
            auto valueName = freshName("__cj_result_value");
            auto numberName = freshName("__cj_result_number");
            auto resultName = freshName("__cj_result_converted");
            code += "let " + valueName + " = " + receiver + ".callMethod(\"__name__\", [";
            for (size_t i = 0; i < params.size(); ++i) {
                if (i != 0) {
                    code += ",";
                }
                auto argument = "__argument_" + std::to_string(i);
                if (types[i].enumeration != nullptr) {
                    code += EnumToJS(types[i].enumeration->identifier.Val(), enumMembers(types[i].enumeration),
                        contextName, argument, "__cj_enum_arg_" + std::to_string(i), literals);
                } else {
                    code += ToJS(types[i].scalar, contextName, argument);
                }
            }
            code += "])\n";
            if (ret.enumeration != nullptr) {
                auto stringName = freshName("__cj_result_string");
                code += EnumFromJS(ret.enumeration->identifier.Val(),
                    OptionValue(*ret.enumeration, "foreignName", ret.enumeration->identifier.Val()),
                    enumMembers(ret.enumeration), valueName, numberName, stringName, "__cj_enum_ret", literals);
            } else {
                code += FromJS(ret.scalar, valueName, numberName, resultName);
            }
            code += "}";
            if (!SpliceArkTSGeneratedBody(ctx, *fd, code, literals, params)) {
                fd->DisableAttr(Attribute::ARKTS_LOWERED);
                continue;
            }
        }
    }
}

bool TypeChecker::TypeCheckerImpl::SpliceArkTSGeneratedBody(ASTContext& ctx, AST::FuncDecl& fd,
    const std::string& code, const std::unordered_map<std::string, std::string>& literals,
    const std::vector<Ptr<AST::FuncParam>>& params)
{
    auto& fb = *fd.funcBody;
    auto file = fd.curFile;
    if (!file) {
        return false;
    }
    Parser parser(code, diag, ci->GetSourceManager(), fd.begin);
    parser.SetCurFile(file);
    auto generated = parser.ParseDecl(ScopeKind::TOPLEVEL);
    auto generatedFunc = DynamicCast<FuncDecl*>(generated.get());
    if (!generatedFunc || !generatedFunc->funcBody || !generatedFunc->funcBody->body) {
        return false;
    }
    fb.body = std::move(generatedFunc->funcBody->body);
    Walker(fb.body.get(), [&](Ptr<Node> node) {
        node->begin = fd.begin;
        node->end = fd.end;
        if (auto literal = DynamicCast<LitConstExpr*>(node); literal && literals.count(literal->stringValue)) {
            literal->stringValue = literals.at(literal->stringValue);
        }
        if (auto re = DynamicCast<RefExpr*>(node)) {
            auto name = re->ref.identifier.Val();
            if (name == "__cj_arkts_current") {
                if (auto target = importManager.GetImportedDecl(CONTEXT_PACKAGE, name)) {
                    re->ref.target = target;
                    re->ref.targets = {target};
                    re->SetTy(target->GetTy());
                }
            }
            if (name == "__cj_arkts_BigInt") {
                if (auto target = importManager.GetImportedDecl("std.math.numeric", "BigInt")) {
                    re->ref.target = target;
                    re->ref.targets = {target};
                    re->SetTy(target->GetTy());
                }
            }
            if (name == "Exception") {
                if (auto target = importManager.GetImportedDecl("std.core", "Exception")) {
                    re->ref.target = target;
                    re->ref.targets = {target};
                    re->SetTy(target->GetTy());
                }
            }
            if (name == "ArkTSConversionException") {
                // A user parameter may share the generated type name.
                // Bind generated failures to the package's declaration,
                // rather than letting parameter lookup shadow it.
                for (auto& owner : ctx.curPackage->files) {
                    for (auto& candidate : owner->decls) {
                        auto target = DynamicCast<ClassDecl*>(candidate.get());
                        if (target && target->identifier == name) {
                            re->ref.target = target;
                            re->ref.targets = {target};
                            re->SetTy(target->GetTy());
                        }
                    }
                }
            }
            for (size_t i = 0; i < params.size(); ++i) {
                if (name == "__argument_" + std::to_string(i)) {
                    re->ref.identifier = params[i]->identifier;
                    re->ref.target = params[i];
                    // Parameters precede the generated body. Using the
                    // declaration's beginning makes Lookup reject them
                    // as declared after this compiler-generated use.
                    re->begin = fd.end;
                }
            }
        }
        if (auto returned = DynamicCast<ReturnExpr*>(node)) {
            returned->refFuncBody = &fb;
        }
        return VisitAction::WALK_CHILDREN;
    }).Walk();
    AddCurFile(*fb.body, file);
    auto oldScope = ctx.currentScopeName;
    auto oldLevel = ctx.currentScopeLevel;
    ctx.currentScopeName = params.empty() ? fb.scopeName : params.front()->scopeName;
    if (params.empty()) {
        auto gate = ctx.currentScopeName.find_last_of(ScopeManagerApi::childScopeNameSplit);
        if (gate != std::string::npos) {
            ctx.currentScopeName[gate] = ScopeManagerApi::scopeNameSplit;
        }
    }
    ctx.currentScopeLevel = fb.scopeLevel + 1;
    Collector collector(scopeManager);
    auto previousSymbol = ctx.symbolTable.empty() ? ctx.symbolTable.end() : std::prev(ctx.symbolTable.end());
    // Function-body statements live directly in the parameter scope;
    // collecting the Block would create a nested scope unlike
    // Collector::CollectFuncBody and hide the generated local bindings.
    for (auto& statement : fb.body->body) {
        collector.BuildSymbolTable(ctx, statement.get(), ci->buildTrie);
    }
    // PreCheck has already built the declaration lookup map. Updating
    // only the symbol table leaves these new locals invisible to Lookup.
    auto firstGenerated = previousSymbol == ctx.symbolTable.end() ? ctx.symbolTable.begin() : std::next(previousSymbol);
    for (auto it = firstGenerated; it != ctx.symbolTable.end(); ++it) {
        auto& sym = *it;
        if (auto generatedDecl = DynamicCast<Decl*>(sym->node)) {
            ctx.AddDeclName({sym->name, ScopeManagerApi::GetScopeNameWithoutTail(sym->scopeName)},
                *generatedDecl);
        }
    }
    ctx.currentScopeName = oldScope;
    ctx.currentScopeLevel = oldLevel;
    return true;
}

void TypeChecker::TypeCheckerImpl::PrepareArkTSVariables(Package& pkg)
{
    if (pkg.TestAttr(Attribute::IMPORTED)) {
        return;
    }
    for (auto& file : pkg.files) {
        const auto declCount = file->decls.size();
        for (size_t i = 0; i < declCount; ++i) {
            auto vd = DynamicCast<VarDecl*>(file->decls[i].get());
            if (!vd || !vd->HasAnno(AnnotationKind::ARKTS) || !vd->TestAttr(Attribute::FOREIGN) || vd->outerDecl) {
                continue;
            }
            if (vd->isVar) {
                diag.DiagnoseRefactor(DiagKindRefactor::sema_arkts_foreign_var_mutable, *vd, vd->identifier.Val());
                continue;
            }
            // Resolve this binding as CJ, not C. Reads of the variable are
            // redirected to a generated getter that fetches the live ArkTS value.
            vd->EnableAttr(Attribute::ARKTS_LOWERED);
            vd->DisableAttr(Attribute::FOREIGN, Attribute::C, Attribute::NO_MANGLE);
            for (auto it = vd->modifiers.begin(); it != vd->modifiers.end();) {
                if (it->modifier == TokenKind::FOREIGN) {
                    it = vd->modifiers.erase(it);
                } else {
                    ++it;
                }
            }
            auto getter = MakeOwned<FuncDecl>();
            getter->identifier = "__cj_arkts_get_" + vd->identifier.Val();
            getter->curFile = file.get();
            getter->fullPackageName = vd->fullPackageName;
            getter->moduleName = vd->moduleName;
            getter->begin = vd->begin;
            getter->end = vd->end;
            getter->funcBody = MakeOwned<FuncBody>();
            getter->funcBody->funcDecl = getter.get();
            getter->funcBody->begin = vd->begin;
            getter->funcBody->end = vd->end;
            getter->funcBody->paramLists.emplace_back(MakeOwned<FuncParamList>());
            if (vd->type != nullptr) {
                getter->funcBody->retType = ASTCloner::Clone(vd->type.get());
            }
            getter->EnableAttr(Attribute::GLOBAL, Attribute::PUBLIC);
            arkTSVarGetters.emplace(vd, getter.get());
            arkTSVarGetterSources.emplace(getter.get(), vd);
            file->decls.emplace_back(std::move(getter));
        }
    }
}

void TypeChecker::TypeCheckerImpl::PrepareArkTSEnums(AST::Package& pkg)
{
    if (pkg.TestAttr(Attribute::IMPORTED)) {
        return;
    }
    for (auto& file : pkg.files) {
        for (auto& decl : file->decls) {
            auto ed = DynamicCast<EnumDecl*>(decl.get());
            if (!ed || !ed->HasAnno(AnnotationKind::ARKTS) || !ed->TestAttr(Attribute::FOREIGN) || ed->outerDecl) {
                continue;
            }
            PrepareArkTSEnum(*ed);
        }
    }
}

void TypeChecker::TypeCheckerImpl::PrepareArkTSEnum(AST::EnumDecl& decl)
{
    // Phase 1 keeps the Cangjie side a plain enum: its constructors stay normal
    // enum values and only the foreign-function glue consults the @ForeignValue
    // mapping registered below. Mark ARKTS_LOWERED so later annotation checks
    // accept @ArkTS/@ForeignValue on this enum and its constructors.
    decl.EnableAttr(Attribute::ARKTS_LOWERED);
    decl.DisableAttr(Attribute::FOREIGN, Attribute::C, Attribute::NO_MANGLE);
    // @ForeignValue maps individual constructors; on the enum type itself it is
    // misplaced. The generic annotation checker does not visit lowered enum
    // declarations, so validate the declaration's own annotations here.
    for (auto& anno : decl.annotations) {
        if (anno->kind == AnnotationKind::FOREIGN_VALUE) {
            diag.DiagnoseRefactor(DiagKindRefactor::sema_arkts_foreign_value_misplaced, *anno,
                decl.identifier.Val());
        }
    }
    for (auto it = decl.modifiers.begin(); it != decl.modifiers.end();) {
        if (it->modifier == TokenKind::FOREIGN) {
            it = decl.modifiers.erase(it);
        } else {
            ++it;
        }
    }
    if (decl.GetGeneric()) {
        diag.DiagnoseRefactor(DiagKindRefactor::sema_arkts_enum_generic, decl, decl.identifier.Val());
        return;
    }
    if (!decl.inheritedTypes.empty()) {
        diag.DiagnoseRefactor(DiagKindRefactor::sema_arkts_enum_super, decl, decl.identifier.Val());
        return;
    }
    if (decl.hasEllipsis) {
        diag.DiagnoseRefactor(DiagKindRefactor::sema_arkts_enum_non_exhaustive, decl, decl.identifier.Val());
        return;
    }
    if (!decl.members.empty()) {
        diag.DiagnoseRefactor(DiagKindRefactor::sema_arkts_enum_member, decl, decl.identifier.Val());
        return;
    }
    std::vector<ArkTSEnumMember> members;
    bool valid = true;
    // Auto numbering mirrors ArkTS: the first member defaults to 0 and an
    // omitted value continues the previous numeric value + 1; a string member
    // disables the counter, so a later omitted value is an error.
    double counter = -1.0;
    bool counterValid = true;
    for (auto& ctor : decl.constructors) {
        auto varCtor = DynamicCast<VarDecl*>(ctor.get());
        if (!varCtor) {
            diag.DiagnoseRefactor(DiagKindRefactor::sema_arkts_enum_payload, *ctor, ctor->identifier.Val());
            valid = false;
            continue;
        }
        const Annotation* value = nullptr;
        for (auto& anno : varCtor->annotations) {
            if (anno->kind == AnnotationKind::FOREIGN_VALUE) {
                value = anno.get(); // Duplicates are rejected by the annotation check.
                break;
            }
        }
        auto literal = (value != nullptr && value->args.size() == 1 && value->args[0]->expr != nullptr)
                           ? DynamicCast<LitConstExpr*>(value->args[0]->expr.get())
                           : nullptr;
        if (literal && literal->kind == LitConstKind::STRING) {
            counterValid = false;
            members.push_back({varCtor, true, literal->stringValue});
            continue;
        }
        if (literal && (literal->kind == LitConstKind::INTEGER || literal->kind == LitConstKind::FLOAT)) {
            counter = std::strtod(literal->stringValue.c_str(), nullptr);
            counterValid = true;
            members.push_back({varCtor, false, FormatEnumCounter(counter)});
            continue;
        }
        if (!counterValid) {
            diag.DiagnoseRefactor(DiagKindRefactor::sema_arkts_enum_value_type_mismatch, *varCtor, "String",
                "auto-increment");
            valid = false;
            continue;
        }
        counter += 1.0;
        members.push_back({varCtor, false, FormatEnumCounter(counter)});
    }
    if (valid && !members.empty()) {
        arkTSEnums.emplace(&decl, std::move(members));
    }
}

void TypeChecker::TypeCheckerImpl::LowerArkTSVariables(ASTContext& ctx)
{
    if (ctx.curPackage->TestAttr(Attribute::IMPORTED)) {
        return;
    }
    for (auto& file : ctx.curPackage->files) {
        for (auto& decl : file->decls) {
            auto fd = DynamicCast<FuncDecl*>(decl.get());
            if (!fd || !fd->funcBody || fd->funcBody->body) {
                continue;
            }
            auto source = arkTSVarGetterSources.find(fd);
            if (source == arkTSVarGetterSources.end()) {
                continue;
            }
            auto& vd = *source->second;
            if (!importManager.GetImportedDecl(CONTEXT_PACKAGE, "__cj_arkts_current")) {
                diag.DiagnoseRefactor(DiagKindRefactor::sema_arkts_bridge_pending, *fd);
                continue;
            }
            auto resolveScalar = [&](Ptr<Type> type) {
                if (!type) {
                    return std::string{};
                }
                SetTypeTy(ctx, *type); // Also substitutes aliases and preserves aliasTy for cjo.
                return ScalarName(type->GetTy());
            };
            auto ret = resolveScalar(vd.type.get());
            if (ret.empty()) {
                diag.DiagnoseRefactor(DiagKindRefactor::sema_arkts_foreign_var_unsupported_type, vd,
                    vd.type && vd.type->GetTy() ? Ty::ToString(vd.type->GetTy()) : vd.identifier.Val());
                continue;
            }
            std::unordered_set<std::string> allocatedNames;
            auto freshName = [&](std::string name) {
                while (allocatedNames.count(name)) {
                    name += "_";
                }
                allocatedNames.emplace(name);
                return name;
            };
            auto contextName = freshName("__cj_arkts_context");
            auto moduleName = freshName("__cj_module");
            std::string code = "func __generated() { let " + contextName + " = __cj_arkts_current()\nlet " + moduleName +
                " = " + contextName + ".requireArkModule(\"__module__\").asObject()\n";
            std::unordered_map<std::string, std::string> literals{
                {"__module__", OptionValue(vd, "module", ctx.curPackage->fullPackageName)},
                {"__name__", OptionValue(vd, "foreignName", vd.identifier.Val())}};
            auto ns = OptionValue(vd, "namespace", "");
            std::string receiver = moduleName;
            size_t nsIndex = 0;
            size_t start = 0;
            while (start < ns.size()) {
                auto end = ns.find('.', start);
                auto key = "__namespace_" + std::to_string(nsIndex) + "__";
                auto next = freshName("__cj_namespace_" + std::to_string(nsIndex++));
                literals[key] = ns.substr(start, end == std::string::npos ? end : end - start);
                code += "let " + next + " = " + receiver + ".getProperty(\"" + key + "\").asObject()\n";
                receiver = next;
                if (end == std::string::npos) {
                    break;
                }
                start = end + 1;
            }
            auto valueName = freshName("__cj_result_value");
            auto numberName = freshName("__cj_result_number");
            auto resultName = freshName("__cj_result_converted");
            code += "let " + valueName + " = " + receiver + ".getProperty(\"__name__\")\n" +
                FromJS(ret, valueName, numberName, resultName) + "}";
            (void)SpliceArkTSGeneratedBody(ctx, *fd, code, literals, {});
        }
    }
}

AST::FuncDecl* TypeChecker::TypeCheckerImpl::GetArkTSVarGetter(const AST::VarDecl& varDecl) const
{
    auto it = arkTSVarGetters.find(&varDecl);
    return it == arkTSVarGetters.end() ? nullptr : it->second;
}
