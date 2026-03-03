// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements AST and related data print apis.
 */

#include "cangjie/AST/PrintNode.h"

#if !defined(NDEBUG) || defined(CMAKE_ENABLE_ASSERT)
#include <algorithm>
#include <iostream>
#include <string>

#include "cangjie/AST/ASTCasting.h"
#include "cangjie/AST/Utils.h"
#include "cangjie/Basic/Match.h"
#include "cangjie/Basic/Print.h"
#include "cangjie/Basic/StringConvertor.h"
#include "cangjie/Utils/Utils.h"
#endif

namespace Cangjie {
using namespace AST;
// Enable printNode for debug and CI version.
#if !defined(NDEBUG) || defined(CMAKE_ENABLE_ASSERT)
using namespace Utils;
using namespace Meta;

namespace {
const unsigned ONE_INDENT = 1u;
const unsigned TWO_INDENT = 2u;
const unsigned THREE_INDENT = 3u;
#define UNKNOWN_TY (ANSI_COLOR_RED + "unknown" + ANSI_COLOR_RESET)

// Function declarations
void PrintAnnotations(const Decl& decl, unsigned indent);
// `PrintTarget` has a default parameter in the declaration only; the
// definition below omits it to avoid a redeclaration error.
void PrintTarget(unsigned indent, const Decl& target, std::string addition);

const char* CaptureKindName(CaptureKind kind)
{
    switch (kind) {
        case CaptureKind::NO_CAPTURE:
            return "no_capture";
        case CaptureKind::CAPTURE_VAR:
            return "capture_var";
        case CaptureKind::TRANSITIVE_CAPTURE:
            return "transitive_capture";
        default:
            return "unknown";
    }
}

const char* SugarKindName(Expr::SugarKind kind)
{
    switch (kind) {
        case Expr::SugarKind::QUEST:
            return "quest";
        case Expr::SugarKind::IF_LET:
            return "if_let";
        case Expr::SugarKind::WHILE_LET:
            return "while_let";
        case Expr::SugarKind::FOR_IN_EXPR:
            return "for_in";
        case Expr::SugarKind::IS:
            return "is";
        case Expr::SugarKind::AS:
            return "as";
        case Expr::SugarKind::TOKEN_CALL:
            return "token_call";
        case Expr::SugarKind::NO_SUGAR:
            return "none";
        default:
            return "unknown";
    }
}

const char* ForInKindName(ForInKind kind)
{
    switch (kind) {
        case ForInKind::FORIN_INVALID:
            return "invalid";
        case ForInKind::FORIN_RANGE:
            return "range";
        case ForInKind::FORIN_STRING:
            return "string";
        case ForInKind::FORIN_ITER:
            return "iter";
        default:
            return "unknown";
    }
}

const char* CallKindName(CallKind kind)
{
    switch (kind) {
        case CallKind::CALL_INVALID:
            return "invalid";
        case CallKind::CALL_DECLARED_FUNCTION:
            return "declared_function";
        case CallKind::CALL_OBJECT_CREATION:
            return "object_creation";
        case CallKind::CALL_FUNCTION_PTR:
            return "function_ptr";
        case CallKind::CALL_STRUCT_CREATION:
            return "struct_creation";
        case CallKind::CALL_SUPER_FUNCTION:
            return "super_function";
        case CallKind::CALL_VARIADIC_FUNCTION:
            return "variadic_function";
        case CallKind::CALL_ANNOTATION:
            return "annotation";
        case CallKind::CALL_BUILTIN_FUNCTION:
            return "builtin_function";
        case CallKind::CALL_INTRINSIC_FUNCTION:
            return "intrinsic_function";
        default:
            return "unknown";
    }
}

void PrintAnnotation(unsigned indent, const Annotation& anno);

void PrintModifiers(const Decl& decl, unsigned indent)
{
    std::string str = "Modifiers:";
    for (auto& i : decl.modifiers) {
        str += " " + std::string(TOKENS[static_cast<int>(i.modifier)]);
    }
    if (!decl.modifiers.empty()) {
        PrintIndent(indent + ONE_INDENT, str);
    }
}

void PrintModifierSet(unsigned indent, const std::set<Modifier>& modifiers)
{
    if (modifiers.empty()) {
        return;
    }
    std::string str = "Modifiers:";
    for (auto& i : modifiers) {
        str += " " + std::string(TOKENS[static_cast<int>(i.modifier)]);
    }
    PrintIndent(indent + ONE_INDENT, str);
}

void PrintAnnotationsVec(unsigned indent, const std::vector<OwnedPtr<Annotation>>& annotations)
{
    if (annotations.empty()) {
        return;
    }
    PrintIndent(indent, "annotations [");
    for (auto& anno : annotations) {
        if (!anno) {
            continue;
        }
        PrintAnnotation(indent + ONE_INDENT, *anno.get());
    }
    PrintIndent(indent, "]");
}

void PrintNameReferenceInfo(unsigned indent, const NameReferenceExpr& expr)
{
    if (!expr.instTys.empty()) {
        PrintIndent(indent + ONE_INDENT, "instTys [");
        for (auto ty : expr.instTys) {
            if (ty) {
                PrintIndent(indent + TWO_INDENT, ty->String());
            }
        }
        PrintIndent(indent + ONE_INDENT, "]");
    }
    if (expr.callOrPattern) {
        // Print raw address to avoid operator<< ambiguity on Ptr<T>.
        PrintIndent(indent + ONE_INDENT, "callOrPattern: ptr:",
            static_cast<const void*>(expr.callOrPattern.get()));
    }
    if (expr.matchedParentTy) {
        PrintIndent(indent + ONE_INDENT, "matchedParentTy:", expr.matchedParentTy->String());
    }
    if (expr.aliasTarget) {
        PrintTarget(indent + ONE_INDENT, *expr.aliasTarget, "aliasTarget");
    }
    if (!expr.isAlone) {
        PrintIndent(indent + ONE_INDENT, "isAlone:", expr.isAlone);
    }
    if (expr.compilerAddedTyArgs) {
        PrintIndent(indent + ONE_INDENT, "compilerAddedTyArgs:", expr.compilerAddedTyArgs);
    }
}

void PrintTarget(unsigned indent, const Decl& target, std::string addition = "target")
{
    if (target.mangledName.empty()) {
        PrintIndent(indent, addition + ": ptr:", &target);
    } else {
        PrintIndent(indent, addition + ": mangledName:", "\"" + target.exportId + "\"");
    }
}

void PrintBasic(unsigned indent, const Node& node)
{
    // Basic info:
    if (node.curFile) {
        PrintIndent(indent, "curFile:", node.curFile->filePath);
    }
    if (!node.begin.IsZero() || !node.end.IsZero()) {
        PrintIndent(indent, "position:", node.begin.ToString(), node.end.ToString());
    }
    if (!node.scopeName.empty()) {
        PrintIndent(indent, "scopeName:", "\"" + node.scopeName + "\"");
    }
    if (node.ty) {
        const auto tyStr = node.ty->String();
        if (!tyStr.empty() && tyStr != "UnknownType") {
            PrintIndent(indent, "ty:", tyStr);
        }
    }
    auto fullPkgName = node.GetFullPackageName();
    if (!fullPkgName.empty()) {
        PrintIndent(indent, "fullPackageName:", fullPkgName);
    }
    if (!node.GetAttrs().GetAllIdxOfAttr().empty()) {
        PrintIndent(indent, "attributes:", node.GetAttrs().ToString());
    }
    if (!node.exportId.empty()) {
        PrintIndent(indent, "exportId:", "\"" + node.exportId + "\"");
    }
    if (auto d = DynamicCast<Decl>(&node)) {
        if (d->linkage != Linkage::EXTERNAL || d->IsConst()) {
            PrintIndent(indent, "linkage:", static_cast<int>(d->linkage), ", isConst:", static_cast<int>(d->IsConst()));
        }
        if (d->doNotExport) {
            PrintIndent(indent, "doNotExport:", d->doNotExport);
        }
        if (!d->moduleName.empty()) {
            PrintIndent(indent, "moduleName:", d->moduleName);
        }
        if (!d->mangledName.empty()) {
            PrintIndent(indent, "mangledName:", "\"" + d->mangledName + "\"");
        }
        if (d->annotationsArray) {
            PrintNode(d->annotationsArray.get(), indent, "annotationsArray");
        }
        if (d->outerDecl) {
            PrintTarget(indent, *d->outerDecl, "outerDecl");
        }
        if (d->genericDecl) {
            PrintTarget(indent, *d->genericDecl, "genericDecl");
        }
        if (d->platformImplementation) {
            PrintTarget(indent, *d->platformImplementation, "platformImplementation");
        }
    }
    if (!node.comments.IsEmpty()) {
        PrintIndent(indent, "comments: " + node.comments.ToString());
    }
}

void PrintModifierNode(unsigned indent, const Modifier& modifier)
{
    PrintIndent(indent, "Modifier:", TOKENS[static_cast<int>(modifier.modifier)], "{");
    PrintBasic(indent + ONE_INDENT, modifier);
    PrintIndent(indent + ONE_INDENT, "isExplicit:", modifier.isExplicit);
    PrintIndent(indent, "}");
}

void PrintIndentTokens(unsigned indent, const std::vector<Token>& args)
{
    if (args.empty()) {
        return;
    }
    std::stringstream ss;
    PrintIndent(indent, "pos: ", args.front().Begin().ToString(), args.back().End().ToString());
    for (auto& it : args) {
        if (it.kind == TokenKind::NL) {
            ss << ";";
        } else {
            ss << it.Value();
        }
    }
    PrintIndent(indent, "\"" + ss.str() + "\"");
}

void PrintMacroInvocation(unsigned indent, const MacroInvocation& invocation)
{
    if (!invocation.attrs.empty()) {
        PrintIndent(indent + ONE_INDENT, "attrs {");
        PrintIndentTokens(indent + TWO_INDENT, invocation.attrs);
        PrintIndent(indent + ONE_INDENT, "}");
    }
    if (!invocation.args.empty()) {
        PrintIndent(indent + ONE_INDENT, "args {");
        PrintIndentTokens(indent + TWO_INDENT, invocation.args);
        PrintIndent(indent + ONE_INDENT, "}");
    }
    if (invocation.decl) {
        PrintNode(invocation.decl, indent + ONE_INDENT, "decl");
    }
    if (!invocation.outerDeclIdent.empty()) {
        PrintIndent(indent + ONE_INDENT, "outerDeclIdent:", invocation.outerDeclIdent);
    }
    if (invocation.parent) {
        PrintIndent(indent + ONE_INDENT, "parent: ptr:", static_cast<const void*>(invocation.parent.get()));
    }
    if (invocation.target) {
        PrintTarget(indent + ONE_INDENT, *invocation.target, "target");
    }
    if (!invocation.newTokens.empty()) {
        PrintIndent(indent + ONE_INDENT, "newTokens {");
        PrintIndentTokens(indent + TWO_INDENT, invocation.newTokens);
        PrintIndent(indent + ONE_INDENT, "}");
    }
    if (!invocation.newTokensStr.empty()) {
        PrintIndent(indent + ONE_INDENT, "newTokensStr:", "\"" + invocation.newTokensStr + "\"");
    }
    if (invocation.hasParenthesis) {
        PrintIndent(indent + ONE_INDENT, "hasParenthesis:", invocation.hasParenthesis);
    }
    if (invocation.hasAttr) {
        PrintIndent(indent + ONE_INDENT, "hasAttr:", invocation.hasAttr);
    }
    if (invocation.isFuncParam) {
        PrintIndent(indent + ONE_INDENT, "isFuncParam:", invocation.isFuncParam);
    }
    if (invocation.isCustom) {
        PrintIndent(indent + ONE_INDENT, "isCustom:", invocation.isCustom);
    }
    if (invocation.isCompileTimeVisible) {
        PrintIndent(indent + ONE_INDENT, "isCompileTimeVisible:", invocation.isCompileTimeVisible);
    }
    if (invocation.isCurFile) {
        PrintIndent(indent + ONE_INDENT, "isCurFile:", invocation.isCurFile);
    }
    if (invocation.isForLSP) {
        PrintIndent(indent + ONE_INDENT, "isForLSP:", invocation.isForLSP);
    }
    if (invocation.hasShownCode) {
        PrintIndent(indent + ONE_INDENT, "hasShownCode:", invocation.hasShownCode);
    }
}

void PrintPackage(unsigned indent, const Package& package)
{
    PrintIndent(indent, "Package:", package.fullPackageName, "{");

    if (package.noSubPkg) {
        PrintIndent(indent + ONE_INDENT, "noSubPkg:", package.noSubPkg);
    }
    if (package.accessible != AccessLevel::PUBLIC) {
        PrintIndent(indent + ONE_INDENT, "accessible:", static_cast<int>(package.accessible));
    }
    if (package.hasSourceImportedDecl) {
        PrintIndent(indent + ONE_INDENT, "hasSourceImportedDecl:", package.hasSourceImportedDecl);
    }
    if (package.isMacroPackage) {
        PrintIndent(indent + ONE_INDENT, "isMacroPackage:", package.isMacroPackage);
    }
    if (!package.needExported) {
        PrintIndent(indent + ONE_INDENT, "needExported:", package.needExported);
    }

    for (auto& it : package.files) {
        PrintNode(it.get(), indent + ONE_INDENT);
    }

    if (!package.genericInstantiatedDecls.empty()) {
        PrintIndent(indent + ONE_INDENT, "genericInstantiatedDecls {");
        for (auto& it : package.genericInstantiatedDecls) {
            PrintNode(it.get(), indent + TWO_INDENT);
        }
        PrintIndent(indent + ONE_INDENT, "}");
    }

    if (!package.srcImportedNonGenericDecls.empty()) {
        PrintIndent(indent + ONE_INDENT, "srcImportedNonGenericDecls {");
        for (auto& it : package.srcImportedNonGenericDecls) {
            PrintNode(it.get(), indent + TWO_INDENT);
        }
        PrintIndent(indent + ONE_INDENT, "}");
    }

    if (!package.inlineFuncDecls.empty()) {
        PrintIndent(indent + ONE_INDENT, "inlineFuncDecls {");
        for (auto& it : package.inlineFuncDecls) {
            PrintNode(it.get(), indent + TWO_INDENT);
        }
        PrintIndent(indent + ONE_INDENT, "}");
    }

    PrintIndent(indent, "}");
}

void PrintFile(unsigned indent, const File& file)
{
    PrintIndent(indent, "File:", file.fileName, "{");
    if (!file.filePath.empty()) {
        PrintIndent(indent + ONE_INDENT, "filePath:", file.filePath);
    }
    if (file.hasMacro) {
        PrintIndent(indent + ONE_INDENT, "hasMacro:", file.hasMacro);
    }
    if (!file.macroCallFilePath.empty()) {
        PrintIndent(indent + ONE_INDENT, "macroCallFilePath:", file.macroCallFilePath);
    }
    if (file.isCommon) {
        PrintIndent(indent + ONE_INDENT, "isCommon:", file.isCommon);
    }
    if (file.isPlatform) {
        PrintIndent(indent + ONE_INDENT, "isPlatform:", file.isPlatform);
    }
    if (file.feature) {
        PrintNode(file.feature.get(), indent + ONE_INDENT, "features");
    }
    if (file.package) {
        PrintNode(file.package.get(), indent + ONE_INDENT, "package");
    }
    for (auto& it : file.imports) {
        PrintNode(it.get(), indent + ONE_INDENT);
    }
    for (auto& it : file.decls) {
        if (!it || it->identifier == MAIN_INVOKE) {
            continue;
        }
        PrintNode(it.get(), indent + ONE_INDENT);
    }
    for (auto& it : file.exportedInternalDecls) {
        PrintNode(it.get(), indent + ONE_INDENT, "exportedInternalDecls");
    }
    if (!file.originalMacroCallNodes.empty()) {
        PrintIndent(indent + ONE_INDENT, "originalMacroCallNodes [");
        for (auto& macroNode : file.originalMacroCallNodes) {
            PrintNode(macroNode.get(), indent + TWO_INDENT);
        }
        PrintIndent(indent + ONE_INDENT, "]");
    }
    PrintIndent(indent, "}");
}

void PrintMacroExpandParam(unsigned indent, const AST::MacroExpandParam& macroExpand)
{
    PrintIndent(indent, "MacroExpandParam:", macroExpand.invocation.fullName, "{");
    PrintBasic(indent + ONE_INDENT, macroExpand);
    PrintAnnotations(macroExpand, indent + ONE_INDENT);
    if (macroExpand.hasLetOrVar) {
        PrintIndent(indent + ONE_INDENT, "paramDeclKind:", macroExpand.isVar ? "var" : "let");
    }
    if (macroExpand.isNamedParam) {
        PrintIndent(indent + ONE_INDENT, "isNamedParam:", macroExpand.isNamedParam);
    }
    if (!macroExpand.notMarkPos.IsZero()) {
        PrintIndent(indent + ONE_INDENT, "notMarkPos:", macroExpand.notMarkPos.ToString());
    }
    PrintNode(macroExpand.type.get(), indent + ONE_INDENT, "type");
    PrintNode(macroExpand.assignment.get(), indent + ONE_INDENT, "assignment");
    if (macroExpand.desugarDecl) {
        PrintNode(macroExpand.desugarDecl.get(), indent + ONE_INDENT, "desugarDecl");
    }
    PrintMacroInvocation(indent, macroExpand.invocation);
    PrintIndent(indent, "}");
}

void PrintGenericParamDecl(unsigned indent, const AST::GenericParamDecl& gpd)
{
    PrintIndent(indent, "GenericParamDecl:", gpd.identifier.Val(), "{");
    PrintBasic(indent + ONE_INDENT, gpd);
    PrintAnnotations(gpd, indent + ONE_INDENT);
    PrintModifiers(gpd, indent);
    PrintIndent(indent, "}");
}

void PrintAnnotation(unsigned indent, const Annotation& anno)
{
    PrintIndent(indent, "Annotation:", anno.identifier, "{");
    PrintBasic(indent + ONE_INDENT, anno);
    PrintIndent(indent + ONE_INDENT, "kind:", static_cast<int>(anno.kind));
    if (anno.overflowStrategy != OverflowStrategy::NA) {
        PrintIndent(indent + ONE_INDENT, "overflowStrategy:", OverflowStrategyName(anno.overflowStrategy));
    }
    if (anno.isCompileTimeVisible) {
        PrintIndent(indent + ONE_INDENT, "isCompileTimeVisible:", anno.isCompileTimeVisible);
    }
    if (!anno.args.empty()) {
        PrintIndent(indent + ONE_INDENT, "args [");
        for (auto& arg : anno.args) {
            PrintNode(arg.get(), indent + TWO_INDENT);
        }
        PrintIndent(indent + ONE_INDENT, "]");
    }
    if (!anno.adAnnotation.Empty()) {
        PrintIndent(indent + ONE_INDENT, "adAnnotation:", anno.adAnnotation);
    }
    if (!anno.attrs.empty()) {
        PrintIndent(indent + ONE_INDENT, "attrs {");
        PrintIndentTokens(indent + TWO_INDENT, anno.attrs);
        PrintIndent(indent + ONE_INDENT, "}");
    }
    if (!anno.definedPackage.empty()) {
        PrintIndent(indent + ONE_INDENT, "definedPackage:", anno.definedPackage);
    }
    if (anno.condExpr) {
        PrintNode(anno.condExpr.get(), indent + ONE_INDENT, "condExpr");
    }
    if (anno.target != 0) {
        PrintIndent(indent + ONE_INDENT, "target:", anno.target);
    }
    if (anno.runtimeVisible) {
        PrintIndent(indent + ONE_INDENT, "runtimeVisible:", anno.runtimeVisible);
    }
    if (anno.baseExpr) {
        PrintNode(anno.baseExpr.get(), indent + ONE_INDENT, "baseExpr");
    }
    PrintIndent(indent, "}");
}

void PrintAnnotations(const Decl& decl, unsigned indent)
{
    if (decl.annotations.empty()) {
        return;
    }
    PrintIndent(indent, "annotations [");
    for (auto& anno : decl.annotations) {
        if (!anno) {
            continue;
        }
        PrintAnnotation(indent + ONE_INDENT, *anno.get());
    }
    PrintIndent(indent, "]");
}


void PrintFuncParam(unsigned indent, const FuncParam& param)
{
    PrintIndent(indent, "FuncParam:", param.identifier.Val(), "{");
    PrintBasic(indent + ONE_INDENT, param);
    PrintAnnotations(param, indent + ONE_INDENT);
    if (param.hasLetOrVar) {
        PrintIndent(indent + ONE_INDENT, "paramDeclKind:", param.isVar ? "var" : "let");
    }
    if (param.isNamedParam) {
        PrintIndent(indent + ONE_INDENT, "isNamedParam:", param.isNamedParam);
    }
    if (!param.notMarkPos.IsZero()) {
        PrintIndent(indent + ONE_INDENT, "notMarkPos:", param.notMarkPos.ToString());
    }
    PrintNode(param.type.get(), indent + ONE_INDENT, "type");
    PrintNode(param.assignment.get(), indent + ONE_INDENT, "assignment");
    if (param.desugarDecl) {
        PrintNode(param.desugarDecl.get(), indent + ONE_INDENT, "desugarDecl");
    }
    PrintIndent(indent, "}");
}

void PrintFuncParamList(unsigned indent, const FuncParamList& paramList)
{
    PrintIndent(indent, "FuncParamList {");
    if (paramList.hasVariableLenArg) {
        PrintIndent(indent + ONE_INDENT, "hasVariableLenArg:", paramList.hasVariableLenArg);
        PrintIndent(indent + ONE_INDENT, "variadicArgIndex:", paramList.variadicArgIndex);
    }
    for (auto& it : paramList.params) {
        PrintNode(it.get(), indent + ONE_INDENT);
    }
    PrintIndent(indent, "}");
}

void PrintMacroDecl(unsigned indent, const MacroDecl& macroDecl)
{
    PrintIndent(indent, "MacroDecl:", macroDecl.identifier.Val(), "{");
    PrintBasic(indent + ONE_INDENT, macroDecl);
    PrintModifiers(macroDecl, indent);
    if (macroDecl.desugarDecl) {
        PrintIndent(indent + ONE_INDENT, "// desugar decl");
        PrintNode(macroDecl.desugarDecl.get(), indent + ONE_INDENT);
    } else {
        PrintIndent(indent + ONE_INDENT, "// macro body");
        PrintNode(macroDecl.funcBody.get(), indent + ONE_INDENT);
    }
    PrintIndent(indent, "}");
}

void PrintFuncDecl(unsigned indent, const FuncDecl& funcDecl)
{
    PrintIndent(indent, "FuncDecl:", funcDecl.identifier.Val(), funcDecl.identifier.Begin(),
        funcDecl.identifier.End(), "{");
    PrintBasic(indent + ONE_INDENT, funcDecl);
    if (funcDecl.TestAttr(Attribute::CONSTRUCTOR) && funcDecl.constructorCall == ConstructorCall::SUPER) {
        if (funcDecl.funcBody && funcDecl.funcBody->body && !funcDecl.funcBody->body->body.empty()) {
            if (auto ce = DynamicCast<CallExpr*>(funcDecl.funcBody->body->body.begin()->get()); ce) {
                if (ce->TestAttr(Attribute::COMPILER_ADD)) {
                    PrintIndent(indent + ONE_INDENT, "super: implicitSuper");
                } else {
                    PrintIndent(indent + ONE_INDENT, "super: explicitSuper");
                }
            }
        }
    }
    PrintAnnotations(funcDecl, indent + ONE_INDENT);
    PrintModifiers(funcDecl, indent);
    if (funcDecl.funcBody) {
        PrintNode(funcDecl.funcBody.get(), indent + ONE_INDENT);
    }
    if (funcDecl.ownerFunc) {
        PrintTarget(indent + ONE_INDENT, *funcDecl.ownerFunc, "ownerFunc");
    }
    if (funcDecl.propDecl) {
        PrintTarget(indent + ONE_INDENT, *funcDecl.propDecl, "propDecl");
    }
    if (funcDecl.hasVariableLenArg) {
        PrintIndent(indent + ONE_INDENT, "hasVariableLenArg:", funcDecl.hasVariableLenArg);
        PrintIndent(indent + ONE_INDENT, "variadicArgIndex:", funcDecl.variadicArgIndex);
    }
    if (funcDecl.isConst) {
        PrintIndent(indent + ONE_INDENT, "isConst:", funcDecl.isConst);
    }
    if (funcDecl.isGetter) {
        PrintIndent(indent + ONE_INDENT, "isGetter:", funcDecl.isGetter);
    }
    if (funcDecl.isSetter) {
        PrintIndent(indent + ONE_INDENT, "isSetter:", funcDecl.isSetter);
    }
    if (funcDecl.isInline) {
        PrintIndent(indent + ONE_INDENT, "isInline:", funcDecl.isInline);
    }
    if (funcDecl.isFastNative) {
        PrintIndent(indent + ONE_INDENT, "isFastNative:", funcDecl.isFastNative);
    }
    if (funcDecl.isReExportedOrRenamed) {
        PrintIndent(indent + ONE_INDENT, "isReExportedOrRenamed:", funcDecl.isReExportedOrRenamed);
    }
    if (funcDecl.isFrozen) {
        PrintIndent(indent + ONE_INDENT, "isFrozen:", funcDecl.isFrozen);
    }
    if (funcDecl.overflowStrategy != OverflowStrategy::NA) {
        PrintIndent(indent + ONE_INDENT, "overflowStrategy:", OverflowStrategyName(funcDecl.overflowStrategy));
    }
    if (funcDecl.op != TokenKind::ILLEGAL) {
        PrintIndent(indent + ONE_INDENT, "operator:", TOKENS[static_cast<int>(funcDecl.op)]);
    }
    if (funcDecl.constructorCall != ConstructorCall::NONE) {
        PrintIndent(indent + ONE_INDENT,
            "constructorCall:",
            funcDecl.constructorCall == ConstructorCall::SUPER ? "super" : "other_init");
    }
    PrintIndent(indent, "}");
}

void PrintPrimaryCtorDecl(unsigned indent, const PrimaryCtorDecl& primaryCtor)
{
    PrintIndent(indent, "PrimaryCtorDecl:", primaryCtor.identifier.Val(), "{");
    PrintBasic(indent + ONE_INDENT, primaryCtor);
    PrintAnnotations(primaryCtor, indent + ONE_INDENT);
    PrintModifiers(primaryCtor, indent);
    PrintNode(primaryCtor.funcBody.get(), indent + ONE_INDENT);
    if (primaryCtor.hasVariableLenArg) {
        PrintIndent(indent + ONE_INDENT, "hasVariableLenArg:", primaryCtor.hasVariableLenArg);
    }
    if (primaryCtor.isConst) {
        PrintIndent(indent + ONE_INDENT, "isConst:", primaryCtor.isConst);
    }
    if (primaryCtor.isFastNative) {
        PrintIndent(indent + ONE_INDENT, "isFastNative:", primaryCtor.isFastNative);
    }
    if (primaryCtor.overflowStrategy != OverflowStrategy::NA) {
        PrintIndent(indent + ONE_INDENT, "overflowStrategy:", OverflowStrategyName(primaryCtor.overflowStrategy));
    }
    PrintIndent(indent, "}");
}

void PrintMainDecl(unsigned indent, const MainDecl& mainDecl)
{
    PrintIndent(indent, "MainDecl:", mainDecl.identifier.Val(), "{");
    PrintBasic(indent + ONE_INDENT, mainDecl);
    if (mainDecl.desugarDecl) {
        PrintNode(mainDecl.desugarDecl.get(), indent + ONE_INDENT, "desugarDecl");
    } else {
        PrintNode(mainDecl.funcBody.get(), indent + ONE_INDENT, "funcBody");
    }
    if (mainDecl.isFastNative) {
        PrintIndent(indent + ONE_INDENT, "isFastNative:", mainDecl.isFastNative);
    }
    if (mainDecl.overflowStrategy != OverflowStrategy::NA) {
        PrintIndent(indent + ONE_INDENT, "overflowStrategy:", OverflowStrategyName(mainDecl.overflowStrategy));
    }
    PrintIndent(indent, "}");
}

void PrintGeneric(unsigned indent, const Generic& generic)
{
    if (!generic.typeParameters.empty()) {
        PrintIndent(indent, "typeParameters {");
        for (auto& it : generic.typeParameters) {
            PrintNode(it.get(), indent + ONE_INDENT);
        }
        PrintIndent(indent, "}");
    }
    if (!generic.genericConstraints.empty()) {
        PrintIndent(indent, "genericConstraints {");
        for (auto& it : generic.genericConstraints) {
            PrintNode(it.get(), indent + ONE_INDENT);
        }
        PrintIndent(indent, "}");
    }
}

void PrintGenericNode(unsigned indent, const Generic& generic)
{
    PrintIndent(indent, "Generic {");
    PrintBasic(indent + ONE_INDENT, generic);
    PrintGeneric(indent + ONE_INDENT, generic);
    PrintIndent(indent, "}");
}

void PrintDeclNode(unsigned indent, const Decl& decl)
{
    PrintIndent(indent, "Decl:", decl.identifier.Val(), "{");
    PrintBasic(indent + ONE_INDENT, decl);
    PrintAnnotations(decl, indent + ONE_INDENT);
    PrintModifiers(decl, indent);
    if (decl.generic) {
        PrintGeneric(indent + ONE_INDENT, *decl.generic);
    }
    PrintIndent(indent, "}");
}

void PrintClassLikeDecl(unsigned indent, const ClassLikeDecl& decl)
{
    PrintIndent(indent, "ClassLikeDecl:", decl.identifier.Val(), "{");
    PrintBasic(indent + ONE_INDENT, decl);
    PrintAnnotations(decl, indent + ONE_INDENT);
    PrintModifiers(decl, indent);
    if (decl.generic) {
        PrintGeneric(indent + ONE_INDENT, *decl.generic);
    }
    if (!decl.inheritedTypes.empty()) {
        PrintIndent(indent + ONE_INDENT, "inheritedTypes [");
        for (auto& it : decl.inheritedTypes) {
            PrintNode(it.get(), indent + TWO_INDENT);
        }
        PrintIndent(indent + ONE_INDENT, "]");
    }
    if (!decl.subDecls.empty()) {
        PrintIndent(indent + ONE_INDENT, "subDecls [");
        for (auto sub : decl.subDecls) {
            if (sub) {
                PrintTarget(indent + TWO_INDENT, *sub, "subDecl");
            }
        }
        PrintIndent(indent + ONE_INDENT, "]");
    }
    PrintIndent(indent, "}");
}

const char* BuiltInTypeName(BuiltInType type)
{
    switch (type) {
        case BuiltInType::ARRAY:
            return "Array";
        case BuiltInType::POINTER:
            return "Pointer";
        case BuiltInType::CSTRING:
            return "CString";
        case BuiltInType::CFUNC:
            return "CFunc";
        case BuiltInType::VARRAY:
            return "VArray";
        default:
            return "Unknown";
    }
}

void PrintBuiltInDecl(unsigned indent, const BuiltInDecl& decl)
{
    PrintIndent(indent, "BuiltInDecl:", BuiltInTypeName(decl.type), "{");
    PrintBasic(indent + ONE_INDENT, decl);
    PrintAnnotations(decl, indent + ONE_INDENT);
    PrintModifiers(decl, indent);
    PrintIndent(indent, "}");
}

void PrintPackageDecl(unsigned indent, const PackageDecl& pkgDecl)
{
    PrintIndent(indent, "PackageDecl:", pkgDecl.identifier.Val(), "{");
    PrintBasic(indent + ONE_INDENT, pkgDecl);
    if (!pkgDecl.fullPackageName.empty()) {
        PrintIndent(indent + ONE_INDENT, "fullPackageName:", pkgDecl.fullPackageName);
    }
    PrintNode(pkgDecl.srcPackage, indent + ONE_INDENT, "srcPackage");
    PrintIndent(indent, "}");
}

void PrintInvalidDecl(unsigned indent, const InvalidDecl& decl)
{
    PrintIndent(indent, "InvalidDecl {");
    PrintBasic(indent + ONE_INDENT, decl);
    if (decl.doNotExport) {
        PrintIndent(indent + ONE_INDENT, "doNotExport:", decl.doNotExport);
    }
    PrintIndent(indent, "}");
}

void PrintFuncBody(unsigned indent, const FuncBody& body)
{
    PrintIndent(indent, "FuncBody {");
    PrintBasic(indent + ONE_INDENT, body);
    if (!body.doubleArrowPos.IsZero()) {
        PrintIndent(indent + ONE_INDENT, "doubleArrowPos:", body.doubleArrowPos.ToString());
    }
    if (!body.colonPos.IsZero()) {
        PrintIndent(indent + ONE_INDENT, "colonPos:", body.colonPos.ToString());
    }
    if (body.generic) {
        PrintGeneric(indent + ONE_INDENT, *body.generic);
    }
    if (body.captureKind != CaptureKind::NO_CAPTURE) {
        PrintIndent(indent + ONE_INDENT, "captureKind:", CaptureKindName(body.captureKind));
    }
    if (!body.capturedVars.empty()) {
        PrintIndent(indent + ONE_INDENT, "capturedVars:", body.capturedVars.size());
    }
    if (body.funcDecl) {
        PrintTarget(indent + ONE_INDENT, *body.funcDecl, "funcDecl");
    }
    if (body.outerFunc) {
        PrintIndent(indent + ONE_INDENT, "outerFunc: ptr:", static_cast<const void*>(body.outerFunc));
    }
    if (body.parentClassLike) {
        PrintTarget(indent + ONE_INDENT, *body.parentClassLike, "parentClassLike");
    }
    if (body.parentStruct) {
        PrintTarget(indent + ONE_INDENT, *body.parentStruct, "parentStruct");
    }
    if (body.parentEnum) {
        PrintTarget(indent + ONE_INDENT, *body.parentEnum, "parentEnum");
    }
    for (auto& it : body.paramLists) {
        PrintNode(it.get(), indent + ONE_INDENT);
    }
    if (body.retType) {
        PrintNode(body.retType.get(), indent + ONE_INDENT);
    }
    PrintNode(body.body.get(), indent + ONE_INDENT, "body");
    PrintIndent(indent, "}");
}

void PrintPropDecl(unsigned indent, const PropDecl& propDecl)
{
    PrintIndent(indent, "PropDecl:", propDecl.identifier.Val(), "{");
    PrintBasic(indent + ONE_INDENT, propDecl);
    PrintAnnotations(propDecl, indent + ONE_INDENT);
    PrintModifiers(propDecl, indent);
    if (propDecl.isConst) {
        PrintIndent(indent + ONE_INDENT, "Const");
    } else {
        PrintIndent(indent + ONE_INDENT, propDecl.isVar ? "Var" : "Let");
    }
    PrintNode(propDecl.type.get(), indent + ONE_INDENT, "type");
    PrintNode(propDecl.initializer.get(), indent + ONE_INDENT, "initializer");
    for (auto& propMemberDecl : propDecl.setters) {
        PrintNode(propMemberDecl.get(), indent + ONE_INDENT);
    }
    for (auto& propMemberDecl : propDecl.getters) {
        PrintNode(propMemberDecl.get(), indent + ONE_INDENT);
    }
    PrintIndent(indent, "}");
}

void PrintMacroExpandDecl(unsigned indent, const AST::MacroExpandDecl& macroExpand)
{
    PrintIndent(indent, "MacroExpand:", macroExpand.invocation.fullName, "{");
    PrintBasic(indent + ONE_INDENT, macroExpand);
    PrintAnnotations(macroExpand, indent + ONE_INDENT);
    PrintModifiers(macroExpand, indent);
    PrintMacroInvocation(indent, macroExpand.invocation);
    PrintIndent(indent, "}");
}

void PrintVarWithPatternDecl(unsigned indent, const VarWithPatternDecl& varWithPatternDecl)
{
    PrintIndent(indent, "VarWithPatternDecl {");
    PrintBasic(indent + ONE_INDENT, varWithPatternDecl);
    PrintAnnotations(varWithPatternDecl, indent + ONE_INDENT);
    PrintModifiers(varWithPatternDecl, indent);
    if (varWithPatternDecl.isConst) {
        PrintIndent(indent + ONE_INDENT, "Const");
    } else {
        PrintIndent(indent + ONE_INDENT, varWithPatternDecl.isVar ? "Var" : "Let");
    }
    PrintNode(varWithPatternDecl.irrefutablePattern.get(), indent + ONE_INDENT);
    PrintNode(varWithPatternDecl.type.get(), indent + ONE_INDENT, "type");
    PrintNode(varWithPatternDecl.initializer.get(), indent + ONE_INDENT, "initializer");
    PrintIndent(indent, "}");
}

void PrintVarDecl(unsigned indent, const VarDecl& varDecl)
{
    std::string key = varDecl.isConst ? "const " : varDecl.isVar ? "var " : "let ";
    PrintIndent(indent, "VarDecl:", key + varDecl.identifier.Val(), "{");
    PrintBasic(indent + ONE_INDENT, varDecl);
    PrintAnnotations(varDecl, indent + ONE_INDENT);
    PrintModifiers(varDecl, indent);
    if (varDecl.isIdentifierCompilerAdd) {
        PrintIndent(indent + ONE_INDENT, "isIdentifierCompilerAdd:", varDecl.isIdentifierCompilerAdd);
    }
    if (varDecl.isResourceVar) {
        PrintIndent(indent + ONE_INDENT, "isResourceVar:", varDecl.isResourceVar);
    }
    if (varDecl.isMemberParam) {
        PrintIndent(indent + ONE_INDENT, "isMemberParam:", varDecl.isMemberParam);
    }
    if (varDecl.parentPattern) {
        PrintNode(varDecl.parentPattern, indent + ONE_INDENT, "parentPattern");
    }
    PrintNode(varDecl.type.get(), indent + ONE_INDENT, "type");
    PrintNode(varDecl.initializer.get(), indent + ONE_INDENT, "initializer");
    PrintIndent(indent, "}");
}

void PrintBasicpeAliasDecl(unsigned indent, const TypeAliasDecl& alias)
{
    PrintIndent(indent, "TypeAliasDecl", alias.identifier.Val(), "{");
    PrintBasic(indent + ONE_INDENT, alias);
    PrintAnnotations(alias, indent + ONE_INDENT);
    PrintModifiers(alias, indent);
    if (alias.generic) {
        PrintGeneric(indent + ONE_INDENT, *alias.generic);
    }
    PrintNode(alias.type.get(), indent + ONE_INDENT, "type");
    PrintIndent(indent, "}");
}

void PrintClassDecl(unsigned indent, const ClassDecl& classDecl)
{
    if (auto attr = HasJavaAttr(classDecl); attr) {
        std::string argStr = attr.value() == Attribute::JAVA_APP ? "app" : "ext";
        PrintIndent(indent, "@Java[\"" + argStr + "\"]");
    }
    PrintIndent(indent, "ClassDecl:", classDecl.identifier.Val(), "{");
    PrintBasic(indent + ONE_INDENT, classDecl);
    PrintAnnotations(classDecl, indent + ONE_INDENT);
    PrintModifiers(classDecl, indent);
    if (classDecl.generic) {
        PrintGeneric(indent + ONE_INDENT, *classDecl.generic);
    }
    if (!classDecl.inheritedTypes.empty()) {
        PrintIndent(indent + ONE_INDENT, "inheritedTypes [");
        for (auto& it : classDecl.inheritedTypes) {
            PrintNode(it.get(), indent + TWO_INDENT);
        }
        PrintIndent(indent + ONE_INDENT, "]");
    }
    if (!classDecl.subDecls.empty()) {
        PrintIndent(indent + ONE_INDENT, "subDecls [");
        for (auto sub : classDecl.subDecls) {
            if (sub) {
                PrintTarget(indent + TWO_INDENT, *sub, "subDecl");
            }
        }
        PrintIndent(indent + ONE_INDENT, "]");
    }
    if (classDecl.body) {
        PrintNode(classDecl.body.get(), indent + ONE_INDENT);
    }
    PrintIndent(indent, "}");
}

void PrintInterfaceDecl(unsigned indent, const InterfaceDecl& interfaceDecl)
{
    if (auto attr = HasJavaAttr(interfaceDecl); attr) {
        PrintIndent(indent, "@Java[\"", attr.value() == Attribute::JAVA_APP ? "app" : "ext", "\"]");
    }
    PrintIndent(indent, "InterfaceDecl:", interfaceDecl.identifier.Val(), "{");
    PrintBasic(indent + ONE_INDENT, interfaceDecl);
    PrintAnnotations(interfaceDecl, indent + ONE_INDENT);
    PrintModifiers(interfaceDecl, indent);
    if (interfaceDecl.generic) {
        PrintGeneric(indent + ONE_INDENT, *interfaceDecl.generic);
    }
    if (!interfaceDecl.inheritedTypes.empty()) {
        PrintIndent(indent + ONE_INDENT, "inheritedTypes: [");
        for (auto& it : interfaceDecl.inheritedTypes) {
            PrintNode(it.get(), indent + TWO_INDENT);
        }
        PrintIndent(indent + ONE_INDENT, "]");
    }
    if (!interfaceDecl.subDecls.empty()) {
        PrintIndent(indent + ONE_INDENT, "subDecls [");
        for (auto sub : interfaceDecl.subDecls) {
            if (sub) {
                PrintTarget(indent + TWO_INDENT, *sub, "subDecl");
            }
        }
        PrintIndent(indent + ONE_INDENT, "]");
    }
    if (interfaceDecl.body) {
        PrintNode(interfaceDecl.body.get(), indent + ONE_INDENT);
    }
    PrintIndent(indent, "}");
}

void PrintEnumDecl(unsigned indent, const EnumDecl& enumDecl)
{
    PrintIndent(indent, "EnumDecl:", enumDecl.identifier.Val(), "{");
    PrintBasic(indent + ONE_INDENT, enumDecl);
    PrintAnnotations(enumDecl, indent + ONE_INDENT);
    PrintModifiers(enumDecl, indent);
    if (enumDecl.generic) {
        PrintGeneric(indent + ONE_INDENT, *enumDecl.generic);
    }
    if (!enumDecl.inheritedTypes.empty()) {
        PrintIndent(indent + ONE_INDENT, "inheritedTypes [");
        for (auto& it : enumDecl.inheritedTypes) {
            PrintNode(it.get(), indent + TWO_INDENT);
        }
        PrintIndent(indent + ONE_INDENT, "]");
    }
    if (!enumDecl.constructors.empty()) {
        PrintIndent(indent + ONE_INDENT, "constructors [");
        for (auto& it : enumDecl.constructors) {
            PrintNode(it.get(), indent + TWO_INDENT);
        }
        PrintIndent(indent + ONE_INDENT, "]");
    }
    if (!enumDecl.members.empty()) {
        PrintIndent(indent + ONE_INDENT, "members [");
        for (auto& it : enumDecl.members) {
            PrintNode(it.get(), indent + TWO_INDENT);
        }
        PrintIndent(indent + ONE_INDENT, "]");
    }
    if (enumDecl.hasArguments) {
        PrintIndent(indent + ONE_INDENT, "hasArguments:", enumDecl.hasArguments);
    }
    if (enumDecl.hasEllipsis) {
        PrintIndent(indent + ONE_INDENT, "hasEllipsis:", enumDecl.hasEllipsis);
    }
    PrintIndent(indent, "}");
}

void PrintStructDecl(unsigned indent, const StructDecl& decl)
{
    if (decl.TestAttr(Attribute::C)) {
        PrintIndent(indent, "@C");
    }
    PrintIndent(indent, "StructDecl:", decl.identifier.Val(), "{");
    PrintBasic(indent + ONE_INDENT, decl);
    PrintAnnotations(decl, indent + ONE_INDENT);
    PrintModifiers(decl, indent);
    if (decl.generic) {
        PrintGeneric(indent + ONE_INDENT, *decl.generic);
    }
    if (!decl.inheritedTypes.empty()) {
        PrintIndent(indent + ONE_INDENT, "inheritedTypes [");
        for (auto& it : decl.inheritedTypes) {
            PrintNode(it.get(), indent + TWO_INDENT);
        }
        PrintIndent(indent + ONE_INDENT, "]");
    }
    PrintIndent(indent + ONE_INDENT, "StructBody {");
    if (decl.body) {
        PrintNode(decl.body.get(), indent + TWO_INDENT);
    }
    PrintIndent(indent + ONE_INDENT, "}");
    PrintIndent(indent, "}");
}

void PrintExtendDecl(unsigned indent, const ExtendDecl& ed)
{
    PrintIndent(indent, "ExtendDecl {");
    PrintBasic(indent + ONE_INDENT, ed);
    PrintAnnotations(ed, indent + ONE_INDENT);
    PrintModifiers(ed, indent);
    if (ed.extendedType) {
        PrintNode(ed.extendedType.get(), indent + ONE_INDENT, "extendedType");
    }
    if (!ed.inheritedTypes.empty()) {
        PrintIndent(indent + ONE_INDENT, "inheritedTypes [");
        for (auto& interface : ed.inheritedTypes) {
            PrintNode(interface.get(), indent + TWO_INDENT);
        }
        PrintIndent(indent + ONE_INDENT, "]");
    }
    if (ed.generic) {
        PrintGeneric(indent + ONE_INDENT, *ed.generic);
    }
    if (!ed.members.empty()) {
        PrintIndent(indent + ONE_INDENT, "extendMembers [");
        for (auto& it : ed.members) {
            PrintNode(it.get(), indent + TWO_INDENT);
        }
        PrintIndent(indent + ONE_INDENT, "]");
    }
    PrintIndent(indent, "}");
}

void PrintIfExpr(unsigned indent, const IfExpr& expr)
{
    PrintIndent(indent, "IfExpr {");
    PrintBasic(indent + ONE_INDENT, expr);
    if (expr.isElseIf) {
        PrintIndent(indent + ONE_INDENT, "isElseIf:", expr.isElseIf);
    }
    if (expr.sugarKind != Expr::SugarKind::NO_SUGAR) {
        PrintIndent(indent + ONE_INDENT, "sugarKind:", SugarKindName(expr.sugarKind));
    }
    if (expr.condExpr) {
        PrintNode(expr.condExpr.get(), indent + ONE_INDENT, "condExpr");
    }
    if (expr.thenBody) {
        PrintNode(expr.thenBody.get(), indent + ONE_INDENT, "thenBody");
    }
    if (expr.hasElse && expr.elseBody) {
        PrintNode(expr.elseBody.get(), indent + ONE_INDENT, "elseBody");
    }
    PrintIndent(indent, "}");
}

void PrintMatchCase(unsigned indent, const MatchCase& mc)
{
    PrintIndent(indent, "MatchCase: {");
    PrintBasic(indent + ONE_INDENT, mc);
    PrintIndent(indent + ONE_INDENT, "patterns {");
    for (auto& it : mc.patterns) {
        if (it) {
            PrintNode(it.get(), indent + TWO_INDENT);
        }
    }
    PrintIndent(indent + ONE_INDENT, "}");
    if (mc.patternGuard) {
        PrintNode(mc.patternGuard.get(), indent + ONE_INDENT, "patternGuard");
    }
    if (mc.exprOrDecls) {
        PrintIndent(indent + ONE_INDENT, "exprOrDecls [");
        for (auto& it : mc.exprOrDecls->body) {
            if (it) {
                PrintNode(it.get(), indent + TWO_INDENT);
            }
        }
        PrintIndent(indent + ONE_INDENT, "]");
    }

    PrintIndent(indent, "}");
}

void PrintMatchCaseOther(unsigned indent, const MatchCaseOther& mc)
{
    PrintIndent(indent, "MatchCaseOther {");
    PrintBasic(indent + ONE_INDENT, mc);
    if (mc.matchExpr) {
        PrintNode(mc.matchExpr.get(), indent + ONE_INDENT, "matchExpr");
    }
    if (mc.exprOrDecls) {
        PrintNode(mc.exprOrDecls.get(), indent + ONE_INDENT, "exprOrDecls");
    }
    PrintIndent(indent, "}");
}

void PrintMatchExpr(unsigned indent, const MatchExpr& expr)
{
    PrintIndent(indent, "MatchExpr:", "{");
    PrintBasic(indent + ONE_INDENT, expr);
    if (expr.sugarKind != Expr::SugarKind::NO_SUGAR) {
        PrintIndent(indent + ONE_INDENT, "sugarKind:", SugarKindName(expr.sugarKind));
    }
    if (expr.matchMode) {
        PrintIndent(indent + ONE_INDENT, "selector: {");
        PrintNode(expr.selector.get(), indent + TWO_INDENT);
        PrintIndent(indent + ONE_INDENT, "}");

        PrintIndent(indent + ONE_INDENT, "matchCases: [");
        for (auto& matchCase : expr.matchCases) {
            if (!matchCase) {
                continue;
            }
            PrintMatchCase(indent + TWO_INDENT, *StaticCast<MatchCase*>(matchCase.get()));
        }
        PrintIndent(indent + ONE_INDENT, "]");
    } else {
        for (auto& matchCaseOther : expr.matchCaseOthers) {
            if (!matchCaseOther) {
                continue;
            }
            PrintIndent(indent + ONE_INDENT, "matchCases: {");
            PrintNode(matchCaseOther->matchExpr.get(), indent + TWO_INDENT);
            PrintIndent(indent + ONE_INDENT, "}");

            if (matchCaseOther->exprOrDecls) {
                PrintIndent(indent + ONE_INDENT, "exprOrDecls: [");
                for (auto& it : matchCaseOther->exprOrDecls->body) {
                    PrintNode(it.get(), indent + ONE_INDENT);
                }
                PrintIndent(indent + ONE_INDENT, "]");
            }
        }
    }
    PrintIndent(indent, "}");
}

void PrintTryExpr(unsigned indent, const TryExpr& expr)
{
    PrintIndent(indent, "TryExpr", "{");
    PrintBasic(indent + ONE_INDENT, expr);
    if (expr.isIllegalResourceSpec) {
        PrintIndent(indent + ONE_INDENT, "isIllegalResourceSpec:", expr.isIllegalResourceSpec);
    }
    if (expr.isDesugaredFromTryWithResources) {
        PrintIndent(indent + ONE_INDENT,
            "isDesugaredFromTryWithResources:",
            expr.isDesugaredFromTryWithResources);
    }
    if (expr.isDesugaredFromSyncBlock) {
        PrintIndent(indent + ONE_INDENT, "isDesugaredFromSyncBlock:", expr.isDesugaredFromSyncBlock);
    }
    if (!expr.resourceSpec.empty()) {
        PrintIndent(indent + ONE_INDENT, "ResourceSpec", "{");
        for (auto& i : expr.resourceSpec) {
            PrintNode(i.get(), indent + TWO_INDENT);
        }
        PrintIndent(indent + ONE_INDENT, "}");
    }
    if (expr.tryBlock) {
        PrintIndent(indent + ONE_INDENT, "TryBlock", "{");
        PrintNode(expr.tryBlock.get(), indent + TWO_INDENT);
        PrintIndent(indent + ONE_INDENT, "}");
    }
    const auto catchCount = std::min(expr.catchPatterns.size(), expr.catchBlocks.size());
    for (size_t i = 0; i < catchCount; i++) {
        PrintIndent(indent + ONE_INDENT, "Catch", "{");
        if (expr.catchPatterns[i]) {
            PrintIndent(indent + TWO_INDENT, "CatchPattern", "{");
            PrintNode(expr.catchPatterns[i].get(), indent + THREE_INDENT);
            PrintIndent(indent + TWO_INDENT, "}");
        }
        if (expr.catchBlocks[i]) {
            PrintIndent(indent + TWO_INDENT, "CatchBlock", "{");
            PrintNode(expr.catchBlocks[i].get(), indent + THREE_INDENT);
            PrintIndent(indent + TWO_INDENT, "}");
        }
        PrintIndent(indent + ONE_INDENT, "}");
    }
    for (const auto& handler : expr.handlers) {
        PrintIndent(indent + ONE_INDENT, "Handle", "{");
        if (handler.commandPattern) {
            PrintIndent(indent + TWO_INDENT, "HandlePattern", "{");
            PrintIndent(indent + THREE_INDENT, "CommandTypePattern:");
            PrintNode(handler.commandPattern.get(), indent + THREE_INDENT);
            PrintIndent(indent + TWO_INDENT, "}");
        }
        if (handler.block) {
            PrintIndent(indent + TWO_INDENT, "HandleBlock", "{");
            PrintNode(handler.block.get(), indent + THREE_INDENT);
            PrintIndent(indent + TWO_INDENT, "}");
        }
        if (handler.desugaredLambda) {
            PrintIndent(indent + TWO_INDENT, "DesugaredLambda", "{");
            PrintNode(handler.desugaredLambda.get(), indent + THREE_INDENT);
            PrintIndent(indent + TWO_INDENT, "}");
        }
        PrintIndent(indent + ONE_INDENT, "}");
    }
    if (expr.finallyBlock) {
        PrintIndent(indent + ONE_INDENT, "FinallyBlock", "{");
        PrintNode(expr.finallyBlock.get(), indent + TWO_INDENT);
        PrintIndent(indent + ONE_INDENT, "}");
    }
    if (!expr.handlers.empty()) {
        if (expr.tryLambda) {
            PrintIndent(indent + ONE_INDENT, "TryLambda", "{");
            PrintNode(expr.tryLambda.get(), indent + TWO_INDENT);
            PrintIndent(indent + ONE_INDENT, "}");
        }
        if (expr.finallyLambda) {
            PrintIndent(indent + ONE_INDENT, "FinallyLambda", "{");
            PrintNode(expr.finallyLambda.get(), indent + TWO_INDENT);
            PrintIndent(indent + ONE_INDENT, "}");
        }
    }
    PrintIndent(indent, "}");
}

void PrintThrowExpr(unsigned indent, const ThrowExpr& expr)
{
    PrintIndent(indent, "ThrowExpr", "{");
    PrintBasic(indent + ONE_INDENT, expr);
    PrintNode(expr.expr.get(), indent + ONE_INDENT, "expr");
    PrintIndent(indent, "}");
}

void PrintPerformExpr(unsigned indent, const PerformExpr& expr)
{
    PrintIndent(indent, "PerformExpr", "{");
    PrintBasic(indent + ONE_INDENT, expr);
    if (expr.expr) {
        PrintNode(expr.expr.get(), indent + ONE_INDENT, "PerformExpr");
    }
    PrintIndent(indent, "}");
}

void PrintResumeExpr(unsigned indent, const ResumeExpr& expr)
{
    PrintIndent(indent, "ResumeExpr", "{");
    PrintBasic(indent + ONE_INDENT, expr);
    if (expr.enclosing.has_value() && expr.enclosing.value() != nullptr) {
        PrintIndent(indent + ONE_INDENT, "enclosingHandler: ptr:",
            static_cast<const void*>(expr.enclosing.value().get()));
    }
    if (expr.withExpr) {
        PrintNode(expr.withExpr.get(), indent + ONE_INDENT, "WithExpr");
    }
    if (expr.throwingExpr) {
        PrintNode(expr.throwingExpr.get(), indent + ONE_INDENT, "ThrowingExpr");
    }
    PrintIndent(indent, "}");
}

void PrintReturnExpr(unsigned indent, const ReturnExpr& expr)
{
    PrintIndent(indent, "ReturnExpr", "{");
    PrintBasic(indent + ONE_INDENT, expr);
    if (expr.expr) {
        PrintNode(expr.expr.get(), indent + ONE_INDENT, "expr");
    }
    if (expr.refFuncBody) {
        PrintIndent(indent + ONE_INDENT, "refFuncBody: ptr:", static_cast<const void*>(expr.refFuncBody));
    }
    PrintIndent(indent, "}");
}

void PrintJumpExpr(unsigned indent, const JumpExpr& expr)
{
    std::string str1 = expr.isBreak ? "Break" : "Continue";
    PrintIndent(indent, "JumpExpr:", str1);
    PrintBasic(indent + ONE_INDENT, expr);
    if (expr.refLoop) {
        PrintIndent(indent + ONE_INDENT, "refLoop: ptr:", static_cast<const void*>(expr.refLoop.get()));
    }
}

void PrintForInExpr(unsigned indent, const ForInExpr& expr)
{
    PrintIndent(indent, "ForInExpr {");
    PrintBasic(indent + ONE_INDENT, expr);
    if (expr.forInKind != ForInKind::FORIN_INVALID) {
        PrintIndent(indent + ONE_INDENT, "forInKind:", ForInKindName(expr.forInKind));
    }
    if (expr.patternInDesugarExpr) {
        PrintIndent(indent + ONE_INDENT, "patternInDesugarExpr: ptr:",
            static_cast<const void*>(expr.patternInDesugarExpr.get()));
    }
    if (expr.pattern) {
        PrintIndent(indent + ONE_INDENT, "pattern", ":");
        PrintNode(expr.pattern.get(), indent + TWO_INDENT);
    }
    if (expr.inExpression) {
        PrintIndent(indent + ONE_INDENT, "inExpression", ":");
        PrintNode(expr.inExpression.get(), indent + TWO_INDENT);
    }
    if (expr.patternGuard) {
        PrintIndent(indent + ONE_INDENT, "patternGuard", ":");
        PrintNode(expr.patternGuard.get(), indent + TWO_INDENT);
    }
    if (expr.body) {
        PrintNode(expr.body.get(), indent + ONE_INDENT, "body");
    }
    PrintIndent(indent, "}");
}

void PrintWhileExpr(unsigned indent, const WhileExpr& expr)
{
    PrintIndent(indent, "WhileExpr {");
    PrintBasic(indent + ONE_INDENT, expr);
    if (expr.sugarKind != Expr::SugarKind::NO_SUGAR) {
        PrintIndent(indent + ONE_INDENT, "sugarKind:", SugarKindName(expr.sugarKind));
    }
    if (expr.condExpr) {
        PrintIndent(indent + ONE_INDENT, "condExpr", "{");
        PrintNode(expr.condExpr.get(), indent + TWO_INDENT, "condition expr");
        PrintIndent(indent + ONE_INDENT, "}");
    }
    if (expr.body) {
        PrintNode(expr.body.get(), indent + ONE_INDENT, "While-Body");
    }
    PrintIndent(indent, "}");
}

void PrintDoWhileExpr(unsigned indent, const DoWhileExpr& expr)
{
    PrintIndent(indent, "DoWhileExpr {");
    PrintBasic(indent + ONE_INDENT, expr);
    if (expr.body) {
        PrintNode(expr.body.get(), indent + ONE_INDENT, "DoWhile-Body");
    }
    if (expr.condExpr) {
        PrintIndent(indent + ONE_INDENT, "condExpr", "{");
        PrintNode(expr.condExpr.get(), indent + TWO_INDENT);
        PrintIndent(indent + ONE_INDENT, "}");
    }
    PrintIndent(indent, "}");
}

void PrintAssignExpr(unsigned indent, const AssignExpr& expr)
{
    const std::unordered_map<TokenKind, std::string> op2str = {
        {TokenKind::ASSIGN, "'='"},
        {TokenKind::ADD_ASSIGN, "'+='"},
        {TokenKind::SUB_ASSIGN, "'-='"},
        {TokenKind::MUL_ASSIGN, "'*='"},
        {TokenKind::EXP_ASSIGN, "'**='"},
        {TokenKind::DIV_ASSIGN, "'/='"},
        {TokenKind::MOD_ASSIGN, "'%%='"},
        {TokenKind::AND_ASSIGN, "'&&='"},
        {TokenKind::OR_ASSIGN, "'||='"},
        {TokenKind::BITAND_ASSIGN, "'&='"},
        {TokenKind::BITOR_ASSIGN, "'|='"},
        {TokenKind::BITXOR_ASSIGN, "'^='"},
        {TokenKind::LSHIFT_ASSIGN, "'<<='"},
        {TokenKind::RSHIFT_ASSIGN, "'>>='"},
    };
    PrintIndent(indent, "AssignExpr", "{");
    PrintBasic(indent + ONE_INDENT, expr);
    std::string str1;
    auto result = op2str.find(expr.op);
    if (result != op2str.end()) {
        str1 = result->second;
    }
    PrintIndent(indent + ONE_INDENT, str1);
    if (expr.leftValue) {
        PrintNode(expr.leftValue.get(), indent + ONE_INDENT, "leftValue");
    }
    if (expr.rightExpr) {
        PrintNode(expr.rightExpr.get(), indent + ONE_INDENT, "rightExpr");
    }
    if (expr.overflowStrategy != OverflowStrategy::NA) {
        PrintIndent(indent + ONE_INDENT, "overflowStrategy:", OverflowStrategyName(expr.overflowStrategy));
    }
    if (expr.isCompound) {
        PrintIndent(indent + ONE_INDENT, "isCompound:", expr.isCompound);
    }
    PrintIndent(indent, "}");
}

void PrintIncOrDecExpr(unsigned indent, const IncOrDecExpr& expr)
{
    PrintIndent(indent, "IncOrDecExpr", "{");
    PrintBasic(indent + ONE_INDENT, expr);
    std::string str1 = (expr.op == TokenKind::INCR) ? "++" : "--";
    PrintIndent(indent + ONE_INDENT, str1);
    PrintNode(expr.expr.get(), indent + ONE_INDENT);
    if (expr.overflowStrategy != OverflowStrategy::NA) {
        PrintIndent(indent + ONE_INDENT, "overflowStrategy:", OverflowStrategyName(expr.overflowStrategy));
    }
    PrintIndent(indent, "}");
}

void PrintUnaryExpr(unsigned indent, const UnaryExpr& unaryExpr)
{
    PrintIndent(indent, "UnaryExpr", "{", TOKENS[static_cast<int>(unaryExpr.op)]);
    PrintBasic(indent + ONE_INDENT, unaryExpr);
    if (unaryExpr.expr) {
        PrintNode(unaryExpr.expr.get(), indent + ONE_INDENT);
    }
    if (unaryExpr.overflowStrategy != OverflowStrategy::NA) {
        PrintIndent(indent + ONE_INDENT, "overflowStrategy:", OverflowStrategyName(unaryExpr.overflowStrategy));
    }
    PrintIndent(indent, "}");
}

void PrintBinaryExpr(unsigned indent, const BinaryExpr& binaryExpr)
{
    PrintIndent(indent, "BinaryExpr ", TOKENS[static_cast<int>(binaryExpr.op)], "{");
    PrintBasic(indent + ONE_INDENT, binaryExpr);
    if (binaryExpr.leftExpr) {
        PrintNode(binaryExpr.leftExpr.get(), indent + ONE_INDENT, "leftExpr:");
    }
    if (binaryExpr.rightExpr) {
        PrintNode(binaryExpr.rightExpr.get(), indent + ONE_INDENT, "rightExpr:");
    }
    if (binaryExpr.overflowStrategy != OverflowStrategy::NA) {
        PrintIndent(indent + ONE_INDENT, "overflowStrategy:", OverflowStrategyName(binaryExpr.overflowStrategy));
    }
    PrintIndent(indent, "}");
}

void PrintRangeExpr(unsigned indent, const RangeExpr& rangeExpr)
{
    PrintIndent(indent, "RangeExpr {");
    PrintBasic(indent + ONE_INDENT, rangeExpr);
    if (rangeExpr.decl) {
        PrintTarget(indent + ONE_INDENT, *rangeExpr.decl, "decl");
    }
    if (!rangeExpr.isClosed) {
        PrintIndent(indent + ONE_INDENT, "isClosed:", rangeExpr.isClosed);
    }
    if (rangeExpr.startExpr) {
        PrintIndent(indent + ONE_INDENT, "Start:");
        PrintNode(rangeExpr.startExpr.get(), indent + TWO_INDENT);
    }
    if (rangeExpr.stopExpr) {
        PrintIndent(indent + ONE_INDENT, "Stop:");
        PrintNode(rangeExpr.stopExpr.get(), indent + TWO_INDENT);
    }
    if (rangeExpr.stepExpr) {
        PrintIndent(indent + ONE_INDENT, "Step:");
        PrintNode(rangeExpr.stepExpr.get(), indent + TWO_INDENT);
    }
    PrintIndent(indent, "}");
}

void PrintSubscriptExpr(unsigned indent, const SubscriptExpr& expr)
{
    PrintIndent(indent, "SubscriptExpr {");
    PrintBasic(indent + ONE_INDENT, expr);
    if (expr.isTupleAccess) {
        PrintIndent(indent + ONE_INDENT, "isTupleAccess:", expr.isTupleAccess);
    }
    if (expr.baseExpr) {
        PrintNode(expr.baseExpr.get(), indent + ONE_INDENT, "baseExpr");
    }
    if (!expr.indexExprs.empty()) {
        PrintIndent(indent + ONE_INDENT, "indexExprs [");
        for (auto& it : expr.indexExprs) {
            if (it) {
                PrintNode(it.get(), indent + TWO_INDENT);
            }
        }
        PrintIndent(indent + ONE_INDENT, "]");
    }
    PrintIndent(indent, "}");
}

void PrintMemberAccess(unsigned indent, const MemberAccess& expr)
{
    PrintIndent(indent, "MemberAccess", "{");
    PrintBasic(indent + ONE_INDENT, expr);
    PrintNameReferenceInfo(indent, expr);
    PrintNode(expr.baseExpr.get(), indent + ONE_INDENT, "baseExpr");
    PrintIndent(indent + ONE_INDENT, "field:", expr.field.Val());
    if (!expr.typeArguments.empty()) {
        PrintIndent(indent + ONE_INDENT, "typeArguments [");
        for (auto& it : expr.typeArguments) {
            PrintNode(it.get(), indent + TWO_INDENT);
        }
        PrintIndent(indent + ONE_INDENT, "]");
    }
    if (expr.target) {
        PrintTarget(indent + ONE_INDENT, *expr.target);
    }
    if (!expr.targets.empty()) {
        PrintIndent(indent + ONE_INDENT, "targets [");
        for (auto t : expr.targets) {
            if (!t) {
                continue;
            }
            PrintTarget(indent + TWO_INDENT, *t);
        }
        PrintIndent(indent + ONE_INDENT, "]");
    }
    if (!expr.foundUpperBoundMap.empty()) {
        PrintIndent(indent + ONE_INDENT, "foundUpperBoundMap:", expr.foundUpperBoundMap.size());
    }
    if (expr.isExposedAccess) {
        PrintIndent(indent + ONE_INDENT, "isExposedAccess:", expr.isExposedAccess);
    }
    if (expr.isPattern) {
        PrintIndent(indent + ONE_INDENT, "isPattern:", expr.isPattern);
    }
    PrintIndent(indent, "}");
}

void PrintOptionalExpr(unsigned indent, const OptionalExpr& optionalExpr)
{
    PrintIndent(indent, "OptionalExpr", "{");
    PrintBasic(indent + ONE_INDENT, optionalExpr);
    if (optionalExpr.baseExpr) {
        PrintIndent(indent + ONE_INDENT, "BaseExpr:");
        PrintNode(optionalExpr.baseExpr.get(), indent + ONE_INDENT);
    }
    PrintIndent(indent, "}");
}

void PrintOptionalChainExpr(unsigned indent, const OptionalChainExpr& optionalChainExpr)
{
    PrintIndent(indent, "OptionalChainExpr", "{");
    PrintBasic(indent + ONE_INDENT, optionalChainExpr);
    if (optionalChainExpr.expr) {
        PrintIndent(indent + ONE_INDENT, "Expr:");
        PrintNode(optionalChainExpr.expr.get(), indent + ONE_INDENT);
    }
    PrintIndent(indent, "}");
}

void PrintExprNode(unsigned indent, const Expr& expr)
{
    PrintIndent(indent, "Expr {");
    PrintBasic(indent + ONE_INDENT, expr);
    if (expr.overflowStrategy != OverflowStrategy::NA) {
        PrintIndent(indent + ONE_INDENT, "overflowStrategy:", OverflowStrategyName(expr.overflowStrategy));
    }
    if (expr.isConst) {
        PrintIndent(indent + ONE_INDENT, "isConst:", expr.isConst);
    }
    if (expr.hasQuestSuffix) {
        PrintIndent(indent + ONE_INDENT, "hasQuestSuffix:", expr.hasQuestSuffix);
    }
    if (expr.hasSemi) {
        PrintIndent(indent + ONE_INDENT, "hasSemi:", expr.hasSemi);
    }
    if (expr.isBaseFunc) {
        PrintIndent(indent + ONE_INDENT, "isBaseFunc:", expr.isBaseFunc);
    }
    if (expr.isInFlowExpr) {
        PrintIndent(indent + ONE_INDENT, "isInFlowExpr:", expr.isInFlowExpr);
    }
    if (expr.sourceExpr) {
        PrintNode(expr.sourceExpr, indent + ONE_INDENT, "sourceExpr");
    }
    if (expr.mapExpr) {
        PrintNode(expr.mapExpr, indent + ONE_INDENT, "mapExpr");
    }
    PrintIndent(indent, "}");
}

void PrintPointerExpr(unsigned indent, const PointerExpr& expr)
{
    PrintIndent(indent, "PointerExpr {");
    PrintBasic(indent + ONE_INDENT, expr);
    PrintNode(expr.type.get(), indent + ONE_INDENT, "type");
    PrintNode(expr.arg.get(), indent + ONE_INDENT, "arg");
    PrintIndent(indent, "}");
}

void PrintTrailingClosureExpr(unsigned indent, const TrailingClosureExpr& trailing)
{
    PrintIndent(indent, "TrailingClosureExpr {");
    PrintBasic(indent + ONE_INDENT, trailing);
    PrintNode(trailing.expr.get(), indent + ONE_INDENT, "expr");
    PrintNode(trailing.lambda.get(), indent + ONE_INDENT, "lambda");
    PrintIndent(indent, "}");
}

void PrintLetPatternDestructor(unsigned indent, const LetPatternDestructor& expr)
{
    PrintIndent(indent, "LetPattern", "{");
    PrintBasic(indent + ONE_INDENT, expr);
    PrintIndent(indent + ONE_INDENT, "patterns [");
    for (auto& p : expr.patterns) {
        PrintIndent(indent + TWO_INDENT, "|", "{");
        PrintNode(p.get(), indent + THREE_INDENT);
        PrintIndent(indent + TWO_INDENT, "}");
    }
    PrintIndent(indent + ONE_INDENT, "]");
    if (expr.initializer) {
        PrintIndent(indent + ONE_INDENT, "Initializer:", "{");
        PrintNode(expr.initializer.get(), indent + TWO_INDENT);
        PrintIndent(indent + ONE_INDENT, "}");
    }
    PrintIndent(indent, "}");
}

void PrintFuncArg(unsigned indent, const FuncArg& expr)
{
    PrintIndent(indent, "FuncArg", "{");
    PrintBasic(indent + ONE_INDENT, expr);
    if (!expr.name.Empty()) {
        PrintIndent(indent + ONE_INDENT, "ArgName:", expr.name.Val());
    }
    if (expr.withInout) {
        PrintIndent(indent + ONE_INDENT, "inout");
    }
    PrintNode(expr.expr.get(), indent + ONE_INDENT);
    PrintIndent(indent, "}");
}

void PrintCallExpr(unsigned indent, const CallExpr& expr)
{
    PrintIndent(indent, "CallExpr {");
    PrintBasic(indent + ONE_INDENT, expr);
    if (expr.callKind != CallKind::CALL_INVALID) {
        PrintIndent(indent + ONE_INDENT, "callKind:", CallKindName(expr.callKind));
    }
    if (expr.sugarKind != Expr::SugarKind::NO_SUGAR) {
        PrintIndent(indent + ONE_INDENT, "sugarKind:", SugarKindName(expr.sugarKind));
    }
    if (expr.needCheckToTokens) {
        PrintIndent(indent + ONE_INDENT, "needCheckToTokens:", expr.needCheckToTokens);
    }
    PrintIndent(indent + ONE_INDENT, "BaseFunc {");
    PrintNode(expr.baseFunc.get(), indent + TWO_INDENT);
    PrintIndent(indent + ONE_INDENT, "}");
    if (!expr.args.empty()) {
        PrintIndent(indent + ONE_INDENT, "arguments [");
        for (auto& it : expr.args) {
            PrintNode(it.get(), indent + TWO_INDENT);
        }
        PrintIndent(indent + ONE_INDENT, "]");
    }
    if (expr.desugarArgs.has_value()) {
        PrintIndent(indent + ONE_INDENT, "desugarArgs [");
        // desugarArgs stores Ptr<FuncArg>, iterate by reference and extract
        // raw pointer if present.
        for (auto &argPtr : expr.desugarArgs.value()) {
            if (auto *arg = argPtr.get()) {
                PrintNode(arg, indent + TWO_INDENT);
            }
        }
        PrintIndent(indent + ONE_INDENT, "]");
    }
    if (!expr.defaultArgs.empty()) {
        PrintIndent(indent + ONE_INDENT, "defaultArgs [");
        for (auto& it : expr.defaultArgs) {
            PrintNode(it.get(), indent + TWO_INDENT);
        }
        PrintIndent(indent + ONE_INDENT, "]");
    }
    if (expr.resolvedFunction) {
        PrintTarget(indent + ONE_INDENT, *expr.resolvedFunction, "resolvedFunction");
    }
    PrintIndent(indent, "}");
}

void PrintParenExpr(unsigned indent, const ParenExpr& parenExpr)
{
    PrintIndent(indent, "ParenExpr {");
    PrintBasic(indent + ONE_INDENT, parenExpr);
    PrintNode(parenExpr.expr.get(), indent + ONE_INDENT);
    PrintIndent(indent, "}");
}

void PrintLambdaExpr(unsigned indent, const LambdaExpr& expr)
{
    PrintIndent(indent, "LambdaExpr {");
    PrintBasic(indent + ONE_INDENT, expr);
    if (!expr.mangledName.empty()) {
        PrintIndent(indent + ONE_INDENT, "mangledName:", expr.mangledName);
    }
    PrintNode(expr.funcBody.get(), indent + ONE_INDENT, "LambdaExpr-Body");
    PrintIndent(indent, "}");
}

void PrintLitConstExpr(unsigned indent, const LitConstExpr& expr)
{
    PrintIndent(
        indent, "LitConstExpr:", expr.LitKindToString(), "\"" + StringConvertor::Recover(expr.stringValue) + "\"", "{");
    PrintBasic(indent + ONE_INDENT, expr);
    if (expr.ref) {
        PrintNode(expr.ref.get(), indent + ONE_INDENT, "ref");
    }
    if (!expr.rawString.empty() && expr.rawString != expr.stringValue) {
        PrintIndent(indent + ONE_INDENT, "rawString:", "\"" + expr.rawString + "\"");
    }
    if (expr.stringKind != StringKind::NORMAL) {
        PrintIndent(indent + ONE_INDENT, "stringKind:", static_cast<int>(expr.stringKind));
    }
    if (expr.isSingleQuote) {
        PrintIndent(indent + ONE_INDENT, "isSingleQuote:", expr.isSingleQuote);
    }
    if (expr.delimiterNum != 1U) {
        PrintIndent(indent + ONE_INDENT, "delimiterNum:", expr.delimiterNum);
    }
    if (!expr.codepoint.empty()) {
        std::stringstream ss;
        for (size_t i = 0; i < expr.codepoint.size(); ++i) {
            ss << expr.codepoint[i];
            if (i + 1 < expr.codepoint.size()) {
                ss << ", ";
            }
        }
        PrintIndent(indent + ONE_INDENT, "codepoint:", ss.str());
    }
    if (expr.siExpr) {
        PrintNode(expr.siExpr.get(), indent + ONE_INDENT, "siExpr");
    }
    PrintIndent(indent, "}");
}

void PrintInterpolationExpr(unsigned indent, const InterpolationExpr& ie)
{
    PrintIndent(indent, "InterpolationExpr:", "\"" + ie.rawString + "\"", "{");
    PrintBasic(indent + ONE_INDENT, ie);
    PrintNode(ie.block.get(), indent + ONE_INDENT, "block");
    PrintIndent(indent, "}");
}

void PrintStrInterpolationExpr(unsigned indent, const StrInterpolationExpr& expr)
{
    PrintIndent(indent, "StrInterpolationExpr:", "\"" + expr.rawString + "\"", "{");
    PrintBasic(indent + ONE_INDENT, expr);
    if (!expr.strParts.empty()) {
        PrintIndent(indent + ONE_INDENT, "strParts:", expr.strParts.size());
    }
    PrintIndent(indent + ONE_INDENT, "strPartExprs [");
    for (auto& sp : expr.strPartExprs) {
        PrintNode(sp.get(), indent + TWO_INDENT);
    }
    PrintIndent(indent + ONE_INDENT, "]");
    PrintIndent(indent, "}");
}

void PrintArrayLit(unsigned indent, const ArrayLit& expr)
{
    PrintIndent(indent, "ArrayLiteral", "{");
    PrintBasic(indent + ONE_INDENT, expr);
    if (expr.initFunc) {
        PrintTarget(indent + ONE_INDENT, *expr.initFunc, "initFunc");
    }
    if (!expr.children.empty()) {
        PrintIndent(indent + ONE_INDENT, "children [");
        for (auto& it : expr.children) {
            if (it) {
                PrintNode(it.get(), indent + TWO_INDENT);
            }
        }
        PrintIndent(indent + ONE_INDENT, "]");
    }
    PrintIndent(indent, "}");
}

void PrintArrayExpr(unsigned indent, const ArrayExpr& expr)
{
    auto typeName = expr.isValueArray ? "VArrayExpr" : "ArrayExpr";
    PrintIndent(indent, typeName, "{");
    PrintBasic(indent + ONE_INDENT, expr);
    if (expr.type) {
        PrintNode(expr.type.get(), indent + ONE_INDENT, "type");
    }
    if (expr.initFunc) {
        PrintTarget(indent + ONE_INDENT, *expr.initFunc, "initFunc");
    }
    if (!expr.args.empty()) {
        PrintIndent(indent + ONE_INDENT, "args [");
        for (size_t i = 0; i < expr.args.size(); ++i) {
            if (expr.args[i]) {
                PrintNode(expr.args[i].get(), indent + TWO_INDENT);
            }
        }
        PrintIndent(indent + ONE_INDENT, "]");
    }
    PrintIndent(indent, "}");
}

void PrintTupleLit(unsigned indent, const TupleLit& expr)
{
    PrintIndent(indent, "TupleLiteral", "{");
    PrintBasic(indent + ONE_INDENT, expr);
    for (auto& it : expr.children) {
        if (it) {
            PrintNode(it.get(), indent + ONE_INDENT);
        }
    }
    PrintIndent(indent, "}");
}

void PrintBasicpeConvExpr(unsigned indent, const TypeConvExpr& expr)
{
    PrintIndent(indent, "TypeConvExpr", "{");
    PrintBasic(indent + ONE_INDENT, expr);
    PrintIndent(indent + ONE_INDENT, "// Convert Type:");
    PrintNode(expr.type.get(), indent + TWO_INDENT);
    PrintIndent(indent + ONE_INDENT, "// Expr:");
    PrintNode(expr.expr.get(), indent + TWO_INDENT);
    PrintIndent(indent, "}");
}

void PrintRefExpr(unsigned indent, const RefExpr& refExpr)
{
    PrintIndent(indent, "RefExpr:", refExpr.ref.identifier.Val(), "{");
    PrintBasic(indent + ONE_INDENT, refExpr);
    PrintNameReferenceInfo(indent, refExpr);
    if (refExpr.isThis) {
        PrintIndent(indent + ONE_INDENT, "isThis:", refExpr.isThis);
    }
    if (refExpr.isSuper) {
        PrintIndent(indent + ONE_INDENT, "isSuper:", refExpr.isSuper);
    }
    if (refExpr.isQuoteDollar) {
        PrintIndent(indent + ONE_INDENT, "isQuoteDollar:", refExpr.isQuoteDollar);
    }
    if (!refExpr.typeArguments.empty()) {
        PrintIndent(indent + ONE_INDENT, "typeArguments [");
        for (auto& it : refExpr.typeArguments) {
            PrintNode(it.get(), indent + TWO_INDENT);
        }
        PrintIndent(indent + ONE_INDENT, "]");
    }
    if (refExpr.ref.target) {
        PrintTarget(indent + ONE_INDENT, *refExpr.ref.target);
    }
    if (!refExpr.ref.targets.empty()) {
        PrintIndent(indent + ONE_INDENT, "targets [");
        for (auto t : refExpr.ref.targets) {
            if (!t) {
                continue;
            }
            PrintTarget(indent + TWO_INDENT, *t);
        }
        PrintIndent(indent + ONE_INDENT, "]");
    }
    PrintIndent(indent, "}");
}

void PrintSpawnExpr(unsigned indent, const SpawnExpr& expr)
{
    PrintIndent(indent, "SpawnExpr {");
    PrintBasic(indent + ONE_INDENT, expr);
    if (expr.futureObj) {
        PrintNode(expr.futureObj.get(), indent + ONE_INDENT);
    }
    if (expr.arg) {
        PrintNode(expr.arg.get(), indent + ONE_INDENT);
    }
    if (expr.task) {
        PrintNode(expr.task.get(), indent + ONE_INDENT);
    }
    PrintIndent(indent, "}");
}

void PrintSynchronizedExpr(unsigned indent, const SynchronizedExpr& expr)
{
    PrintIndent(indent, "SynchronizedExpr {");
    PrintBasic(indent + ONE_INDENT, expr);
    if (expr.mutex) {
        PrintNode(expr.mutex.get(), indent + TWO_INDENT, "mutex");
    }
    if (expr.body) {
        PrintNode(expr.body.get(), indent + TWO_INDENT, "body");
    }
    PrintIndent(indent, "}");
}

void PrintIfAvailableExpr(unsigned indent, const IfAvailableExpr& expr)
{
    PrintIndent(indent, "IfAvailableExpr {");
    PrintBasic(indent + ONE_INDENT, expr);
    auto arg = expr.GetArg();
    if (arg) {
        PrintNode(arg, indent + TWO_INDENT, "arg");
    }
    auto lambda1 = expr.GetLambda1();
    if (lambda1) {
        PrintNode(lambda1, indent + TWO_INDENT, "lambda1");
    }
    auto lambda2 = expr.GetLambda2();
    if (lambda2) {
        PrintNode(lambda2, indent + TWO_INDENT, "lamdba2");
    }
    PrintIndent(indent, "}");
}

void PrintBlock(unsigned indent, const Block& block)
{
    PrintIndent(indent, "Block: {");
    PrintBasic(indent + ONE_INDENT, block);
    if (!block.unsafePos.IsZero()) {
        PrintIndent(indent + ONE_INDENT, "unsafe:", true);
    }
    for (auto& it : block.body) {
        PrintNode(it.get(), indent + ONE_INDENT);
    }
    PrintIndent(indent, "}");
}

void PrintInterfaceBody(unsigned indent, const InterfaceBody& body)
{
    PrintIndent(indent, "InterfaceBody {");
    for (auto& it : body.decls) {
        PrintNode(it.get(), indent + ONE_INDENT);
    }
    PrintIndent(indent, "}");
}

void PrintClassBody(unsigned indent, const ClassBody& body)
{
    PrintIndent(indent, "ClassBody {");
    for (auto& it : body.decls) {
        PrintNode(it.get(), indent + ONE_INDENT);
    }
    PrintIndent(indent, "}");
}

void PrintStructBody(unsigned indent, const StructBody& body)
{
    for (auto& it : body.decls) {
        PrintNode(it.get(), indent);
    }
}

void PrintDummyBody(unsigned indent, const DummyBody& body)
{
    PrintIndent(indent, "DummyBody {");
    PrintBasic(indent + ONE_INDENT, body);
    PrintIndent(indent, "}");
}

void PrintIsExpr(unsigned indent, const IsExpr& isExpr)
{
    PrintIndent(indent, "IsExpr : ", "{");
    PrintBasic(indent + ONE_INDENT, isExpr);
    if (isExpr.leftExpr) {
        PrintNode(isExpr.leftExpr.get(), indent + ONE_INDENT, "leftExpr");
    }
    if (isExpr.isType) {
        PrintNode(isExpr.isType.get(), indent + ONE_INDENT, "isType");
    }
    PrintIndent(indent, "}");
}

void PrintAsExpr(unsigned indent, const AsExpr& asExpr)
{
    PrintIndent(indent, "AsExpr: {");
    PrintBasic(indent + ONE_INDENT, asExpr);
    if (asExpr.leftExpr) {
        PrintNode(asExpr.leftExpr.get(), indent + ONE_INDENT, "leftExpr");
    }
    if (asExpr.asType) {
        PrintNode(asExpr.asType.get(), indent + ONE_INDENT, "asType");
    }
    PrintIndent(indent, "}");
}

void PrintTokenPart(unsigned indent, const AST::TokenPart& tokenPart)
{
    if (tokenPart.tokens.empty()) {
        return;
    }
    PrintIndentTokens(indent + ONE_INDENT, tokenPart.tokens);
}

void PrintQuoteExpr(unsigned indent, const AST::QuoteExpr& qe)
{
    PrintIndent(indent, "QuoteExpr {");
    for (auto& expr : qe.exprs) {
        if (!expr) {
            continue;
        }
        if (expr->astKind == ASTKind::TOKEN_PART) {
            PrintIndent(indent + ONE_INDENT, "TokenPart", "(");
        } else {
            PrintIndent(indent + ONE_INDENT, "DollarExpr", "(");
        }
        PrintNode(expr.get(), indent + TWO_INDENT);
        PrintIndent(indent + ONE_INDENT, ")");
    }
    PrintIndent(indent, "}");
}

void PrintMacroExpandExpr(unsigned indent, const AST::MacroExpandExpr& expr)
{
    PrintIndent(indent, "MacroExpand:", expr.invocation.fullName, "{");
    PrintBasic(indent + ONE_INDENT, expr);
    if (!expr.identifier.Val().empty()) {
        PrintIndent(indent + ONE_INDENT, "identifier:", expr.identifier.Val());
    }
    PrintAnnotationsVec(indent + ONE_INDENT, expr.annotations);
    PrintModifierSet(indent, expr.modifiers);
    PrintMacroInvocation(indent, expr.invocation);
    PrintIndent(indent, "}");
}

void PrintPrimitiveTypeExpr(unsigned indent, const PrimitiveTypeExpr& expr)
{
    PrintIndent(indent, "PrimitiveTypeExpr {");
    PrintBasic(indent + ONE_INDENT, expr);
    PrintIndent(indent + ONE_INDENT, "typeKind:", static_cast<int>(expr.typeKind));
    PrintIndent(indent, "}");
}

void PrintInvalidExpr(unsigned indent, const InvalidExpr& expr)
{
    PrintIndent(indent, "InvalidExpr {");
    PrintBasic(indent + ONE_INDENT, expr);
    if (!expr.value.empty()) {
        PrintIndent(indent + ONE_INDENT, "value:", "\"" + expr.value + "\"");
    }
    PrintIndent(indent, "}");
}

void PrintRefType(unsigned indent, const RefType& refType)
{
    PrintIndent(indent, "RefType:", refType.ref.identifier.Val(), "{");
    PrintBasic(indent + ONE_INDENT, refType);
    if (!refType.typeArguments.empty()) {
        PrintIndent(indent + ONE_INDENT, "typeArguments [");
        for (auto& it : refType.typeArguments) {
            PrintNode(it.get(), indent + TWO_INDENT);
        }
        PrintIndent(indent + ONE_INDENT, "]");
    }
    if (refType.ref.target) {
        PrintTarget(indent + ONE_INDENT, *refType.ref.target);
    }
    if (!refType.ref.targets.empty()) {
        PrintIndent(indent + ONE_INDENT, "targets [");
        for (auto t : refType.ref.targets) {
            PrintTarget(indent + TWO_INDENT, *t);
        }
        PrintIndent(indent + ONE_INDENT, "]");
    }
    PrintIndent(indent, "}");
}

void PrintParenType(unsigned indent, const ParenType& type)
{
    PrintIndent(indent, "ParenType {");
    PrintBasic(indent + ONE_INDENT, type);
    PrintNode(type.type.get(), indent + ONE_INDENT);
    PrintIndent(indent, "}");
}

void PrintFuncType(unsigned indent, const FuncType& type)
{
    PrintIndent(indent, "FuncType {");
    PrintBasic(indent + ONE_INDENT, type);
    if (!type.paramTypes.empty()) {
        PrintIndent(indent + ONE_INDENT, "paramTypes [");
        for (auto& paramType : type.paramTypes) {
            PrintNode(paramType.get(), indent + TWO_INDENT);
        }
        PrintIndent(indent + ONE_INDENT, "]");
    }
    if (type.retType) {
        PrintNode(type.retType.get(), indent + ONE_INDENT, "retType");
    }
    if (type.isC) {
        PrintIndent(indent + ONE_INDENT, "IsCFuncType:", type.isC);
    }
    PrintIndent(indent, "}");
}

void PrintTupleType(unsigned indent, const TupleType& type)
{
    PrintIndent(indent, "TupleType {");
    PrintBasic(indent + ONE_INDENT, type);
    if (!type.fieldTypes.empty()) {
        PrintIndent(indent + ONE_INDENT, "fieldTypes [");
        for (auto& it : type.fieldTypes) {
            PrintNode(it.get(), indent + TWO_INDENT);
        }
        PrintIndent(indent + ONE_INDENT, "]");
    }
    PrintIndent(indent, "}");
}

void PrintThisType(unsigned indent, const ThisType& type)
{
    PrintIndent(indent, "ThisType {");
    PrintBasic(indent + ONE_INDENT, type);
    PrintIndent(indent, "}");
}

void PrintPrimitiveType(unsigned indent, const PrimitiveType& type)
{
    PrintIndent(indent, "PrimitiveType:", type.str, "{");
    PrintBasic(indent + ONE_INDENT, type);
    if (type.kind != TypeKind::TYPE_INVALID) {
        PrintIndent(indent + ONE_INDENT, "kind:", static_cast<int>(type.kind));
    }
    PrintIndent(indent, "}");
}

void PrintOptionType(unsigned indent, const OptionType& type)
{
    PrintIndent(indent, "OptionType {");
    PrintBasic(indent + ONE_INDENT, type);
    if (type.componentType) {
        PrintIndent(indent + ONE_INDENT, "BaseType:");
        PrintNode(type.componentType.get(), indent + TWO_INDENT);
    }
    if (type.questNum > 0) {
        PrintIndent(indent + ONE_INDENT, "QuestNums:", type.questNum);
    }
    if (type.desugarType) {
        PrintNode(type.desugarType.get(), indent + ONE_INDENT, "desugarType");
    }
    PrintIndent(indent, "}");
}

void PrintVArrayType(unsigned indent, const VArrayType& type)
{
    PrintIndent(indent, "VArrayType {");
    PrintBasic(indent + ONE_INDENT, type);
    if (type.typeArgument) {
        PrintIndent(indent + ONE_INDENT, "typeArgument:");
        PrintNode(type.typeArgument.get(), indent + TWO_INDENT);
    }
    if (type.constantType) {
        PrintNode(type.constantType.get(), indent + ONE_INDENT, "constantType");
    }
    PrintIndent(indent, "}");
}

void PrintQualifiedType(unsigned indent, const QualifiedType& type)
{
    PrintIndent(indent, "QualifiedType {");
    PrintBasic(indent + ONE_INDENT, type);
    PrintNode(type.baseType.get(), indent + ONE_INDENT, "baseType");
    PrintIndent(indent + ONE_INDENT, "field:", type.field.Val());
    if (!type.typeArguments.empty()) {
        PrintIndent(indent + ONE_INDENT, "typeArguments [");
        for (auto& it : type.typeArguments) {
            PrintNode(it.get(), indent + TWO_INDENT);
        }
        PrintIndent(indent + ONE_INDENT, "]");
    }
    if (type.target) {
        PrintTarget(indent + ONE_INDENT, *type.target);
    }
    PrintIndent(indent, "}");
}

void PrintTypeNode(unsigned indent, const Type& type)
{
    PrintIndent(indent, "Type {");
    PrintBasic(indent + ONE_INDENT, type);
    if (!type.typeParameterName.empty()) {
        PrintIndent(indent + ONE_INDENT, "typeParameterName:", type.typeParameterName);
        PrintIndent(indent + ONE_INDENT, "typeParameterNameIsRawId:", type.typeParameterNameIsRawId);
    }
    PrintIndent(indent, "}");
}

void PrintConstantType(unsigned indent, const ConstantType& type)
{
    PrintIndent(indent, "ConstantType {");
    PrintBasic(indent + ONE_INDENT, type);
    if (!type.dollarPos.IsZero()) {
        PrintIndent(indent + ONE_INDENT, "dollarPos:", type.dollarPos.ToString());
    }
    PrintNode(type.constantExpr.get(), indent + ONE_INDENT, "constantExpr");
    PrintIndent(indent, "}");
}

void PrintGenericConstraint(unsigned indent, const GenericConstraint& generic)
{
    PrintIndent(indent, "GenericConstraint {");
    PrintNode(generic.type.get(), indent + ONE_INDENT);
    PrintIndent(indent + ONE_INDENT, "upperBounds {");
    for (auto& upperBound : generic.upperBounds) {
        PrintNode(upperBound.get(), indent + TWO_INDENT);
    }
    PrintIndent(indent + ONE_INDENT, "}");
    PrintIndent(indent, "}");
}

void PrintPatternNode(unsigned indent, const Pattern& pattern)
{
    PrintIndent(indent, "Pattern {");
    PrintBasic(indent + ONE_INDENT, pattern);
    if (pattern.ctxExpr) {
        PrintNode(pattern.ctxExpr, indent + ONE_INDENT, "ctxExpr");
    }
    PrintIndent(indent, "}");
}

void PrintInvalidPatternNode(unsigned indent, const InvalidPattern& pattern)
{
    PrintIndent(indent, "InvalidPattern {");
    PrintBasic(indent + ONE_INDENT, pattern);
    if (pattern.ctxExpr) {
        PrintNode(pattern.ctxExpr, indent + ONE_INDENT, "ctxExpr");
    }
    PrintIndent(indent, "}");
}

void PrintConstPattern(unsigned indent, const ConstPattern& constPattern)
{
    PrintIndent(indent, "ConstPattern {");
    PrintNode(constPattern.literal.get(), indent + ONE_INDENT);
    if (constPattern.operatorCallExpr) {
        PrintNode(constPattern.operatorCallExpr.get(), indent + ONE_INDENT, "operatorCallExpr");
    }
    PrintIndent(indent, "}");
}

void PrintTuplePattern(unsigned indent, const TuplePattern& tuplePattern)
{
    PrintIndent(indent, "TuplePattern {");
    if (!tuplePattern.patterns.empty()) {
        PrintIndent(indent + ONE_INDENT, "patterns [");
        for (auto& pattern : tuplePattern.patterns) {
            PrintNode(pattern.get(), indent + ONE_INDENT);
        }
        PrintIndent(indent + ONE_INDENT, "]");
    }
    PrintIndent(indent, "}");
}

void PrintBasicpePattern(unsigned indent, const TypePattern& typePattern)
{
    PrintIndent(indent, "TypePattern {");
    PrintBasic(indent + ONE_INDENT, typePattern);
    PrintNode(typePattern.pattern.get(), indent + ONE_INDENT, "pattern");
    PrintNode(typePattern.type.get(), indent + ONE_INDENT, "type");
    if (typePattern.desugarExpr) {
        PrintNode(typePattern.desugarExpr.get(), indent + ONE_INDENT, "desugarExpr");
    }
    if (typePattern.desugarVarPattern.get()) {
        PrintNode(typePattern.desugarVarPattern.get(), indent + ONE_INDENT, "// desugarVarPattern");
    }
    if (!typePattern.needRuntimeTypeCheck) {
        PrintIndent(indent + ONE_INDENT, "needRuntimeTypeCheck:", typePattern.needRuntimeTypeCheck);
    }
    if (!typePattern.matchBeforeRuntime) {
        PrintIndent(indent + ONE_INDENT, "matchBeforeRuntime:", typePattern.matchBeforeRuntime);
    }
    PrintIndent(indent, "}");
}

void PrintVarPattern(unsigned indent, const VarPattern& varPattern)
{
    if (!varPattern.varDecl) {
        PrintIndent(indent, "VarPattern: <null> {");
        PrintBasic(indent + ONE_INDENT, varPattern);
        PrintIndent(indent, "}");
        return;
    }
    PrintIndent(indent, "VarPattern:", varPattern.varDecl->identifier.Val(), "{");
    PrintBasic(indent + ONE_INDENT, varPattern);
    PrintNode(varPattern.varDecl, indent + ONE_INDENT, "varDecl");
    if (varPattern.desugarExpr) {
        PrintNode(varPattern.desugarExpr.get(), indent + ONE_INDENT, "desugarExpr");
    }
    PrintIndent(indent, "}");
}

void PrintEnumPattern(unsigned indent, const EnumPattern& enumPattern)
{
    PrintIndent(indent, "EnumPattern {");
    PrintNode(enumPattern.constructor.get(), indent + ONE_INDENT);
    for (auto& pattern : enumPattern.patterns) {
        PrintNode(pattern.get(), indent + ONE_INDENT);
    }
    PrintIndent(indent, "}");
}

void PrintExceptTypePattern(unsigned indent, const ExceptTypePattern& exceptTypePattern)
{
    PrintIndent(indent, "ExceptTypePattern {");
    PrintNode(exceptTypePattern.pattern.get(), indent + ONE_INDENT);
    for (auto& type : exceptTypePattern.types) {
        PrintNode(type.get(), indent + ONE_INDENT);
    }
    PrintIndent(indent, "}");
}

void PrintVarOrEnumPattern(unsigned indent, const VarOrEnumPattern& ve)
{
    PrintIndent(indent, "VarOrEnumPattern {");
    PrintIndent(indent + ONE_INDENT, ve.identifier);
    if (ve.pattern) {
        PrintIndent(indent + ONE_INDENT, "Subpatterns {");
        PrintNode(ve.pattern, indent + TWO_INDENT);
        PrintIndent(indent + ONE_INDENT, "}");
    }
    PrintIndent(indent, "}");
}

void PrintCommandTypePattern(unsigned indent, const CommandTypePattern& commandTypePattern)
{
    PrintNode(commandTypePattern.pattern.get(), indent + ONE_INDENT);
    for (auto& type : commandTypePattern.types) {
        PrintNode(type.get(), indent + ONE_INDENT);
    }
}

void PrintFeaturesDirective(unsigned indent, const FeaturesDirective& featuresDirective)
{
    PrintIndent(indent, "FeaturesDirective:", "features", "{");
    if (!featuresDirective.content.empty()) {
        PrintIndent(indent + ONE_INDENT, "ids: ", "[");
        std::stringstream ss;
        for (size_t i = 0; i < featuresDirective.content.size(); i++) {
            ss << featuresDirective.content[i].ToString();
            if (i < featuresDirective.content.size() - 1) { ss << ", "; }
        }
        PrintIndent(indent + TWO_INDENT, ss.str());
        PrintIndent(indent + ONE_INDENT, "]");
    }
    if (!featuresDirective.begin.IsZero() || !featuresDirective.end.IsZero()) {
        PrintIndent(indent + ONE_INDENT,
            "position:",
            featuresDirective.begin.ToString(),
            featuresDirective.end.ToString());
    }
    PrintIndent(indent, "}");
}

void PrintFeatureId(unsigned indent, const FeatureId& featureId)
{
    PrintIndent(indent, "FeatureId:", featureId.ToString(), "{");
    if (!featureId.identifiers.empty()) {
        PrintIndent(indent + ONE_INDENT, "identifiers [");
        for (auto& ident : featureId.identifiers) {
            PrintIndent(indent + TWO_INDENT, ident.Val());
        }
        PrintIndent(indent + ONE_INDENT, "]");
    }
    PrintIndent(indent, "}");
}

void PrintPackageSpec(unsigned indent, const PackageSpec& package)
{
    PrintIndent(indent, "PackageSpec:", package.packageName.Val(), "{");
    if (!package.GetPackageName().empty()) {
        PrintIndent(indent + ONE_INDENT, package.GetPackageName());
    }
    if (package.hasDoubleColon) {
        PrintIndent(indent + ONE_INDENT, "hasDoubleColon:", package.hasDoubleColon);
    }
    if (package.hasMacro) {
        PrintIndent(indent + ONE_INDENT, "hasMacro:", package.hasMacro);
    }
    if (package.hasPlatform) {
        PrintIndent(indent + ONE_INDENT, "hasPlatform:", package.hasPlatform);
    }
    if (package.hasCommon) {
        PrintIndent(indent + ONE_INDENT, "hasCommon:", package.hasCommon);
    }
    if (!package.prefixPaths.empty()) {
        auto prefixPath = JoinStrings(package.prefixPaths, ".");
        PrintIndent(indent + ONE_INDENT, "prefixPath:", std::move(prefixPath));
    }
    if (package.modifier) {
        PrintIndent(indent + ONE_INDENT, "modifier:", TOKENS[static_cast<int>(package.modifier->modifier)]);
    }
    PrintIndent(indent, "}");
}


const char* ImportKindName(ImportKind kind)
{
    switch (kind) {
        case ImportKind::IMPORT_SINGLE:
            return "single";
        case ImportKind::IMPORT_ALIAS:
            return "alias";
        case ImportKind::IMPORT_ALL:
            return "all";
        case ImportKind::IMPORT_MULTI:
            return "multi";
        default:
            return "unknown";
    }
}

void PrintImportContent(unsigned indent, const ImportContent& import)
{
    PrintIndent(indent, "ImportContent:", import.ToString(), "{");
    PrintIndent(indent + ONE_INDENT, "kind:", ImportKindName(import.kind));
    if (import.hasDoubleColon) {
        PrintIndent(indent + ONE_INDENT, "hasDoubleColon:", import.hasDoubleColon);
    }
    auto prefixPath = import.GetPrefixPath();
    if (!prefixPath.empty()) {
        PrintIndent(indent + ONE_INDENT, "prefixPath:", prefixPath);
    }
    if (!import.identifier.Val().empty()) {
        PrintIndent(indent + ONE_INDENT, "identifier:", import.identifier.Val());
    }
    if (import.kind == ImportKind::IMPORT_ALIAS) {
        PrintIndent(indent + ONE_INDENT, "alias:", import.aliasName.Val());
    }
    if (import.kind == ImportKind::IMPORT_MULTI) {
        PrintIndent(indent + ONE_INDENT, "items [");
        for (const auto& item : import.items) {
            PrintImportContent(indent + TWO_INDENT, item);
        }
        PrintIndent(indent + ONE_INDENT, "]");
    }
    if (import.isDecl) {
        PrintIndent(indent + ONE_INDENT, "isDecl:", import.isDecl);
    }
    PrintIndent(indent, "}");
}
void PrintImportSpec(unsigned indent, const ImportSpec& import)
{
    if (import.content.kind != ImportKind::IMPORT_MULTI) {
        PrintIndent(indent, "ImportSpec:", import.content.identifier.Val(), "{");
        PrintAnnotationsVec(indent + ONE_INDENT, import.annotations);
        if (import.content.kind == ImportKind::IMPORT_ALIAS) {
            PrintIndent(indent + ONE_INDENT, "aliasName:", import.content.aliasName.Val());
        }
        if (!import.content.prefixPaths.empty()) {
            PrintIndent(indent + ONE_INDENT, "prefixPaths:", import.content.GetPrefixPath());
        }
        if (import.modifier) {
            PrintIndent(indent + ONE_INDENT, "modifier:", TOKENS[static_cast<int>(import.modifier->modifier)]);
        }
        if (import.content.isDecl) {
            PrintIndent(indent + ONE_INDENT, "isDecl:", import.content.isDecl);
        }
        PrintIndent(indent, "}");
    } else {
        std::string commonPrefix = import.content.GetPrefixPath();
        for (const auto& item : import.content.items) {
            PrintIndent(indent, "ImportSpec:", item.identifier.Val(), "{");
            PrintAnnotationsVec(indent + ONE_INDENT, import.annotations);
            if (item.kind == ImportKind::IMPORT_ALIAS) {
                PrintIndent(indent + ONE_INDENT, "aliasName:", item.aliasName.Val());
            }
            auto itemPrefix = item.GetPrefixPath();
            if (!itemPrefix.empty()) {
                PrintIndent(indent + ONE_INDENT, "prefixPaths:", itemPrefix);
            }
            if (import.modifier) {
                PrintIndent(indent + ONE_INDENT, "modifier:", TOKENS[static_cast<int>(import.modifier->modifier)]);
            }
            if (item.isDecl) {
                PrintIndent(indent + ONE_INDENT, "isDecl:", item.isDecl);
            }
            PrintIndent(indent, "}");
        }
    }
}
} // namespace
#endif

void PrintNode([[maybe_unused]] Ptr<const Node> node, [[maybe_unused]] unsigned indent,
    [[maybe_unused]] const std::string addition)
{
#if !defined(NDEBUG) || defined(CMAKE_ENABLE_ASSERT) // Enable printNode for debug and CI version.
    if (!node) {
        return;
    }
    if (!addition.empty()) {
        PrintIndent(indent, "//", addition);
    }
    if (node->IsExpr()) {
        auto expr = RawStaticCast<const Expr*>(node);
        if (expr->desugarExpr) {
            return PrintNode(expr->desugarExpr.get(), indent, addition);
        }
    }
    match (*node)([&indent](const Package& package) { PrintPackage(indent, package); },
        [&indent](const File& file) { PrintFile(indent, file); },
        [&indent](const Annotation& anno) { PrintAnnotation(indent, anno); },
        [&indent](const Modifier& modifier) { PrintModifierNode(indent, modifier); },
        // ----------- Decls --------------------
        [&indent](const GenericParamDecl& gpd) { PrintGenericParamDecl(indent, gpd); },
        [&indent](const FuncParam& param) { PrintFuncParam(indent, param); },
        [&indent](const MacroExpandParam& macroExpand) { PrintMacroExpandParam(indent, macroExpand); },
        [&indent](const FuncParamList& paramList) { PrintFuncParamList(indent, paramList); },
        [&indent](const MainDecl& mainDecl) { PrintMainDecl(indent, mainDecl); },
        [&indent](const FuncDecl& funcDecl) { PrintFuncDecl(indent, funcDecl); },
        [&indent](const MacroDecl& macroDecl) { PrintMacroDecl(indent, macroDecl); },
        [&indent](const FuncBody& body) { PrintFuncBody(indent, body); },
        [&indent](const PropDecl& propDecl) { PrintPropDecl(indent, propDecl); },
        [&indent](const MacroExpandDecl& macroExpand) { PrintMacroExpandDecl(indent, macroExpand); },
        [&indent](const VarWithPatternDecl& vwpd) { PrintVarWithPatternDecl(indent, vwpd); },
        [&indent](const VarDecl& varDecl) { PrintVarDecl(indent, varDecl); },
        [&indent](const TypeAliasDecl& alias) { PrintBasicpeAliasDecl(indent, alias); },
        [&indent](const ClassDecl& classDecl) { PrintClassDecl(indent, classDecl); },
        [&indent](const InterfaceDecl& interfaceDecl) { PrintInterfaceDecl(indent, interfaceDecl); },
        [&indent](const EnumDecl& enumDecl) { PrintEnumDecl(indent, enumDecl); },
        [&indent](const StructDecl& decl) { PrintStructDecl(indent, decl); },
        [&indent](const ExtendDecl& ed) { PrintExtendDecl(indent, ed); },
        [&indent](const PrimaryCtorDecl& pc) { PrintPrimaryCtorDecl(indent, pc); },
        [&indent](const BuiltInDecl& decl) { PrintBuiltInDecl(indent, decl); },
        [&indent](const PackageDecl& decl) { PrintPackageDecl(indent, decl); },
        [&indent](const InvalidDecl& decl) { PrintInvalidDecl(indent, decl); },
        [&indent](const ClassLikeDecl& decl) { PrintClassLikeDecl(indent, decl); },
        [&indent](const Decl& decl) { PrintDeclNode(indent, decl); },
        // -----------Exprs----------------------
        [&indent](const IfExpr& expr) { PrintIfExpr(indent, expr); },
        [&indent](const MatchExpr& expr) { PrintMatchExpr(indent, expr); },
        [&indent](const MatchCase& matchCase) { PrintMatchCase(indent, matchCase); },
        [&indent](const MatchCaseOther& matchCase) { PrintMatchCaseOther(indent, matchCase); },
        [&indent](const TryExpr& expr) { PrintTryExpr(indent, expr); },
        [&indent](const ThrowExpr& expr) { PrintThrowExpr(indent, expr); },
        [&indent](const PerformExpr& expr) { PrintPerformExpr(indent, expr); },
        [&indent](const ResumeExpr& expr) { PrintResumeExpr(indent, expr); },
        [&indent](const ReturnExpr& expr) { PrintReturnExpr(indent, expr); },
        [&indent](const JumpExpr& expr) { PrintJumpExpr(indent, expr); },
        [&indent](const ForInExpr& expr) { PrintForInExpr(indent, expr); },
        [&indent](const WhileExpr& expr) { PrintWhileExpr(indent, expr); },
        [&indent](const DoWhileExpr& expr) { PrintDoWhileExpr(indent, expr); },
        [&indent](const AssignExpr& expr) { PrintAssignExpr(indent, expr); },
        [&indent](const IncOrDecExpr& expr) { PrintIncOrDecExpr(indent, expr); },
        [&indent](const UnaryExpr& unaryExpr) { PrintUnaryExpr(indent, unaryExpr); },
        [&indent](const BinaryExpr& binaryExpr) { PrintBinaryExpr(indent, binaryExpr); },
        [&indent](const RangeExpr& rangeExpr) { PrintRangeExpr(indent, rangeExpr); },
        [&indent](const SubscriptExpr& expr) { PrintSubscriptExpr(indent, expr); },
        [&indent](const MemberAccess& expr) { PrintMemberAccess(indent, expr); },
        [&indent](const FuncArg& expr) { PrintFuncArg(indent, expr); },
        [&indent](const CallExpr& expr) { PrintCallExpr(indent, expr); },
        [&indent](const ParenExpr& parenExpr) { PrintParenExpr(indent, parenExpr); },
        [&indent](const LambdaExpr& expr) { PrintLambdaExpr(indent, expr); },
        [&indent](const TrailingClosureExpr& expr) { PrintTrailingClosureExpr(indent, expr); },
        [&indent](const LitConstExpr& expr) { PrintLitConstExpr(indent, expr); },
        [&indent](const InterpolationExpr& expr) { PrintInterpolationExpr(indent, expr); },
        [&indent](const StrInterpolationExpr& expr) { PrintStrInterpolationExpr(indent, expr); },
        [&indent](const ArrayLit& expr) { PrintArrayLit(indent, expr); },
        [&indent](const ArrayExpr& expr) { PrintArrayExpr(indent, expr); },
        [&indent](const PointerExpr& expr) { PrintPointerExpr(indent, expr); },
        [&indent](const TupleLit& expr) { PrintTupleLit(indent, expr); },
        [&indent](const TypeConvExpr& expr) { PrintBasicpeConvExpr(indent, expr); },
        [&indent](const RefExpr& refExpr) { PrintRefExpr(indent, refExpr); },
        [&indent](const OptionalExpr& expr) { PrintOptionalExpr(indent, expr); },
        [&indent](const OptionalChainExpr& expr) { PrintOptionalChainExpr(indent, expr); },
        [&indent](const LetPatternDestructor& expr) { PrintLetPatternDestructor(indent, expr); },
        [&indent](const PrimitiveTypeExpr& pte) { PrintPrimitiveTypeExpr(indent, pte); },
        [&indent](const SpawnExpr& expr) { PrintSpawnExpr(indent, expr); },
        [&indent](const SynchronizedExpr& expr) { PrintSynchronizedExpr(indent, expr); },
        [&indent](const InvalidExpr& expr) { PrintInvalidExpr(indent, expr); },
        [&indent](const Block& block) { PrintBlock(indent, block); },
        [&indent](const InterfaceBody& body) { PrintInterfaceBody(indent, body); },
        [&indent](const ClassBody& body) { PrintClassBody(indent, body); },
        [&indent](const StructBody& body) { PrintStructBody(indent, body); },
        [&indent](const DummyBody& body) { PrintDummyBody(indent, body); },
        [&indent](const IsExpr& isExpr) { PrintIsExpr(indent, isExpr); },
        [&indent](const AsExpr& asExpr) { PrintAsExpr(indent, asExpr); },
        [&indent](const TokenPart& tokenPart) { PrintTokenPart(indent, tokenPart); },
        [&indent](const QuoteExpr& quoteExpr) { PrintQuoteExpr(indent, quoteExpr); },
        [&indent](const MacroExpandExpr& macroExpr) { PrintMacroExpandExpr(indent, macroExpr); },
        [&indent](const WildcardExpr& /* WildcardExpr */) { PrintIndent(indent, "WildcardExpr:", "_"); },
        [&indent](const IfAvailableExpr& expr) { PrintIfAvailableExpr(indent, expr); },
        [&indent](const Expr& expr) { PrintExprNode(indent, expr); },
        // ----------- Type --------------------
        [&indent](const RefType& refType) { PrintRefType(indent, refType); },
        [&indent](const ParenType& type) { PrintParenType(indent, type); },
        [&indent](const FuncType& type) { PrintFuncType(indent, type); },
        [&indent](const TupleType& type) { PrintTupleType(indent, type); },
        [&indent](const ThisType& type) { PrintThisType(indent, type); },
        [&indent](const PrimitiveType& type) { PrintPrimitiveType(indent, type); },
        [&indent](const OptionType& type) { PrintOptionType(indent, type); },
        [&indent](const VArrayType& type) { PrintVArrayType(indent, type); },
        [&indent](const ConstantType& type) { PrintConstantType(indent, type); },
        [&indent](const QualifiedType& type) { PrintQualifiedType(indent, type); },
        [&indent](const GenericConstraint& generic) { PrintGenericConstraint(indent, generic); },
        [&indent](const Generic& generic) { PrintGenericNode(indent, generic); },
        [&indent](const InvalidType&) { PrintIndent(indent, "InvalidType: Need to be fixed!"); },
        [&indent](const Type& type) { PrintTypeNode(indent, type); },
        // -----------pattern----------------------
        [&indent](const ConstPattern& constPattern) { PrintConstPattern(indent, constPattern); },
        [&indent](const WildcardPattern& /* WildcardPattern */) { PrintIndent(indent, "WildcardPattern:", "_"); },
        [&indent](const VarPattern& varPattern) { PrintVarPattern(indent, varPattern); },
        [&indent](const TuplePattern& tuplePattern) { PrintTuplePattern(indent, tuplePattern); },
        [&indent](const TypePattern& typePattern) { PrintBasicpePattern(indent, typePattern); },
        [&indent](const EnumPattern& enumPattern) { PrintEnumPattern(indent, enumPattern); },
        [&indent](const ExceptTypePattern& exceptTypePattern) { PrintExceptTypePattern(indent, exceptTypePattern); },
        [&indent](const CommandTypePattern& cmdTypePattern) { PrintCommandTypePattern(indent, cmdTypePattern); },
        [indent](const VarOrEnumPattern& ve) { PrintVarOrEnumPattern(indent, ve); },
        [&indent](const InvalidPattern& pattern) { PrintInvalidPatternNode(indent, pattern); },
        [&indent](const Pattern& pattern) { PrintPatternNode(indent, pattern); },
        // ----------- package----------------------
        [&indent](const FeatureId& featureId) { PrintFeatureId(indent, featureId); },
        [&indent](const FeaturesDirective& feature) { PrintFeaturesDirective(indent, feature); },
        [&indent](const PackageSpec& package) { PrintPackageSpec(indent, package); },
        [&indent](const ImportSpec& import) {
            if (import.IsImportMulti()) {
                return;
            }
            PrintImportSpec(indent, import);
        },
        [&indent](const ImportContent& importContent) { PrintImportContent(indent, importContent); },
        // -----------no match----------------------
        [&indent, &node]() {
            PrintIndent(indent, "UnknowNode:", ASTKIND_TO_STR.at(node->astKind), "{");
            PrintBasic(indent + ONE_INDENT, *node);
            PrintIndent(indent, "}");
        });
#endif
}

void PrintNode(Ptr<const AST::Node> node)
{
    PrintNode(node, 0, "");
}
} // namespace Cangjie
