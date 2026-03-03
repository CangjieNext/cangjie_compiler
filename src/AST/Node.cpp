// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements the ToString method for nodes.
 */

#include "cangjie/AST/Node.h"
#include "cangjie/AST/NodeX.h"

#include <algorithm>
#include <iomanip>
#include <ios>
#include <iterator>
#include <memory>
#include <numeric>
#include <queue>
#include <sstream>
#include <string_view>
#include <vector>

#include "cangjie/AST/Match.h"
#include "cangjie/AST/RecoverDesugar.h"
#include "cangjie/AST/Symbol.h"
#include "cangjie/AST/Utils.h"
#include "cangjie/AST/Walker.h"
#include "cangjie/Basic/Position.h"
#include "cangjie/Utils/CheckUtils.h"
#include "cangjie/Utils/StdUtils.h"

namespace Cangjie {
using namespace AST;

namespace {
const std::string EMPTY_PACKAGE_NAME = "";

struct Span {
    std::string str;
    int numNewLine{0};
    int width{0};
};

Span NextSpan(const std::string& str, const Position& begin, const Position& end, int offset = 0)
{
    Span span;
    span.str = str;
    span.numNewLine = end.line - begin.line;
    span.width = (span.numNewLine > 0 ? end.column - 1 : end.column - begin.column) + offset;
    return span;
}

std::ostream& operator<<(std::ostream& out, Span const& span)
{
    int numNewLine = span.numNewLine;
    while (numNewLine > 0) {
        out << '\n';
        numNewLine--;
    }
    out << std::right << std::setw(span.width) << span.str;
    return out;
}

std::string GetIdentifierOrAnonymous(const SrcIdentifier& ident)
{
    auto text = ident.GetRawText();
    if (text.empty()) {
        return "<anonymous>";
    }
    return text;
}

std::string GetIdentifierOrAnonymous(const Identifier& ident)
{
    auto text = ident.Val();
    if (text.empty()) {
        return "<anonymous>";
    }
    return text;
}

void AppendAnnotations(const std::vector<OwnedPtr<Annotation>>& annotations, std::stringstream& ss)
{
    for (const auto& annotation : annotations) {
        if (!annotation) {
            continue;
        }
        ss << annotation->ToString() << "\n";
    }
}

void AppendModifiers(const std::set<Modifier>& modifiers, std::stringstream& ss)
{
    bool hasModifier = false;
    for (const auto& modifier : modifiers) {
        auto text = modifier.ToString();
        if (text.empty()) {
            continue;
        }
        if (hasModifier) {
            ss << " ";
        }
        ss << text;
        hasModifier = true;
    }
    if (hasModifier) {
        ss << " ";
    }
}

void AppendTypeList(const std::vector<OwnedPtr<Type>>& types, std::stringstream& ss, std::string_view delimiter = ", ")
{
    bool first = true;
    for (const auto& ty : types) {
        if (!ty) {
            continue;
        }
        if (!first) {
            ss << delimiter;
        }
        ss << ty->ToString();
        first = false;
    }
}

void AppendDeclNames(const std::vector<OwnedPtr<Decl>>& decls, std::stringstream& ss)
{
    bool first = true;
    for (const auto& decl : decls) {
        if (!decl) {
            continue;
        }
        if (!first) {
            ss << ", ";
        }
        ss << GetIdentifierOrAnonymous(decl->identifier);
        first = false;
    }
}

void AppendBraceWrappedDecls(const std::vector<OwnedPtr<Decl>>& decls, std::stringstream& ss)
{
    if (decls.empty()) {
        ss << " {}";
        return;
    }
    ss << " { ";
    AppendDeclNames(decls, ss);
    ss << " }";
}

void AppendMacroInvocationNodes(const MacroInvocation& invocation, std::stringstream& ss)
{
    if (invocation.nodes.empty()) {
        ss << "()";
        return;
    }
    ss << "(";
    bool first = true;
    for (const auto& node : invocation.nodes) {
        if (!node) {
            continue;
        }
        if (!first) {
            ss << ", ";
        }
        ss << node->ToString();
        first = false;
    }
    ss << ")";
}

std::string_view BuiltInTypeName(BuiltInType type)
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
    }
    return "Unknown";
}

std::string_view TokenLiteral(TokenKind kind)
{
    if (kind == TokenKind::ILLEGAL) {
        return "";
    }
    return TOKENS[static_cast<size_t>(kind)];
}

std::string_view TypeKindName(TypeKind kind)
{
    switch (kind) {
#define TYPE_KIND(KIND, TYPE, NAME) \
    case TypeKind::KIND:             \
        return NAME;
#include "cangjie/AST/TypeKind.inc"
#undef TYPE_KIND
        case TypeKind::TYPE_ANY:
            return "Any";
        case TypeKind::TYPE_INTERSECTION:
            return "Intersection";
        case TypeKind::TYPE_UNION:
            return "Union";
        case TypeKind::TYPE_QUEST:
            return "Quest";
        case TypeKind::TYPE_INITIAL:
            return "Initial";
        default:
            return "Type";
    }
}

/**
 * @brief Get the end position of a token in *.macrocall file by the same token's position in curfile.
 * @param key: the Hash value of line and column about token's end position in curfile.
 * @param first: the Hash value of line and column about token's begin position in curfile.
 * @param second: the begin position of the same token in *.macrocall file.
 * @return the end position in *.macrocall file.
 */
Position GetMacroCallEndPos(bool isCurFile, uint32_t key, uint32_t first, const Position second)
{
    auto end = second;
    if (!isCurFile) {
        // key and first are only column.
        auto columnOffset = (key > first) ? (key - first) : 0;
        return end + columnOffset;
    }
    // key and first are the Hash value of line and column that created by Position.Hash32().
    auto keyPos = Position::RestorePosFromHash(key);
    auto firstPos = Position::RestorePosFromHash(first);
    auto lineOffset = (keyPos.first > firstPos.first) ? (keyPos.first - firstPos.first) : 0;
    if (lineOffset > 0) {
        end.line += lineOffset;
        end.column = keyPos.second;
        return end;
    }
    auto columnOffset = (key > first) ? (key - first) : 0;
    return end + columnOffset;
}

/**
 * @brief Get the end position of a token for LSP using new2originPosMap.
 * @param newPos: the Hash value of line and column about token's end position in curfile.
 * @param first: the Hash value of line and column about token's begin position in curfile.
 * @return the end position in *.macrocall file.
 */
Position GetMacroCallEndPosforLsp(uint32_t newPos, uint32_t first, const Position second)
{
    auto end = second;
    // newPos and first are the Hash value of line and column that created by Position.Hash32().
    auto keyPos = Position::RestorePosFromHash(newPos);
    auto firstPos = Position::RestorePosFromHash(first);
    auto lineOffset = (keyPos.first > firstPos.first) ? (keyPos.first - firstPos.first) : 0;
    if (lineOffset > 0) {
        end.line += lineOffset;
        end.column = keyPos.second;
        return end;
    }
    auto columnOffset = (newPos > first) ? (newPos - first) : 0;
    return end + columnOffset;
}

/**
 * @brief Get the sourcePos of macrocall by the curfile's pos. For cjc, we
 *        get sourcePos only in *.macrocall file .
 * @return the sourcePos in curfile if the source can be founded in curfile,
 *  sourcePos in macrocall file otherwise.
 */
Position GetMacroSourcePos(const MacroInvocation& invocation, const Position& pos, bool isLowerBound = false)
{
    auto key = invocation.isCurFile ? pos.Hash32() : static_cast<const uint32_t>(pos.column);
    if (isLowerBound) {
        // Get end position.
        if (invocation.isForLSP) { // For lsp.
            auto posIt = invocation.new2originPosMap.find(pos.Hash32());
            if (posIt != invocation.new2originPosMap.end()) {
                ++posIt;
            }
            if (posIt == invocation.new2originPosMap.end()) {
                posIt = invocation.new2originPosMap.lower_bound(pos.Hash32());
            }
            if (posIt == invocation.new2originPosMap.end()) {
                return pos;
            }
            auto sourcePos = GetMacroCallEndPosforLsp(pos.Hash32(), posIt->first, posIt->second);
            if (sourcePos.isCurFile) {
                return sourcePos;
            }
        }
        auto posIt = invocation.new2macroCallPosMap.lower_bound(key);
        if (posIt != invocation.new2macroCallPosMap.end()) {
            return GetMacroCallEndPos(invocation.isCurFile, key, posIt->first, posIt->second);
        }
        return pos;
    }
    // Get begin/identifier/field position.
    if (invocation.isForLSP) { // For lsp.
        if (invocation.new2originPosMap.find(pos.Hash32()) == invocation.new2originPosMap.end()) {
            return pos;
        }
        auto sourcePos = invocation.new2originPosMap.at(pos.Hash32());
        if (sourcePos.isCurFile) {
            return sourcePos;
        }
    }
    if (invocation.new2macroCallPosMap.find(key) != invocation.new2macroCallPosMap.end()) {
        return invocation.new2macroCallPosMap.at(key);
    }
    return pos;
}
} // namespace

std::string Node::ToString() const
{
    auto it = ASTKIND_TO_STR.find(astKind);
    if (it != ASTKIND_TO_STR.end()) {
        return it->second;
    }
    return "";
}

std::string Modifier::ToString() const
{
    if (isExplicit) {
        return TOKENS[static_cast<int>(modifier)];
    } else {
        return "";
    }
}

std::string VarDecl::ToString() const
{
    std::stringstream ss;
    Position curSpanBegin = begin;
    int i = 0;
    for (auto modifier : modifiers) {
        if (i > 0) {
            ss << NextSpan(modifier.ToString(), curSpanBegin, modifier.end);
        } else {
            ss << modifier.ToString();
        }
        curSpanBegin = modifier.end;
        i++;
    }
    // The length of "var" and "let" are both 3.
    ss << NextSpan(isVar ? "var" : "let", curSpanBegin, keywordPos, 3);
    curSpanBegin = keywordPos + Position{0, 0, 3};
    ss << NextSpan(identifier, curSpanBegin, identifier.Begin(), static_cast<int>(identifier.Length()));
    curSpanBegin = identifier.End();
    if (type) {
        ss << NextSpan(":", curSpanBegin, colonPos, 1);
        curSpanBegin = colonPos + Position{0, 0, 1};
        ss << NextSpan(type->ToString(), curSpanBegin, type->end);
        curSpanBegin = type->end;
    }
    if (initializer) {
        ss << NextSpan("=", curSpanBegin, assignPos, 1);
        curSpanBegin = assignPos + Position{0, 0, 1};
        ss << NextSpan(initializer->ToString(), curSpanBegin, initializer->end);
    }
    return ss.str();
}

std::string Annotation::ToString() const
{
    std::stringstream ss;
    ss << "@";
    if (isCompileTimeVisible) {
        ss << "!";
    }
    if (baseExpr) {
        ss << baseExpr->ToString();
    } else {
        ss << identifier.Val();
    }
    std::vector<std::string> elements;
    elements.reserve(attrs.size() + args.size() + (condExpr ? 1 : 0));
    for (const auto& attr : attrs) {
        elements.emplace_back(attr.Value());
    }
    if (condExpr) {
        elements.emplace_back(condExpr->ToString());
    }
    for (const auto& arg : args) {
        if (arg) {
            elements.emplace_back(arg->ToString());
        }
    }
    if (!elements.empty()) {
        ss << "[";
        for (size_t i = 0; i < elements.size(); ++i) {
            if (i > 0) {
                ss << ", ";
            }
            ss << elements[i];
        }
        ss << "]";
    }
    return ss.str();
}

std::string GenericParamDecl::ToString() const
{
    return identifier.GetRawText();
}

std::string GenericConstraint::ToString() const
{
    std::stringstream ss;
    if (type) {
        ss << type->ToString();
    }
    if (!upperBounds.empty()) {
        if (type) {
            ss << " <: ";
        }
        for (size_t i = 0; i < upperBounds.size(); ++i) {
            if (i > 0) {
                ss << " & ";
            }
            if (upperBounds[i]) {
                ss << upperBounds[i]->ToString();
            }
        }
    }
    return ss.str();
}

std::string Generic::ToString() const
{
    std::stringstream ss;
    if (!typeParameters.empty()) {
        ss << "<";
        for (size_t i = 0; i < typeParameters.size(); ++i) {
            if (i > 0) {
                ss << ", ";
            }
            if (typeParameters[i]) {
                ss << typeParameters[i]->ToString();
            }
        }
        ss << ">";
    }
    if (!genericConstraints.empty()) {
        ss << " where ";
        for (size_t i = 0; i < genericConstraints.size(); ++i) {
            if (i > 0) {
                ss << ", ";
            }
            if (genericConstraints[i]) {
                ss << genericConstraints[i]->ToString();
            }
        }
    }
    return ss.str();
}

std::string Decl::ToString() const
{
    std::stringstream ss;
    AppendAnnotations(annotations, ss);
    AppendModifiers(modifiers, ss);
    ss << GetIdentifierOrAnonymous(identifier);
    if (generic) {
        ss << generic->ToString();
    }
    return ss.str();
}

std::string ClassLikeDecl::ToString() const
{
    return Decl::ToString();
}

std::string EnumPattern::GetIdentifier() const
{
    if (!constructor) {
        return "";
    }
    switch (constructor->astKind) {
        case ASTKind::REF_EXPR: {
            return static_cast<const RefExpr&>(*constructor).ref.identifier;
        }
        case ASTKind::MEMBER_ACCESS: {
            return static_cast<const MemberAccess&>(*constructor).field;
        }
        default: {
            CJC_ABORT();
            return "";
        }
    }
}

std::string Pattern::ToString() const
{
    return Node::ToString();
}

std::string InvalidPattern::ToString() const
{
    return "<invalid pattern>";
}

std::string ConstPattern::ToString() const
{
    return literal ? literal->ToString() : "";
}

std::string WildcardPattern::ToString() const
{
    return "_";
}

std::string VarPattern::ToString() const
{
    if (!varDecl) {
        return "";
    }
    return varDecl->identifier.GetRawText();
}

std::string TuplePattern::ToString() const
{
    std::stringstream ss;
    ss << "(";
    for (size_t i = 0; i < patterns.size(); ++i) {
        if (i > 0) {
            ss << ", ";
        }
        if (patterns[i]) {
            ss << patterns[i]->ToString();
        }
    }
    ss << ")";
    return ss.str();
}

std::string TypePattern::ToString() const
{
    std::stringstream ss;
    if (pattern) {
        ss << pattern->ToString();
    }
    if (type) {
        if (pattern) {
            ss << ": ";
        }
        ss << type->ToString();
    }
    return ss.str();
}

std::string EnumPattern::ToString() const
{
    std::stringstream ss;
    if (constructor) {
        ss << constructor->ToString();
    }
    if (!patterns.empty()) {
        ss << "(";
        for (size_t i = 0; i < patterns.size(); ++i) {
            if (i > 0) {
                ss << ", ";
            }
            if (patterns[i]) {
                ss << patterns[i]->ToString();
            }
        }
        ss << ")";
    }
    return ss.str();
}

std::string VarOrEnumPattern::ToString() const
{
    if (pattern) {
        return pattern->ToString();
    }
    return identifier.GetRawText();
}

static std::string JoinTypes(const std::vector<OwnedPtr<Type>>& types)
{
    std::stringstream ss;
    for (size_t i = 0; i < types.size(); ++i) {
        if (i > 0) {
            ss << " | ";
        }
        if (types[i]) {
            ss << types[i]->ToString();
        }
    }
    return ss.str();
}

static std::string InlineBlock(const Block* block)
{
    if (!block) {
        return "";
    }
    if (block->body.size() == 1 && block->body[0]) {
        return block->body[0]->ToString();
    }
    return block->ToString();
}

std::string ExceptTypePattern::ToString() const
{
    std::stringstream ss;
    if (pattern) {
        ss << pattern->ToString();
    }
    if (!types.empty()) {
        if (pattern) {
            ss << ": ";
        }
        ss << JoinTypes(types);
    }
    return ss.str();
}

std::string CommandTypePattern::ToString() const
{
    std::stringstream ss;
    if (pattern) {
        ss << pattern->ToString();
    }
    if (!types.empty()) {
        if (pattern) {
            ss << ": ";
        }
        ss << JoinTypes(types);
    }
    return ss.str();
}

std::string CallExpr::ToString() const
{
    std::stringstream ss;
    if (!baseFunc) {
        return ss.str();
    }
    Position curSpanBegin = baseFunc->begin;
    ss << NextSpan(baseFunc->ToString(), curSpanBegin, baseFunc->end);
    curSpanBegin = baseFunc->end;
    ss << NextSpan("(", curSpanBegin, leftParenPos, 1);
    curSpanBegin = leftParenPos + Position{0, 0, 1};
    for (auto& arg : args) {
        if (!arg) {
            continue;
        }
        ss << NextSpan(arg->ToString(), curSpanBegin, arg->end);
        curSpanBegin = arg->end;
        if (arg->commaPos != INVALID_POSITION) {
            ss << NextSpan(",", curSpanBegin, arg->commaPos, 1);
            curSpanBegin = arg->commaPos + Position{0, 0, 1};
        }
    }
    ss << NextSpan(")", curSpanBegin, rightParenPos, 1);
    return ss.str();
}

void CallExpr::Clear() noexcept
{
    RecoverToCallExpr(*this);
    Expr::Clear();
    if (baseFunc) {
        baseFunc->Clear();
    }
    callKind = CallKind::CALL_INVALID;
    resolvedFunction = nullptr;
}

std::string FuncArg::ToString() const
{
    std::stringstream ss;
    Position curSpanBegin = begin;
    if (!name.Empty()) {
        curSpanBegin = name.Begin();
        ss << NextSpan(name, curSpanBegin, name.Begin(), static_cast<int>(name.Length()));
        curSpanBegin += Position{0, 0, static_cast<int>(name.Length())};
        ss << NextSpan(":", curSpanBegin, colonPos, 1);
        curSpanBegin += Position{0, 0, 1};
    } else if (expr) {
        curSpanBegin = expr->begin;
    }
    if (withInout) {
        ss << "inout ";
    }
    if (expr) {
        ss << NextSpan(expr->ToString(), curSpanBegin, expr->end);
    }
    return ss.str();
}

std::string FuncParam::ToString() const
{
    std::stringstream ss;
    if (hasLetOrVar) {
        ss << (isVar ? "var " : "let ");
    }
    ss << identifier.GetRawText();
    if (isNamedParam) {
        ss << "!";
    }
    if (type) {
        ss << ": " << type->ToString();
    }
    const Expr* defaultValue = nullptr;
    if (assignment) {
        defaultValue = assignment.get();
    } else if (initializer) {
        defaultValue = initializer.get();
    }
    if (defaultValue) {
        ss << " = " << defaultValue->ToString();
    }
    return ss.str();
}

std::string FuncParamList::ToString() const
{
    std::stringstream ss;
    ss << "(";
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) {
            ss << ", ";
        }
        if (params[i]) {
            ss << params[i]->ToString();
        }
    }
    ss << ")";
    return ss.str();
}

std::string FuncBody::ToString() const
{
    std::stringstream ss;
    if (generic) {
        ss << generic->ToString();
    }
    for (const auto& paramList : paramLists) {
        if (paramList) {
            ss << paramList->ToString();
        }
    }
    if (retType) {
        ss << ": " << retType->ToString();
    }
    if (doubleArrowPos != INVALID_POSITION) {
        ss << " => " << InlineBlock(body.get());
    } else if (body) {
        ss << " " << body->ToString();
    }
    return ss.str();
}

std::string FuncDecl::ToString() const
{
    std::stringstream ss;
    for (size_t i = 0; i < annotations.size(); ++i) {
        if (annotations[i]) {
            ss << annotations[i]->ToString();
            ss << "\n";
        }
    }
    bool needSpace = false;
    for (const auto& modifier : modifiers) {
        auto text = modifier.ToString();
        if (text.empty()) {
            continue;
        }
        if (needSpace) {
            ss << " ";
        }
        ss << text;
        needSpace = true;
    }
    if (needSpace) {
        ss << " ";
    }
    ss << "func " << identifier.GetRawText();
    if (funcBody) {
        ss << funcBody->ToString();
    }
    return ss.str();
}

std::string MacroDecl::ToString() const
{
    std::stringstream ss;
    AppendAnnotations(annotations, ss);
    AppendModifiers(modifiers, ss);
    ss << "macro " << GetIdentifierOrAnonymous(identifier);
    if (funcBody) {
        ss << funcBody->ToString();
    }
    return ss.str();
}

std::string MainDecl::ToString() const
{
    std::stringstream ss;
    AppendAnnotations(annotations, ss);
    AppendModifiers(modifiers, ss);
    ss << "main func " << GetIdentifierOrAnonymous(identifier);
    if (funcBody) {
        ss << funcBody->ToString();
    }
    return ss.str();
}

std::string PrimaryCtorDecl::ToString() const
{
    std::stringstream ss;
    AppendAnnotations(annotations, ss);
    AppendModifiers(modifiers, ss);
    ss << "init";
    if (!identifier.GetRawText().empty()) {
        ss << " " << identifier.GetRawText();
    }
    if (funcBody) {
        ss << funcBody->ToString();
    }
    return ss.str();
}

std::string TypeAliasDecl::ToString() const
{
    std::stringstream ss;
    AppendAnnotations(annotations, ss);
    AppendModifiers(modifiers, ss);
    ss << "typealias " << GetIdentifierOrAnonymous(identifier);
    if (generic) {
        ss << generic->ToString();
    }
    if (type) {
        ss << " = " << type->ToString();
    }
    return ss.str();
}

std::string BuiltInDecl::ToString() const
{
    std::stringstream ss;
    ss << "builtin " << GetIdentifierOrAnonymous(identifier) << " (" << BuiltInTypeName(type) << ")";
    return ss.str();
}

std::string VarWithPatternDecl::ToString() const
{
    std::stringstream ss;
    AppendAnnotations(annotations, ss);
    AppendModifiers(modifiers, ss);
    ss << (isVar ? "var " : "let ");
    if (irrefutablePattern) {
        ss << irrefutablePattern->ToString();
    } else {
        ss << GetIdentifierOrAnonymous(identifier);
    }
    if (type) {
        ss << ": " << type->ToString();
    }
    if (initializer) {
        ss << " = " << initializer->ToString();
    }
    return ss.str();
}

std::string PropDecl::ToString() const
{
    std::stringstream ss;
    AppendAnnotations(annotations, ss);
    AppendModifiers(modifiers, ss);
    ss << "prop " << GetIdentifierOrAnonymous(identifier);
    if (type) {
        ss << ": " << type->ToString();
    }
    if (!getters.empty() || !setters.empty()) {
        ss << " {";
        if (!getters.empty()) {
            ss << " get";
        }
        if (!setters.empty()) {
            if (!getters.empty()) {
                ss << ";";
            }
            ss << " set";
        }
        ss << " }";
    }
    return ss.str();
}

std::string ExtendDecl::ToString() const
{
    std::stringstream ss;
    AppendAnnotations(annotations, ss);
    AppendModifiers(modifiers, ss);
    ss << "extend ";
    if (extendedType) {
        ss << extendedType->ToString();
    } else {
        ss << GetIdentifierOrAnonymous(identifier);
    }
    if (generic) {
        ss << generic->ToString();
    }
    if (!inheritedTypes.empty()) {
        ss << " : ";
        AppendTypeList(inheritedTypes, ss);
    }
    AppendBraceWrappedDecls(members, ss);
    return ss.str();
}

std::string EnumDecl::ToString() const
{
    std::stringstream ss;
    AppendAnnotations(annotations, ss);
    AppendModifiers(modifiers, ss);
    ss << "enum " << GetIdentifierOrAnonymous(identifier);
    if (generic) {
        ss << generic->ToString();
    }
    ss << " {";
    if (!constructors.empty()) {
        ss << " constructors: ";
        AppendDeclNames(constructors, ss);
        if (!members.empty()) {
            ss << ";";
        }
    }
    if (!members.empty()) {
        ss << " members: ";
        AppendDeclNames(members, ss);
    }
    ss << " }";
    return ss.str();
}

std::string StructDecl::ToString() const
{
    std::stringstream ss;
    AppendAnnotations(annotations, ss);
    AppendModifiers(modifiers, ss);
    ss << "struct " << GetIdentifierOrAnonymous(identifier);
    if (generic) {
        ss << generic->ToString();
    }
    if (!inheritedTypes.empty()) {
        ss << " : ";
        AppendTypeList(inheritedTypes, ss);
    }
    if (body) {
        ss << body->ToString();
    } else {
        ss << " {}";
    }
    return ss.str();
}

std::string InterfaceDecl::ToString() const
{
    std::stringstream ss;
    AppendAnnotations(annotations, ss);
    AppendModifiers(modifiers, ss);
    ss << "interface " << GetIdentifierOrAnonymous(identifier);
    if (generic) {
        ss << generic->ToString();
    }
    if (!inheritedTypes.empty()) {
        ss << " : ";
        AppendTypeList(inheritedTypes, ss);
    }
    if (body) {
        ss << body->ToString();
    } else {
        ss << " {}";
    }
    return ss.str();
}

std::string ClassDecl::ToString() const
{
    std::stringstream ss;
    AppendAnnotations(annotations, ss);
    AppendModifiers(modifiers, ss);
    ss << "class " << GetIdentifierOrAnonymous(identifier);
    if (generic) {
        ss << generic->ToString();
    }
    if (!inheritedTypes.empty()) {
        ss << " : ";
        AppendTypeList(inheritedTypes, ss);
    }
    if (body) {
        ss << body->ToString();
    } else {
        ss << " {}";
    }
    return ss.str();
}

std::string MacroExpandExpr::ToString() const
{
    std::stringstream ss;
    AppendAnnotations(annotations, ss);
    AppendModifiers(modifiers, ss);
    auto name = invocation.fullName.empty() ? GetIdentifierOrAnonymous(identifier) : invocation.fullName;
    ss << "@" << name;
    AppendMacroInvocationNodes(invocation, ss);
    return ss.str();
}

std::string IfAvailableExpr::ToString() const
{
    std::stringstream ss;
    ss << "@IfAvailable(";
    bool needComma = false;
    if (arg) {
        ss << arg->ToString();
        needComma = true;
    }
    if (lambda1) {
        if (needComma) {
            ss << ", ";
        }
        ss << lambda1->ToString();
        needComma = true;
    }
    if (lambda2) {
        if (needComma) {
            ss << ", ";
        }
        ss << lambda2->ToString();
    }
    ss << ")";
    return ss.str();
}

std::string MacroExpandDecl::ToString() const
{
    std::stringstream ss;
    auto name = invocation.fullName.empty() ? GetIdentifierOrAnonymous(identifier) : invocation.fullName;
    ss << "@" << name;
    AppendMacroInvocationNodes(invocation, ss);
    return ss.str();
}

std::string MacroExpandParam::ToString() const
{
    std::stringstream ss;
    ss << FuncParam::ToString();
    if (!invocation.fullName.empty()) {
        ss << " @" << invocation.fullName;
    }
    return ss.str();
}

std::string InvalidDecl::ToString() const
{
    return "<invalid decl>";
}

std::string StructBody::ToString() const
{
    std::stringstream ss;
    AppendBraceWrappedDecls(decls, ss);
    return ss.str();
}

std::string InterfaceBody::ToString() const
{
    std::stringstream ss;
    AppendBraceWrappedDecls(decls, ss);
    return ss.str();
}

std::string ClassBody::ToString() const
{
    std::stringstream ss;
    AppendBraceWrappedDecls(decls, ss);
    return ss.str();
}

std::string DummyBody::ToString() const
{
    return "{}";
}

std::string ImportSpec::ToString() const
{
    std::stringstream ss;
    AppendAnnotations(annotations, ss);
    if (modifier) {
        ss << modifier->ToString() << ' ';
    }
    ss << "import " << content.ToString();
    return ss.str();
}

std::string PackageSpec::ToString() const
{
    std::stringstream ss;
    if (modifier) {
        ss << modifier->ToString() << ' ';
    }
    if (hasMacro) {
        ss << "macro ";
    }
    if (hasPlatform) {
        ss << "platform ";
    }
    if (hasCommon) {
        ss << "common ";
    }
    ss << "package " << GetPackageName();
    return ss.str();
}

std::string FeaturesDirective::ToString() const
{
    if (content.empty()) {
        return "features";
    }
    std::stringstream ss;
    ss << "features ";
    for (size_t i = 0; i < content.size(); ++i) {
        if (i > 0) {
            ss << ", ";
        }
        ss << content[i].ToString();
    }
    return ss.str();
}

std::string Package::ToString() const
{
    std::stringstream ss;
    ss << "package " << fullPackageName << " (files=" << files.size() << ", decls=" << genericInstantiatedDecls.size()
       << ")";
    if (HasFeature()) {
        ss << " features";
    }
    if (isMacroPackage) {
        ss << " macro";
    }
    return ss.str();
}

std::string PackageDecl::ToString() const
{
    std::stringstream ss;
    AppendAnnotations(annotations, ss);
    ss << "package " << (srcPackage ? srcPackage->fullPackageName : GetIdentifierOrAnonymous(identifier));
    return ss.str();
}

std::string File::ToString() const
{
    std::stringstream ss;
    ss << "file " << (!filePath.empty() ? filePath : fileName);
    if (package) {
        ss << " in " << package->GetPackageName();
    }
    ss << " (decls=" << decls.size() << ")";
    return ss.str();
}

std::string MemberAccess::ToString() const
{
    std::stringstream ss;
    if (!baseExpr) {
        return ss.str();
    }
    Position curSpanBegin = baseExpr->begin;
    ss << NextSpan(baseExpr->ToString(), curSpanBegin, baseExpr->end);
    curSpanBegin = baseExpr->end;
    ss << NextSpan(".", curSpanBegin, dotPos, 1);
    curSpanBegin = dotPos + Position{0, 0, 1};
    ss << NextSpan(field, curSpanBegin, field.Begin(), static_cast<int>(field.Length()));
    return ss.str();
}

std::string LitConstExpr::ToString() const
{
    if (kind == LitConstKind::STRING) {
        return "\"" + stringValue + "\"";
    } else {
        return stringValue;
    }
}

TypeKind LitConstExpr::GetNumLitTypeKind()
{
    int suffixWidth = 0;
    if (kind == LitConstKind::RUNE_BYTE) {
        return TypeKind::TYPE_UINT8;
    }
    if (kind == LitConstKind::INTEGER) {
        auto suffixStart =
            std::find_if(stringValue.begin(), stringValue.end(), [](char c) { return c == 'i' || c == 'u'; });
        std::string suffix = std::string(suffixStart, stringValue.end());
        if (suffix.empty()) {
            return TypeKind::TYPE_IDEAL_INT;
        } else {
            if (auto suffixWid = Stoi(std::string(suffix.begin() + 1, suffix.end()))) {
                suffixWidth = *suffixWid;
            } else {
                return TypeKind::TYPE_INVALID;
            }
            // The following will calculate logarithm 8, 16, 32, 64 base 2 and will get
            // 3,4,5,6, take 3(int) or 4(float) as base and plus powerBase2 - 3 + INT8 or UINT8 or FLOAT16
            int powerBase2 = __builtin_ctz(static_cast<unsigned>(suffixWidth));
            char signedness = suffix[0];
            // The following code violates P.08-CPP(V5.0), however, I think it is OK since the number of width must
            // be the several possibilities. If not the case, lexer would report an error in the early stage. Can
            // only be i, u, f three cases
            if (signedness == 'i' || signedness == 'u') {
                int leastPowerBase2 = 3; // int starts from 8=2^3 bits width
                TypeKind tk = signedness == 'i' ? TypeKind::TYPE_INT8 : TypeKind::TYPE_UINT8;
                return static_cast<TypeKind>(powerBase2 - leastPowerBase2 + static_cast<int>(tk));
            } else {
                return TypeKind::TYPE_INVALID;
            }
        }
    } else if (kind == LitConstKind::FLOAT) {
        // Check whether it is a hexadecimal floating pointing number. If it is then
        // 'f' is allowed as digits and there will not be any suffix.
        // We should skip the negative sign if it exists.
        bool isNegative = !stringValue.empty() && stringValue.front() == '-';
        std::string prefix = stringValue.substr(isNegative ? 1 : 0, std::min<size_t>(stringValue.size(), 2UL));
        if (prefix == "0x" || prefix == "0X") {
            return TypeKind::TYPE_IDEAL_FLOAT;
        }
        auto suffixStart = std::find_if(stringValue.begin(), stringValue.end(), [](char c) { return c == 'f'; });
        std::string suffix = std::string(suffixStart, stringValue.end());
        if (suffix.empty()) {
            return TypeKind::TYPE_IDEAL_FLOAT;
        } else {
            if (auto suffixWid = Stoi(std::string(suffix.begin() + 1, suffix.end()))) {
                suffixWidth = *suffixWid;
            } else {
                return TypeKind::TYPE_INVALID;
            }
            int powerBase2 = __builtin_ctz(static_cast<unsigned>(suffixWidth));
            int leastPowerBase2 = 4;
            return static_cast<TypeKind>(powerBase2 - leastPowerBase2 + static_cast<int>(TypeKind::TYPE_FLOAT16));
        }
    }
    return TypeKind::TYPE_INVALID;
}

std::string Type::ToString() const
{
    if (!typeParameterName.empty()) {
        return GetTypeParameterNameRawText();
    }
    return Node::ToString();
}

std::string InvalidType::ToString() const
{
    return "<invalid type>";
}

std::string PrimitiveType::ToString() const
{
    if (!str.empty()) {
        return str;
    }
    return std::string(TypeKindName(kind));
}

std::string RefType::ToString() const
{
    std::stringstream ss;
    ss << ref.identifier.GetRawText();
    if (!typeArguments.empty()) {
        ss << "<";
        for (size_t i = 0; i < typeArguments.size(); ++i) {
            if (i > 0) {
                ss << ", ";
            }
            if (typeArguments[i]) {
                ss << typeArguments[i]->ToString();
            }
        }
        ss << ">";
    }
    return ss.str();
}

std::string QualifiedType::ToString() const
{
    std::stringstream ss;
    if (baseType) {
        ss << baseType->ToString() << ".";
    }
    ss << field.GetRawText();
    if (!typeArguments.empty()) {
        ss << "<";
        for (size_t i = 0; i < typeArguments.size(); ++i) {
            if (i > 0) {
                ss << ", ";
            }
            if (typeArguments[i]) {
                ss << typeArguments[i]->ToString();
            }
        }
        ss << ">";
    }
    return ss.str();
}

std::string OptionType::ToString() const
{
    std::stringstream ss;
    size_t count = questNum;
    if (count == 0) {
        count = questVector.size();
    }
    for (size_t i = 0; i < count; ++i) {
        ss << "?";
    }
    if (componentType) {
        ss << componentType->ToString();
    }
    return ss.str();
}

std::string ConstantType::ToString() const
{
    std::stringstream ss;
    ss << "$";
    if (constantExpr) {
        ss << constantExpr->ToString();
    }
    return ss.str();
}

std::string VArrayType::ToString() const
{
    std::stringstream ss;
    ss << "VArray<";
    if (typeArgument) {
        ss << typeArgument->ToString();
        if (constantType) {
            ss << ", ";
        }
    }
    if (constantType) {
        ss << constantType->ToString();
    }
    ss << ">";
    return ss.str();
}

std::string ParenType::ToString() const
{
    std::stringstream ss;
    ss << "(";
    if (type) {
        ss << type->ToString();
    }
    ss << ")";
    return ss.str();
}

std::string FuncType::ToString() const
{
    std::stringstream ss;
    ss << "(";
    for (size_t i = 0; i < paramTypes.size(); ++i) {
        if (i > 0) {
            ss << ", ";
        }
        auto param = paramTypes[i].get();
        if (!param) {
            continue;
        }
        if (!param->typeParameterName.empty()) {
            ss << param->GetTypeParameterNameRawText() << ": ";
        }
        ss << param->ToString();
    }
    ss << ")";
    if (retType) {
        ss << " -> " << retType->ToString();
    }
    return ss.str();
}

std::string TupleType::ToString() const
{
    std::stringstream ss;
    ss << "(";
    for (size_t i = 0; i < fieldTypes.size(); ++i) {
        if (i > 0) {
            ss << ", ";
        }
        auto field = fieldTypes[i].get();
        if (!field) {
            continue;
        }
        if (!field->typeParameterName.empty()) {
            ss << field->GetTypeParameterNameRawText() << ": ";
        }
        ss << field->ToString();
    }
    ss << ")";
    return ss.str();
}

std::string ThisType::ToString() const
{
    return TOKENS[static_cast<int>(TokenKind::THISTYPE)];
}

std::string RefExpr::ToString() const
{
    return ref.identifier;
}

std::string ArrayLit::ToString() const
{
    std::stringstream ss;
    ss << "[";
    Position curSpanBegin = begin + Position{0, 0, 1};
    for (size_t i = 0; i < children.size(); ++i) {
        auto& child = children[i];
        if (!child) {
            continue;
        }
        ss << NextSpan(child->ToString(), curSpanBegin, child->end);
        curSpanBegin = child->end;
        if (i < commaPosVector.size()) {
            ss << NextSpan(",", curSpanBegin, commaPosVector[i], 1);
            curSpanBegin = commaPosVector[i] + Position{0, 0, 1};
        }
    }
    ss << NextSpan("]", curSpanBegin, rightSquarePos, 1);
    return ss.str();
}

std::string ArrayExpr::ToString() const
{
    std::stringstream ss;
    if (!type) {
        return ss.str();
    }
    Position curSpanBegin = begin;
    ss << NextSpan(type->ToString(), curSpanBegin, type->end);
    curSpanBegin = type->end;
    ss << NextSpan("(", curSpanBegin, leftParenPos, 1);
    curSpanBegin = leftParenPos + Position{0, 0, 1};
    for (size_t i = 0; i < args.size(); ++i) {
        if (!args[i]) {
            continue;
        }
        ss << NextSpan(args[i]->ToString(), curSpanBegin, args[i]->end);
        curSpanBegin = args[i]->end;
        if (i < commaPosVector.size()) {
            ss << NextSpan(",", curSpanBegin, commaPosVector[i], 1);
            curSpanBegin = commaPosVector[i] + Position{0, 0, 1};
        }
    }
    ss << NextSpan(")", curSpanBegin, rightParenPos, 1);
    return ss.str();
}

std::string PointerExpr::ToString() const
{
    std::stringstream ss;
    std::string expr = "CPointer<";
    Position curSpanBegin = begin;
    Position curSpanEnd = curSpanBegin + Position{0, 0, static_cast<int>(expr.size())};
    ss << NextSpan(expr, curSpanBegin, curSpanEnd);

    curSpanBegin = curSpanEnd;
    if (ty && !ty->typeArgs.empty()) {
        expr = Ty::ToString(ty->typeArgs[0]);
        curSpanEnd = curSpanBegin + Position{0, 0, static_cast<int>(expr.size())};
        ss << NextSpan(expr, curSpanBegin, curSpanEnd);
    }

    // 2 is string length of ">(".
    curSpanEnd = curSpanBegin + Position{0, 0, 2};
    ss << NextSpan(">(", curSpanBegin, curSpanEnd);
    curSpanBegin = curSpanEnd;
    if (arg) {
        expr = arg->ToString();
        curSpanEnd = curSpanBegin + Position{0, 0, static_cast<int>(expr.size())};
        ss << NextSpan(expr, curSpanBegin, curSpanEnd);
    }
    ss << NextSpan(")", curSpanBegin, curSpanBegin + Position{0, 0, 1});
    return ss.str();
}

std::string ParenExpr::ToString() const
{
    std::stringstream ss;
    Position curSpanBegin = leftParenPos;
    ss << NextSpan("(", curSpanBegin, leftParenPos, 1);
    curSpanBegin = leftParenPos + Position{0, 0, 1};
    if (expr) {
        ss << NextSpan(expr->ToString(), curSpanBegin, expr->end);
        curSpanBegin = expr->end;
    }
    ss << NextSpan(")", curSpanBegin, rightParenPos, 1);
    return ss.str();
}

std::string TupleLit::ToString() const
{
    std::stringstream ss;
    ss << "(";
    Position curSpanBegin = leftParenPos + Position{0, 0, 1};
    for (size_t i = 0; i < children.size(); ++i) {
        auto& child = children[i];
        if (!child) {
            continue;
        }
        ss << NextSpan(child->ToString(), curSpanBegin, child->end);
        curSpanBegin = child->end;
        if (i < commaPosVector.size()) {
            ss << NextSpan(",", curSpanBegin, commaPosVector[i], 1);
            curSpanBegin = commaPosVector[i] + Position{0, 0, 1};
        }
    }
    ss << NextSpan(")", curSpanBegin, rightParenPos, 1);
    return ss.str();
}

std::string ForInExpr::ToString() const
{
    std::stringstream ss;
    ss << "for (";
    if (pattern) {
        ss << pattern->ToString();
    }
    ss << " in ";
    if (inExpression) {
        ss << inExpression->ToString();
    }
    ss << ")";
    if (patternGuard) {
        ss << " if ";
        ss << patternGuard->ToString();
    }
    if (body) {
        ss << " " << body->ToString();
    }
    return ss.str();
}

std::string IfExpr::ToString() const
{
    std::stringstream ss;
    ss << "if (";
    if (condExpr) {
        ss << condExpr->ToString();
    }
    ss << ")";
    if (thenBody) {
        ss << " " << thenBody->ToString();
    }
    if (hasElse && elseBody) {
        ss << " else ";
        ss << elseBody->ToString();
    }
    return ss.str();
}

std::string UnaryExpr::ToString() const
{
    std::stringstream ss;
    std::string opStr = TOKENS[static_cast<int>(op)];
    Position curSpanBegin = operatorPos;
    ss << NextSpan(opStr, curSpanBegin, operatorPos, static_cast<int>(opStr.size()));
    curSpanBegin = operatorPos + Position{0, 0, static_cast<int>(opStr.size())};
    if (expr) {
        ss << NextSpan(expr->ToString(), curSpanBegin, expr->end);
    }
    return ss.str();
}

std::string BinaryExpr::ToString() const
{
    std::stringstream ss;
    if (leftExpr) {
        Position curSpanBegin = leftExpr->begin;
        ss << NextSpan(leftExpr->ToString(), curSpanBegin, leftExpr->end);
        curSpanBegin = leftExpr->end;
        std::string opStr = TOKENS[static_cast<int>(op)];
        ss << NextSpan(opStr, curSpanBegin, operatorPos, static_cast<int>(opStr.size()));
        curSpanBegin = operatorPos + Position{0, 0, static_cast<int>(opStr.size())};
        if (rightExpr) {
            ss << NextSpan(rightExpr->ToString(), curSpanBegin, rightExpr->end);
        }
    }
    return ss.str();
}

std::string AssignExpr::ToString() const
{
    std::stringstream ss;
    if (leftValue) {
        Position curSpanBegin = leftValue->begin;
        ss << NextSpan(leftValue->ToString(), curSpanBegin, leftValue->end);
        curSpanBegin = leftValue->end;
        std::string opStr;
        if (isCompound && op != TokenKind::ILLEGAL) {
            opStr = TOKENS[static_cast<int>(op)];
        } else {
            opStr = "=";
        }
        ss << NextSpan(opStr, curSpanBegin, assignPos, static_cast<int>(opStr.size()));
        curSpanBegin = assignPos + Position{0, 0, static_cast<int>(opStr.size())};
        if (rightExpr) {
            ss << NextSpan(rightExpr->ToString(), curSpanBegin, rightExpr->end);
        }
    }
    return ss.str();
}

std::string SubscriptExpr::ToString() const
{
    std::stringstream ss;
    if (baseExpr) {
        Position curSpanBegin = baseExpr->begin;
        ss << NextSpan(baseExpr->ToString(), curSpanBegin, baseExpr->end);
        curSpanBegin = baseExpr->end;
        ss << NextSpan("[", curSpanBegin, leftParenPos, 1);
        curSpanBegin = leftParenPos + Position{0, 0, 1};
        for (size_t i = 0; i < indexExprs.size(); ++i) {
            auto& idx = indexExprs[i];
            if (!idx) {
                continue;
            }
            ss << NextSpan(idx->ToString(), curSpanBegin, idx->end);
            curSpanBegin = idx->end;
            if (i < commaPos.size()) {
                ss << NextSpan(",", curSpanBegin, commaPos[i], 1);
                curSpanBegin = commaPos[i] + Position{0, 0, 1};
            }
        }
        ss << NextSpan("]", curSpanBegin, rightParenPos, 1);
    }
    return ss.str();
}

std::string Expr::ToString() const
{
    return Node::ToString();
}

std::string WildcardExpr::ToString() const
{
    return "_";
}

std::string TokenPart::ToString() const
{
    if (tokens.empty()) {
        return "";
    }
    std::stringstream ss;
    for (const auto& token : tokens) {
        const auto& value = token.Value();
        if (!value.empty()) {
            ss << value;
        } else {
            ss << TokenLiteral(token.kind);
        }
    }
    return ss.str();
}

std::string QuoteExpr::ToString() const
{
    std::stringstream ss;
    ss << "quote(";
    for (size_t i = 0; i < exprs.size(); ++i) {
        if (i > 0) {
            ss << ", ";
        }
        if (exprs[i]) {
            ss << exprs[i]->ToString();
        }
    }
    ss << ")";
    return ss.str();
}

std::string InterpolationExpr::ToString() const
{
    if (!rawString.empty()) {
        return rawString;
    }
    std::stringstream ss;
    ss << "${";
    if (block) {
        ss << block->ToString();
    }
    ss << "}";
    return ss.str();
}

std::string StrInterpolationExpr::ToString() const
{
    if (!rawString.empty()) {
        return rawString;
    }
    std::stringstream ss;
    ss << '"';
    for (const auto& part : strPartExprs) {
        if (part) {
            ss << part->ToString();
        }
    }
    ss << '"';
    return ss.str();
}

std::string IncOrDecExpr::ToString() const
{
    auto operand = expr ? expr->ToString() : std::string{};
    auto opStr = std::string(TokenLiteral(op));
    if (operand.empty()) {
        return opStr;
    }
    if (expr && expr->begin < operatorPos) {
        return operand + opStr;
    }
    return opStr + operand;
}

std::string OptionalExpr::ToString() const
{
    if (!baseExpr) {
        return "?";
    }
    return baseExpr->ToString() + "?";
}

std::string OptionalChainExpr::ToString() const
{
    if (!expr) {
        return "?.";
    }
    return expr->ToString() + "?.";
}

std::string PrimitiveTypeExpr::ToString() const
{
    return std::string(TypeKindName(typeKind));
}

std::string LambdaExpr::ToString() const
{
    std::stringstream ss;
    ss << "lambda";
    if (funcBody) {
        ss << funcBody->ToString();
    }
    return ss.str();
}

std::string TrailingClosureExpr::ToString() const
{
    std::stringstream ss;
    if (expr) {
        ss << expr->ToString();
    } else {
        ss << "<expr>";
    }
    ss << " trailing ";
    if (lambda) {
        ss << lambda->ToString();
    } else {
        ss << "<lambda>";
    }
    return ss.str();
}

std::string TypeConvExpr::ToString() const
{
    std::stringstream ss;
    ss << (type ? type->ToString() : "typeconv") << "(";
    if (expr) {
        ss << expr->ToString();
    }
    ss << ")";
    return ss.str();
}

std::string SpawnExpr::ToString() const
{
    std::stringstream ss;
    ss << "spawn";
    if (task) {
        ss << " " << task->ToString();
    }
    if (arg) {
        ss << " with " << arg->ToString();
    }
    if (futureObj) {
        ss << " -> " << futureObj->ToString();
    }
    return ss.str();
}

std::string SynchronizedExpr::ToString() const
{
    std::stringstream ss;
    ss << "synchronized(";
    if (mutex) {
        ss << mutex->ToString();
    }
    ss << ")";
    if (body) {
        ss << " " << body->ToString();
    }
    return ss.str();
}

std::string InvalidExpr::ToString() const
{
    if (!value.empty()) {
        return value;
    }
    return "<invalid expr>";
}

std::string Block::ToString() const
{
    std::stringstream ss;
    ss << "{";
    Position curSpanBegin = leftCurlPos + Position{0, 0, 1};
    for (auto& child : body) {
        if (!child) continue;
        ss << NextSpan(child->ToString(), curSpanBegin, child->end);
        curSpanBegin = child->end;
    }
    ss << NextSpan("}", curSpanBegin, rightCurlPos, 1);
    return ss.str();
}

std::string ReturnExpr::ToString() const
{
    std::stringstream ss;
    Position curSpanBegin = returnPos;
    ss << NextSpan("return", curSpanBegin, returnPos, 6);
    curSpanBegin = returnPos + Position{0, 0, 6};
    if (expr) {
        ss << NextSpan(expr->ToString(), curSpanBegin, expr->end);
        curSpanBegin = expr->end;
    }
    if (semiPos != INVALID_POSITION) {
        ss << NextSpan(";", curSpanBegin, semiPos, 1);
    }
    return ss.str();
}

std::string IsExpr::ToString() const
{
    std::stringstream ss;
    if (leftExpr) {
        Position curSpanBegin = leftExpr->begin;
        ss << NextSpan(leftExpr->ToString(), curSpanBegin, leftExpr->end);
        curSpanBegin = leftExpr->end;
        ss << NextSpan("is", curSpanBegin, isPos, 2);
        curSpanBegin = isPos + Position{0, 0, 2};
        if (isType) {
            ss << NextSpan(isType->ToString(), curSpanBegin, isType->end);
        }
    }
    return ss.str();
}

std::string AsExpr::ToString() const
{
    std::stringstream ss;
    if (leftExpr) {
        Position curSpanBegin = leftExpr->begin;
        ss << NextSpan(leftExpr->ToString(), curSpanBegin, leftExpr->end);
        curSpanBegin = leftExpr->end;
        ss << NextSpan("as", curSpanBegin, asPos, 2);
        curSpanBegin = asPos + Position{0, 0, 2};
        if (asType) {
            ss << NextSpan(asType->ToString(), curSpanBegin, asType->end);
        }
    }
    return ss.str();
}

std::string RangeExpr::ToString() const
{
    std::stringstream ss;
    Position curSpanBegin = begin;
    if (startExpr) {
        ss << NextSpan(startExpr->ToString(), curSpanBegin, startExpr->end);
        curSpanBegin = startExpr->end;
    }
    ss << NextSpan("...", curSpanBegin, rangePos, 3);
    curSpanBegin = rangePos + Position{0, 0, 3};
    if (stopExpr) {
        ss << NextSpan(stopExpr->ToString(), curSpanBegin, stopExpr->end);
        curSpanBegin = stopExpr->end;
    }
    if (stepExpr) {
        ss << NextSpan(":", curSpanBegin, colonPos, 1);
        curSpanBegin = colonPos + Position{0, 0, 1};
        ss << NextSpan(stepExpr->ToString(), curSpanBegin, stepExpr->end);
    }
    return ss.str();
}

std::string LetPatternDestructor::ToString() const
{
    std::stringstream ss;
    ss << "let ";
    for (size_t i = 0; i < patterns.size(); ++i) {
        if (i > 0) {
            ss << " | ";
        }
        if (patterns[i]) {
            ss << patterns[i]->ToString();
        }
    }
    ss << " <- ";
    if (initializer) {
        ss << initializer->ToString();
    }
    if (semiPos != INVALID_POSITION) {
        ss << ";";
    }
    return ss.str();
}

std::string MatchCase::ToString() const
{
    std::stringstream ss;
    for (size_t i = 0; i < patterns.size(); ++i) {
        if (i > 0) {
            ss << " | ";
        }
        if (patterns[i]) {
            ss << patterns[i]->ToString();
        }
    }
    if (patternGuard) {
        ss << " where " << patternGuard->ToString();
    }
    ss << " => ";
    if (exprOrDecls) {
        ss << exprOrDecls->ToString();
    }
    return ss.str();
}

std::string MatchCaseOther::ToString() const
{
    std::stringstream ss;
    if (matchExpr) {
        ss << matchExpr->ToString();
    }
    ss << " => ";
    if (exprOrDecls) {
        ss << exprOrDecls->ToString();
    }
    return ss.str();
}

std::string MatchExpr::ToString() const
{
    std::stringstream ss;
    ss << "match";
    if (selector) {
        ss << " (" << selector->ToString() << ")";
    }
    ss << " {";
    bool hasCase = false;
    for (auto& matchCase : matchCases) {
        if (!matchCase) {
            continue;
        }
        ss << " " << matchCase->ToString();
        hasCase = true;
    }
    for (auto& other : matchCaseOthers) {
        if (!other) {
            continue;
        }
        ss << " " << other->ToString();
        hasCase = true;
    }
    if (hasCase) {
        ss << " ";
    }
    ss << "}";
    if (semiPos != INVALID_POSITION) {
        ss << ";";
    }
    return ss.str();
}

std::string TryExpr::ToString() const
{
    std::stringstream ss;
    ss << "try";
    if (!resourceSpec.empty()) {
        ss << " (";
        for (size_t i = 0; i < resourceSpec.size(); ++i) {
            if (i > 0) {
                ss << ", ";
            }
            if (resourceSpec[i]) {
                ss << resourceSpec[i]->ToString();
            }
        }
        ss << ")";
    }
    if (tryBlock) {
        ss << " " << tryBlock->ToString();
    }
    size_t catchCount = std::max(catchBlocks.size(), catchPatterns.size());
    for (size_t i = 0; i < catchCount; ++i) {
        ss << " catch";
        if (i < catchPatterns.size() && catchPatterns[i]) {
            ss << " (" << catchPatterns[i]->ToString() << ")";
        }
        if (i < catchBlocks.size() && catchBlocks[i]) {
            ss << " " << catchBlocks[i]->ToString();
        }
    }
    if (finallyBlock) {
        ss << " finally " << finallyBlock->ToString();
    }
    if (semiPos != INVALID_POSITION) {
        ss << ";";
    }
    return ss.str();
}

std::string ThrowExpr::ToString() const
{
    std::stringstream ss;
    ss << "throw";
    if (expr) {
        ss << " " << expr->ToString();
    }
    if (semiPos != INVALID_POSITION) {
        ss << ";";
    }
    return ss.str();
}

std::string PerformExpr::ToString() const
{
    std::stringstream ss;
    ss << "perform";
    if (expr) {
        ss << " " << expr->ToString();
    }
    if (semiPos != INVALID_POSITION) {
        ss << ";";
    }
    return ss.str();
}

std::string ResumeExpr::ToString() const
{
    std::stringstream ss;
    ss << "resume";
    if (withExpr) {
        ss << " with " << withExpr->ToString();
    }
    if (throwingExpr) {
        ss << " throwing " << throwingExpr->ToString();
    }
    if (semiPos != INVALID_POSITION) {
        ss << ";";
    }
    return ss.str();
}

std::string JumpExpr::ToString() const
{
    std::stringstream ss;
    ss << (isBreak ? "break" : "continue");
    if (semiPos != INVALID_POSITION) {
        ss << ";";
    }
    return ss.str();
}

std::string WhileExpr::ToString() const
{
    std::stringstream ss;
    ss << "while (";
    if (condExpr) {
        ss << condExpr->ToString();
    }
    ss << ")";
    if (body) {
        ss << " " << body->ToString();
    }
    if (semiPos != INVALID_POSITION) {
        ss << ";";
    }
    return ss.str();
}

std::string DoWhileExpr::ToString() const
{
    std::stringstream ss;
    ss << "do";
    if (body) {
        ss << " " << body->ToString();
    }
    ss << " while (";
    if (condExpr) {
        ss << condExpr->ToString();
    }
    ss << ")";
    if (semiPos != INVALID_POSITION) {
        ss << ";";
    }
    return ss.str();
}

Ptr<Node> Block::GetLastExprOrDecl() const
{
    if (body.empty()) {
        return nullptr;
    }

    return body.back().get();
}

Node::~Node()
{
    Node::Clear();
    if (symbol && symbol->node == this) {
        symbol->invertedIndexBeenDeleted = true;
    }
}

bool Node::ShouldDiagnose(bool allowCompilerAdd) const
{
    if (!allowCompilerAdd) {
        if (TestAttr(Attribute::COMPILER_ADD) && !TestAttr(Attribute::IS_CLONED_SOURCE_CODE)) {
            return false;
        }
    } else if (TestAttr(Attribute::COMPILER_ADD) && begin.IsZero()) {
        // Should not diagnose when the position is empty even the 'allowCompilerAdd' flag is true.
        return false;
    }
    // Ignore macro added nodes.
    if (TestAttr(Attribute::MACRO_INVOKE_FUNC) || TestAttr(Attribute::MACRO_INVOKE_BODY)) {
        return false;
    }
    if (auto e = DynamicCast<const AST::Expr*>(this); e) {
        if (e->sourceExpr) {
            return false;
        }
    }
    return true;
}

bool Node::IsSamePackage(const Node& other) const
{
    Ptr<File> otherFile = other.curFile;
    if (!curFile || !otherFile) {
        return true;
    }
    Ptr<Package> curPackage = curFile->curPackage;
    Ptr<Package> otherPackage = otherFile->curPackage;
    if (!curPackage || !otherPackage) {
        return true;
    }
    return curPackage == otherPackage;
}

void SubscriptExpr::Clear() noexcept
{
    RecoverToSubscriptExpr(*this);
    if (baseExpr) {
        baseExpr->Clear();
    }
    for (auto& indexExpr : indexExprs) {
        if (indexExpr) {
            indexExpr->Clear();
        }
    }
    commaPos.clear();
    Expr::Clear();
}

void AssignExpr::Clear() noexcept
{
    RecoverToAssignExpr(*this);
    Expr::Clear();
}

void UnaryExpr::Clear() noexcept
{
    RecoverToUnaryExpr(*this);
    Expr::Clear();
    if (expr) {
        expr->Clear();
    }
}

void BinaryExpr::Clear() noexcept
{
    RecoverToBinaryExpr(*this);
    Expr::Clear();
    if (leftExpr) {
        leftExpr->Clear();
    }
    if (rightExpr) {
        rightExpr->Clear();
    }
}

void ParenExpr::Clear() noexcept
{
    Expr::Clear();
    if (expr) {
        expr->Clear();
    }
}

bool RefType::IsGenericThisType() const
{
    if (auto cd = DynamicCast<ClassDecl*>(ref.target);
        cd && cd->generic && !cd->generic->typeParameters.empty() && ref.identifier == "This") {
        return true;
    }
    return false;
}

std::set<Ptr<InterfaceTy>> InheritableDecl::GetSuperInterfaceTys() const
{
    std::set<Ptr<InterfaceTy>> ret;
    for (auto& types : inheritedTypes) {
        if (types && types->ty && types->ty->kind == TypeKind::TYPE_INTERFACE) {
            ret.insert(RawStaticCast<InterfaceTy*>(types->ty));
        }
    }
    return ret;
}

std::vector<Ptr<InterfaceTy>> InheritableDecl::GetStableSuperInterfaceTys() const
{
    auto cmp = [](const Ptr<InterfaceTy> ty1, const Ptr<InterfaceTy> ty2) { return CompTyByNames(ty1, ty2); };

    std::set<Ptr<InterfaceTy>, decltype(cmp)> ret(cmp);
    for (auto& types : inheritedTypes) {
        if (types && types->ty && types->ty->kind == TypeKind::TYPE_INTERFACE) {
            ret.emplace(RawStaticCast<InterfaceTy*>(types->ty));
        }
    }
    return std::vector<Ptr<InterfaceTy>>(ret.begin(), ret.end());
}

std::vector<Ptr<ClassLikeDecl>> InheritableDecl::GetAllSuperDecls()
{
    std::set<Ptr<ClassLikeDecl>> visited; // to avoid multiple paths or cycle
    std::vector<Ptr<ClassLikeDecl>> ret; // to guarantee order
    std::queue<Ptr<InheritableDecl>> workList;
    workList.push(this);
    if (auto cd = DynamicCast<ClassLikeDecl*>(this)) {
        visited.emplace(cd);
        ret.emplace_back(cd);
    }
    while (!workList.empty()) {
        auto curDecl = workList.front();
        workList.pop();
        for (auto& it : curDecl->inheritedTypes) {
            if (auto clsTy = DynamicCast<ClassTy*>(it->ty); clsTy && visited.count(clsTy->declPtr) == 0) {
                workList.push(clsTy->declPtr);
                visited.emplace(clsTy->declPtr);
                ret.emplace_back(clsTy->declPtr);
            } else if (auto interfaceTy = DynamicCast<InterfaceTy*>(it->ty);
                       interfaceTy && visited.count(interfaceTy->declPtr) == 0) {
                workList.push(interfaceTy->declPtr);
                visited.emplace(interfaceTy->declPtr);
                ret.emplace_back(interfaceTy->declPtr);
            }
        }
    }
    return ret;
}

std::vector<Ptr<Decl>> Decl::GetMemberDeclPtrs() const
{
    std::vector<Ptr<Decl>> results;
    if (auto cd = DynamicCast<const ClassDecl*>(this); cd) {
        for (auto& decl : cd->body->decls) {
            results.push_back(decl.get());
        }
    } else if (auto id = DynamicCast<const InterfaceDecl*>(this); id) {
        for (auto& decl : id->body->decls) {
            results.push_back(decl.get());
        }
    } else if (auto sd = DynamicCast<const StructDecl*>(this); sd) {
        for (auto& decl : sd->body->decls) {
            results.push_back(decl.get());
        }
    } else if (auto ed = DynamicCast<const EnumDecl*>(this); ed) {
        for (auto& constructor : ed->constructors) {
            results.emplace_back(constructor.get());
        }
        for (auto& decl : ed->members) {
            results.push_back(decl.get());
        }
    } else if (auto exd = DynamicCast<const ExtendDecl*>(this); exd) {
        for (auto& decl : exd->members) {
            results.push_back(decl.get());
        }
    }
    return results;
}

void Node::SetTarget(Ptr<Decl> target)
{
    switch (astKind) {
        case ASTKind::REF_TYPE: {
            RawStaticCast<RefType*>(this)->ref.target = target;
            break;
        }
        case ASTKind::REF_EXPR: {
            RawStaticCast<RefExpr*>(this)->ref.target = target;
            break;
        }
        case ASTKind::QUALIFIED_TYPE: {
            RawStaticCast<QualifiedType*>(this)->target = target;
            break;
        }
        case ASTKind::MEMBER_ACCESS: {
            RawStaticCast<MemberAccess*>(this)->target = target;
            break;
        }
        case ASTKind::MACRO_EXPAND_DECL: {
            RawStaticCast<MacroExpandDecl*>(this)->invocation.target = target;
            break;
        }
        case ASTKind::MACRO_EXPAND_EXPR: {
            RawStaticCast<MacroExpandExpr*>(this)->invocation.target = target;
            break;
        }
        case ASTKind::MACRO_EXPAND_PARAM: {
            RawStaticCast<MacroExpandParam*>(this)->invocation.target = target;
            break;
        }
        default:
            return;
    }
}

Ptr<Decl> Node::GetTarget() const
{
    switch (astKind) {
        case ASTKind::REF_TYPE: {
            return RawStaticCast<const RefType*>(this)->ref.target;
        }
        case ASTKind::REF_EXPR: {
            return RawStaticCast<const RefExpr*>(this)->ref.target;
        }
        case ASTKind::QUALIFIED_TYPE: {
            return RawStaticCast<const QualifiedType*>(this)->target;
        }
        case ASTKind::MEMBER_ACCESS: {
            return RawStaticCast<const MemberAccess*>(this)->target;
        }
        case ASTKind::MACRO_EXPAND_DECL: {
            return RawStaticCast<const MacroExpandDecl*>(this)->invocation.target;
        }
        case ASTKind::MACRO_EXPAND_EXPR: {
            return RawStaticCast<const MacroExpandExpr*>(this)->invocation.target;
        }
        case ASTKind::MACRO_EXPAND_PARAM: {
            return RawStaticCast<const MacroExpandParam*>(this)->invocation.target;
        }
        default:
            return nullptr;
    }
}

std::vector<Ptr<Decl>> Node::GetTargets() const
{
    switch (astKind) {
        case ASTKind::REF_TYPE: {
            return RawStaticCast<const RefType*>(this)->ref.targets;
        }
        case ASTKind::REF_EXPR: {
            return RawStaticCast<const RefExpr*>(this)->ref.targets;
        }
        case ASTKind::MEMBER_ACCESS: {
            auto targetDecls = RawStaticCast<const MemberAccess*>(this)->targets;
            std::vector<Ptr<Decl>> decls(targetDecls.begin(), targetDecls.end());
            return decls;
        }
        default:
            return {};
    }
}

/**
 * Get a MacroInvocation ptr.
 * @return MacroInvocation ptr if a node is MacroExpandExpr or MacroExpandDecl,
 *  nullptr otherwise.
 */
Ptr<const MacroInvocation> Node::GetConstInvocation() const
{
    if (this->astKind == ASTKind::MACRO_EXPAND_EXPR) {
        auto mc = RawStaticCast<const MacroExpandExpr*>(this);
        return &(mc->invocation);
    }
    if (this->astKind == ASTKind::MACRO_EXPAND_DECL) {
        auto mc = RawStaticCast<const MacroExpandDecl*>(this);
        return &(mc->invocation);
    }
    if (this->astKind == ASTKind::MACRO_EXPAND_PARAM) {
        auto mc = RawStaticCast<const MacroExpandParam*>(this);
        return &(mc->invocation);
    }
    return nullptr;
}

bool MacroInvocation::IsIfAvailable() const
{
    return fullName == IF_AVAILABLE;
}

Ptr<FuncArg> MacroExpandExpr::GetNamedArg() const
{
    if (invocation.IsIfAvailable() && !invocation.nodes.empty()) {
        return StaticCast<FuncArg>(invocation.nodes[0].get());
    }
    return {};
}
Ptr<LambdaExpr> MacroExpandExpr::GetLambda(size_t i) const
{
    if (invocation.IsIfAvailable() && invocation.nodes.size() > i) {
        return StaticCast<LambdaExpr>(invocation.nodes[i + 1].get());
    }
    return {};
}
std::tuple<OwnedPtr<FuncArg>, OwnedPtr<LambdaExpr>, OwnedPtr<LambdaExpr>> MacroExpandExpr::Decompose()
{
    return {OwnedPtr<FuncArg>(StaticCast<FuncArg>(std::move(invocation.nodes[0].release()))),
        OwnedPtr<LambdaExpr>(StaticCast<LambdaExpr>(std::move(invocation.nodes[1].release()))),
        OwnedPtr<LambdaExpr>(StaticCast<LambdaExpr>(std::move(invocation.nodes[2].release())))};
}

/**
 * Get a MacroInvocation ptr.
 * @return MacroInvocation ptr if a node is MacroExpandExpr or MacroExpandDecl,
 *  nullptr otherwise.
 */
Ptr<MacroInvocation> Node::GetInvocation()
{
    if (this->astKind == ASTKind::MACRO_EXPAND_EXPR) {
        auto mc = RawStaticCast<MacroExpandExpr*>(this);
        return &(mc->invocation);
    }
    if (this->astKind == ASTKind::MACRO_EXPAND_DECL) {
        auto mc = RawStaticCast<MacroExpandDecl*>(this);
        return &(mc->invocation);
    }
    if (this->astKind == ASTKind::MACRO_EXPAND_PARAM) {
        auto mc = RawStaticCast<MacroExpandParam*>(this);
        return &(mc->invocation);
    }
    return nullptr;
}

/**
 * Get the new Position of macrocall in curfile by originPos before the macro is expanded, for lsp.
 * @return new Position of macrocall in curfile if the Node is MacroExpandExpr/MacroExpandDecl or in macrocall,
 *  INVALID_POSITION otherwise.
 */
Position Node::GetMacroCallNewPos(const Position& originPos)
{
    Ptr<MacroInvocation> pInvocation = nullptr;
    if (this->isInMacroCall && this->curMacroCall) {
        Ptr<Node> tempNode = this->curMacroCall;
        // Get outermost macrocall.
        while (tempNode->curMacroCall) {
            tempNode = tempNode->curMacroCall;
        }
        pInvocation = tempNode->GetInvocation();
    }
    if (this->IsMacroCallNode()) {
        Ptr<Node> tempNode = this;
        // Get outermost macrocall.
        while (tempNode->curMacroCall) {
            tempNode = tempNode->curMacroCall;
        }
        pInvocation = tempNode->GetInvocation();
    }
    if (!pInvocation || pInvocation->originPosMap.empty() || pInvocation->origin2newPosMap.empty()) {
        return INVALID_POSITION;
    }
    auto key = static_cast<unsigned int>(originPos.Hash32());
    if (pInvocation->originPosMap.lower_bound(key) == pInvocation->originPosMap.end()) {
        return INVALID_POSITION;
    }
    auto newkey = pInvocation->originPosMap.lower_bound(key)->second.Hash64();
    if (pInvocation->origin2newPosMap.find(newkey) != pInvocation->origin2newPosMap.cend()) {
        return pInvocation->origin2newPosMap.at(newkey);
    }
    return INVALID_POSITION;
}

/**
 * Get the sourcePos of macrocall by originPos in curfile.
 * @return the sourcePos in macrocall file if the Node is expanded from macrocall,
 *  originPos in curfile otherwise.
 */
Position Node::GetMacroCallPos(Position originPos, bool isLowerBound) const
{
    if (this->TestAttr(Attribute::MACRO_EXPANDED_NODE) && this->curMacroCall) {
        auto pInvocation = this->curMacroCall->GetConstInvocation();
        if (pInvocation && !IsPureAnnotation(*pInvocation)) {
            return GetMacroSourcePos(*pInvocation, originPos, isLowerBound);
        }
    }
    auto pInvocation = this->GetConstInvocation();
    if (pInvocation) {
        if (this->begin.fileID != originPos.fileID) {
            // The original position and macrocall are not in the same file.
            return originPos;
        }
        // The original position and macrocall should be in the same file.
        return GetMacroSourcePos(*pInvocation, originPos, isLowerBound);
    }
    return originPos;
}

/**
 * Get the begin Position of the Node.
 * @return begin Position in macrocall file if the Node is expanded from macrocall,
 *  begin position in curfile otherwise.
 */
Position Node::GetBegin() const
{
    return this->GetMacroCallPos(this->begin);
}

/**
 * Get the end Position of the Node.
 * @return end Position in macrocall file if the Node is expanded from macrocall,
 *  end position in curfile otherwise.
 */
Position Node::GetEnd() const
{
    auto beginPos = this->GetMacroCallPos(this->begin);
    auto endPos = this->GetMacroCallPos(this->end, true);
    // The fileID of position may be different, may come from the macro definition or from the macrocall.
    if (beginPos.fileID != endPos.fileID) {
        endPos = beginPos + 1;
    }
    return endPos;
}

size_t NameReferenceExpr::OuterArgSize() const
{
    if (auto ce = DynamicCast<CallExpr*>(callOrPattern)) {
        return ce->args.size();
    } else if (auto pat = DynamicCast<EnumPattern*>(callOrPattern)) {
        return pat->patterns.size();
    }
    return 0;
}

/**
 * Get the field Position of the MemberAccess.
 * @return field Position in macrocall file if the MemberAccess is expanded from macrocall,
 *  field position in curfile otherwise.
 */
Position MemberAccess::GetFieldPos() const
{
    return this->GetMacroCallPos(this->field.Begin());
}

/**
 * Get the identifier Position of the RefExpr.
 * @return identifier Position in macrocall file if the RefExpr is expanded from macrocall,
 *  identifier position in curfile otherwise.
 */
Position RefExpr::GetIdentifierPos() const
{
    return this->GetMacroCallPos(this->ref.identifier.Begin());
}

/**
 * Get the field Position of the QualifiedType.
 * @return field Position in macrocall file if the QualifiedType is expanded from macrocall,
 *  field position in curfile otherwise.
 */
Position QualifiedType::GetFieldPos() const
{
    return this->GetMacroCallPos(this->field.Begin());
}

bool Decl::IsBuiltIn() const
{
    return astKind == ASTKind::BUILTIN_DECL;
}

/**
 * Get the identifier Position of the Decl.
 * @return identifier Position in macrocall file if the Decl is expanded from macrocall,
 *  identifier position in curfile otherwise.
 */
Position Decl::GetIdentifierPos() const
{
    return this->GetMacroCallPos(this->identifier.Begin());
}

Ptr<Generic> Decl::GetGeneric() const
{
    if (auto fd = DynamicCast<const FuncDecl*>(this); fd && fd->funcBody) {
        if (fd->funcBody->generic) {
            return fd->funcBody->generic.get();
        } else if (fd->funcBody->parentEnum != nullptr && fd->TestAttr(Attribute::ENUM_CONSTRUCTOR)) {
            return fd->funcBody->parentEnum->generic.get();
        }
        return nullptr;
    }
    if (auto vd = DynamicCast<const VarDecl*>(this);
        vd && vd->outerDecl && vd->outerDecl->astKind == ASTKind::ENUM_DECL) {
        return StaticAs<ASTKind::ENUM_DECL>(vd->outerDecl)->generic.get();
    }
    return generic.get();
}

bool Decl::IsExportedDecl() const
{
    if (TestAnyAttr(Attribute::PUBLIC, Attribute::PROTECTED)) {
        return true;
    }
    if (TestAttr(Attribute::INTERNAL)) {
        if (curFile && curFile->curPackage) {
            return !curFile->curPackage->noSubPkg;
        } else {
            return true;
        }
    }
    if (TestAttr(Attribute::PRIVATE)) {
        return false;
    }
    // When the decl is `extend A<B>`, B may be decl without modifiers such as GenericParamDecl, BuiltinDecl.
    // In this case, they must be exported for extend's extendType checking.
    return true;
}

bool Decl::IsConst() const
{
    if (auto vd = DynamicCast<const VarDeclAbstract*>(this); vd) {
        return vd->isConst;
    } else if (auto fd = DynamicCast<const FuncDecl*>(this); fd) {
        return fd->isConst;
    } else if (auto pcd = DynamicCast<const PrimaryCtorDecl*>(this); pcd) {
        return pcd->isConst;
    }
    return false;
}

Ptr<FuncDecl> Decl::GetDesugarDecl() const
{
    if (auto macroDecl = DynamicCast<const MacroDecl*>(this); macroDecl) {
        return macroDecl->desugarDecl.get();
    } else if (auto mainDecl = DynamicCast<const MainDecl*>(this); mainDecl) {
        return mainDecl->desugarDecl.get();
    } else if (auto funcParam = DynamicCast<const FuncParam*>(this); funcParam) {
        return funcParam->desugarDecl.get();
    }
    return nullptr;
}

bool Decl::IsCommonOrPlatform() const
{
    return TestAttr(AST::Attribute::COMMON) || TestAttr(AST::Attribute::PLATFORM);
}

bool Decl::IsCommonMatchedWithPlatform() const
{
    return TestAttr(AST::Attribute::COMMON) && platformImplementation;
}

/**
 * For debug, get the original Position of the node if it is from MacroCall in curfile, curPos otherwise.
 */
Position Node::GetDebugPos(const Position& curPos) const
{
    auto pInvocation = this->GetConstInvocation();
    if (!pInvocation) {
        // If the node is not macrocall node, then check whether it is expanded from macrocall node or not.
        if (curPos == INVALID_POSITION || !this->TestAttr(Attribute::MACRO_EXPANDED_NODE) || !this->curMacroCall) {
            return curPos;
        }
        // Current node is expanded from macrocall node.
        pInvocation = this->curMacroCall->GetConstInvocation();
        if (!pInvocation || pInvocation->macroDebugMap.empty()) {
            return curPos;
        }
    }
    auto key = static_cast<unsigned int>(curPos.column);
    if (pInvocation->macroDebugMap.lower_bound(key) == pInvocation->macroDebugMap.end()) {
        return curPos;
    }
    return pInvocation->macroDebugMap.lower_bound(key)->second;
}

const std::string& Node::GetFullPackageName() const
{
    if (auto decl = DynamicCast<Decl>(this); decl && !decl->fullPackageName.empty()) {
        return decl->fullPackageName;
    }
    if (curFile && curFile->curPackage) {
        return curFile->curPackage->fullPackageName;
    }
    return EMPTY_PACKAGE_NAME;
}

std::string PackageSpec::GetPackageName() const
{
    std::stringstream ss;
    for (size_t i{0}; i < prefixPaths.size(); ++i) {
        ss << prefixPaths[i];
        if (i == 0 && hasDoubleColon) {
            ss << TOKENS[static_cast<int>(TokenKind::DOUBLE_COLON)];
        } else {
            ss << TOKENS[static_cast<int>(TokenKind::DOT)];
        }
    }
    ss << packageName.Val();
    return ss.str();
}
 
std::string ImportContent::GetPrefixPath() const
{
    std::stringstream ss;
    for (size_t i{0}; i < prefixPaths.size(); ++i) {
        ss << prefixPaths[i];
        if (i == prefixPaths.size() - 1) {
            break;
        }
        if (i == 0 && hasDoubleColon) {
            // valid import do not end with ::
            ss << TOKENS[static_cast<int>(TokenKind::DOUBLE_COLON)];
        } else {
            ss << TOKENS[static_cast<int>(TokenKind::DOT)];
        }
    }
    return ss.str();
}
 
std::string ImportContent::GetImportedPackageName() const
{
    std::stringstream ss;
    for (size_t i{0}; i < prefixPaths.size(); ++i) {
        ss << prefixPaths[i];
        // do not add . if this is the last of import xxx.*, because * is not part of package name
        if (kind == ImportKind::IMPORT_ALL && i + 1 == prefixPaths.size()) {
            continue;
        }
        if (i == 0 && hasDoubleColon) {
            ss << TOKENS[static_cast<int>(TokenKind::DOUBLE_COLON)];
        } else {
            ss << TOKENS[static_cast<int>(TokenKind::DOT)];
        }
    }
    if (kind != ImportKind::IMPORT_ALL) {
        ss << identifier.Val();
    }
    return ss.str();
}

std::string ImportContent::ToString() const
{
    std::function<void(std::stringstream&, const ImportContent&)> toString = [](auto& ss, auto& content) {
        for (const auto& prefix : content.prefixPaths) {
            ss << prefix << ".";
        }
        if (content.kind != ImportKind::IMPORT_MULTI) {
            ss << content.identifier.Val();
            if (content.kind == ImportKind::IMPORT_ALIAS) {
                ss << " as " << content.aliasName.Val();
            }
        }
        return ss.str();
    };
    std::stringstream ss;
    toString(ss, *this);
    if (kind != ImportKind::IMPORT_MULTI) {
        return ss.str();
    }
    ss << "{" << std::endl;
    for (const auto& item : items) {
        ss << "    ";
        toString(ss, item);
        ss << std::endl;
    }
    ss << "}";
    return ss.str();
}

std::string FeatureId::ToString() const
{
    std::stringstream ss;
    size_t idx = 0;
    for (const auto& ident : this->identifiers) {
        ss << ident.Val();
        if (idx < dotPoses.size()) {
            ss << ".";
            idx++;
        }
    }
    return ss.str();
}

bool ExtendDecl::IsExportedDecl() const
{
    // ExtendedType Check (Direct and Interface Extensions): If B in extend A<B> isn't exported, the extendDecl should
    // not be exported. For imported decl, extendedType may be nullptr and not ready.
    if (extendedType != nullptr) {
        for (auto& it : extendedType->GetTypeArgs()) {
            if (it && it->GetTarget() && !it->GetTarget()->IsExportedDecl()) {
                return false;
            }
        }
    }
    auto extendedDecl = Ty::GetDeclPtrOfTy<InheritableDecl>(ty);
    bool isInSamePkg = extendedDecl && extendedDecl->fullPackageName == fullPackageName;
    auto isUpperBoundExport = [this]() {
        bool isUpperboundAllExported = true;
        for (auto& tp : generic->genericConstraints) {
            CJC_NULLPTR_CHECK(tp);
            for (auto& up : tp->upperBounds) {
                CJC_NULLPTR_CHECK(up);
                if (up->GetTarget() == nullptr) {
                    continue;
                }
                isUpperboundAllExported = up->GetTarget()->IsExportedDecl() && isUpperboundAllExported;
            }
        }
        return isUpperboundAllExported;
    };
    // Direct Extensions Check:
    if (inheritedTypes.empty()) {
        // Rule 1: In `package std.core`, direct extensions of types visible outside the package are exported.
        if (fullPackageName == "std.core") {
            return true;
        }
        if (isInSamePkg) {
            // Rule 2: When direct extensions are defined in the same `package` as the extended type, whether the
            // extension is exported is determined by the lowest access level of the type used in the extended type and
            // the generic constraints (if any).
            if (!generic) {
                return extendedDecl->IsExportedDecl();
            }
            return extendedDecl->IsExportedDecl() && isUpperBoundExport();
        } else {
            // Rule 3: When the direct extension is in a different `package` from the declaration of the type being
            // extended, the extension is never exported and can only be used in the current `package`.
            return false;
        }
    }
    // Interface Extensions Check:
    if (isInSamePkg) {
        // Rule 1: When the interface extension and the extended type are in the same `package`, the extension is
        // exported together with the extended type and is not affected by the access level of the interface type.
        return extendedDecl->IsExportedDecl();
    }
    // Rule 2: When an interface extension is in a different `package` from the type being extended, whether the
    // interface extension is exported is determined by the smallest access level of the type used in the
    // interface type and the generic constraints (if any).
    bool isInterfaceAllExported = false;
    for (auto& inhertType : inheritedTypes) {
        if (inhertType->GetTarget() == nullptr) {
            continue;
        }
        if (inhertType->GetTarget()->IsExportedDecl()) {
            isInterfaceAllExported = true;
            break;
        }
    }
    if (!generic) {
        return isInterfaceAllExported;
    }
    return isInterfaceAllExported && isUpperBoundExport();
}

bool PropDecl::IsExportedDecl() const
{
    if (!outerDecl || outerDecl->astKind != ASTKind::EXTEND_DECL) {
        return Decl::IsExportedDecl();
    }
    auto extend = StaticCast<ExtendDecl>(outerDecl);
    auto extendedDecl = Ty::GetDeclPtrOfTy(extend->ty);
    // If extend and extended decleration in same package, all member of extend will be exported.
    if (!extendedDecl || extendedDecl->fullPackageName == extend->fullPackageName) {
        return Decl::IsExportedDecl();
    }
    // If extend is direct extension, all member will be not exported.
    if (extend->inheritedTypes.empty()) {
        return false;
    }
    return Decl::IsExportedDecl() && TestAttr(Attribute::INTERFACE_IMPL);
}

bool FuncDecl::IsExportedDecl() const
{
    if (!outerDecl || outerDecl->astKind != ASTKind::EXTEND_DECL) {
        return Decl::IsExportedDecl();
    }
    auto extend = StaticCast<ExtendDecl>(outerDecl);
    auto extendedDecl = Ty::GetDeclPtrOfTy(extend->ty);
    // If extend and extended decleration in same package, all member of extend will be exported.
    if (!extendedDecl || extendedDecl->fullPackageName == extend->fullPackageName) {
        return Decl::IsExportedDecl();
    }
    // If extend is direct extension, all member will be not exported.
    if (extend->inheritedTypes.empty()) {
        return false;
    }
    return Decl::IsExportedDecl() && TestAttr(Attribute::INTERFACE_IMPL);
}

bool FuncDecl::IsOpen() const noexcept
{
    if (!outerDecl || !outerDecl->IsOpen() || TestAttr(Attribute::STATIC)) {
        return false;
    }
    if (TestAnyAttr(Attribute::OPEN, Attribute::ABSTRACT)) {
        return true;
    }
    return !TestAttr(AST::Attribute::IMPORTED) && !funcBody->body;
}

bool PropDecl::IsOpen() const noexcept
{
    if (!outerDecl || !outerDecl->IsOpen() || TestAttr(Attribute::STATIC)) {
        return false;
    }
    if (TestAnyAttr(Attribute::OPEN, Attribute::ABSTRACT)) {
        return true;
    }
    return !TestAttr(AST::Attribute::IMPORTED) && getters.empty() && setters.empty();
}


} // namespace Cangjie
