// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// Licensed under Apache-2.0 with Runtime Library Exception.
#include "TypeCheckerImpl.h"
#include "Collector.h"
#include "cangjie/AST/Create.h"
#include "cangjie/AST/Utils.h"
#include "cangjie/Parse/Parser.h"
#include <algorithm>
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

void TypeChecker::TypeCheckerImpl::LowerArkTSFunctions(ASTContext& ctx)
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
            auto resolveScalar = [&](Ptr<Type> type) {
                if (!type) {
                    return std::string{};
                }
                SetTypeTy(ctx, *type); // Also substitutes aliases and preserves aliasTy for cjo.
                return ScalarName(type->GetTy());
            };
            auto ret = resolveScalar(fb.retType.get());
            std::vector<Ptr<FuncParam>> params;
            std::vector<std::string> types;
            bool supported = !ret.empty();
            bool explicitThis = false;
            for (auto& list : fb.paramLists) {
                for (auto& param : list->params) {
                    auto name = resolveScalar(param->type.get());
                    supported = supported && !name.empty() && name != "Nothing";
                    params.push_back(param.get());
                    types.push_back(name);
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
            auto valueName = freshName("__cj_result_value");
            auto numberName = freshName("__cj_result_number");
            auto resultName = freshName("__cj_result_converted");
            code += "let " + valueName + " = " + receiver + ".callMethod(\"__name__\", [";
            for (size_t i = 0; i < params.size(); ++i) {
                if (i != 0) {
                    code += ",";
                }
                code += ToJS(types[i], contextName, "__argument_" + std::to_string(i));
            }
            code += "])\n" + FromJS(ret, valueName, numberName, resultName) + "}";
            Parser parser(code, diag, ci->GetSourceManager(), fd->begin);
            parser.SetCurFile(file.get());
            auto generated = parser.ParseDecl(ScopeKind::TOPLEVEL);
            auto generatedFunc = DynamicCast<FuncDecl*>(generated.get());
            if (!generatedFunc || !generatedFunc->funcBody || !generatedFunc->funcBody->body) {
                fd->DisableAttr(Attribute::ARKTS_LOWERED);
                continue;
            }
            fb.body = std::move(generatedFunc->funcBody->body);
            Walker(fb.body.get(), [&](Ptr<Node> node) {
                node->begin = fd->begin;
                node->end = fd->end;
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
                            re->begin = fd->end;
                        }
                    }
                }
                if (auto returned = DynamicCast<ReturnExpr*>(node)) {
                    returned->refFuncBody = &fb;
                }
                return VisitAction::WALK_CHILDREN;
            }).Walk();
            AddCurFile(*fb.body, file.get());
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
        }
    }
}
