// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements imports parsing.
 */

#include "ParserImpl.h"

#include <functional>

#include "cangjie/AST/Clone.h"
#include "cangjie/AST/Match.h"
#include "cangjie/Utils/Utils.h"

using namespace Cangjie;
using namespace Cangjie::AST;
using namespace Cangjie::Utils;

void ParserImpl::ParseMacroImports(AST::PackageSpec& packageSpec)
{
    // Each macro import statement (single form or one block) becomes its own
    // MacroImportList in packageSpec.macroImports, so positions never collide.
    auto list = MakeOwned<AST::MacroImportList>();
    list->macroPos = Peek().Begin();
    list->begin = list->macroPos;
    AST::MacroImportList* listPtr = list.get();
    packageSpec.macroImports.push_back(std::move(list));
    if (Seeing({TokenKind::MACRO, TokenKind::LCURL})) {
        ParseMacroBlockImport(packageSpec, listPtr);
    } else {
        ParseMacroSingleImport(packageSpec, listPtr);
    }
}

void ParserImpl::ParseMacroSingleImport(AST::PackageSpec& packageSpec, AST::MacroImportList* list)
{
    // macroSingleImport := 'macro' 'import' importContent
    // Syntax rule 5: @When may not prefix a single macro import (checked by the caller).
    Skip(TokenKind::MACRO);
    auto spec = MakeOwned<AST::MacroImportSpec>();
    spec->importPos = Peek().Begin();
    Skip(TokenKind::IMPORT);
    ParseImportContent(spec->content);
    // annotations stays empty — single form forbids @When (syntax rule 5).
    spec->begin = spec->importPos;
    spec->end = spec->content.end;
    list->lcurlPos = INVALID_POSITION;
    list->rcurlPos = INVALID_POSITION;
    list->end = spec->end;
    DeriveMacroImportPkgNames(spec->content, packageSpec);
    list->items.push_back(std::move(spec));

    if (!newlineSkipped) {
        if (Seeing(TokenKind::END)) {
            return;
        }
        if (!Skip(TokenKind::SEMI)) {
            DiagExpectSemiOrNewline();
            ConsumeUntilAny({TokenKind::NL, TokenKind::SEMI});
        }
    }
    SkipBlank(TokenKind::SEMI);
}

void ParserImpl::ParseMacroBlockImport(AST::PackageSpec& packageSpec, AST::MacroImportList* list)
{
    // macroBlockImport := 'macro' '{' macroBlockItem* '}'
    // macroBlockItem  := NewLine* (whenAnnotation)? 'import' importContent
    Skip(TokenKind::MACRO);
    list->lcurlPos = Peek().Begin();
    Skip(TokenKind::LCURL);

    while (true) {
        SkipBlank(TokenKind::NL); // allow blank lines / newlines between items
        if (Seeing(TokenKind::RCURL) || Seeing(TokenKind::END)) {
            break;
        }

        auto spec = MakeOwned<AST::MacroImportSpec>();
        // Syntax rule 3: a block item may carry a leading @When (kept as an
        // Annotation node, evaluated downstream like ordinary ImportSpec).
        if (SeeingAtWhen()) {
            spec->annotations.push_back(ParseAnnotation()); // anno->kind == AnnotationKind::WHEN
        }

        spec->importPos = Peek().Begin();
        if (!Skip(TokenKind::IMPORT)) {
            DiagExpectCharacter(lastToken.End(), "'import'");
            ConsumeUntilAny({TokenKind::NL, TokenKind::SEMI, TokenKind::RCURL});
            continue;
        }
        ParseImportContent(spec->content);
        spec->begin = spec->annotations.empty() ? spec->importPos : spec->annotations.front()->begin;
        spec->end = spec->content.end;
        DeriveMacroImportPkgNames(spec->content, packageSpec);
        list->items.push_back(std::move(spec));

        if (!newlineSkipped) {
            if (Seeing(TokenKind::RCURL) || Seeing(TokenKind::END)) {
                break;
            }
            if (!Skip(TokenKind::SEMI)) {
                DiagExpectSemiOrNewline();
                ConsumeUntilAny({TokenKind::NL, TokenKind::SEMI, TokenKind::RCURL});
            }
        }
        SkipBlank(TokenKind::SEMI);
    }
    list->rcurlPos = Peek().Begin();
    list->end = list->rcurlPos;
    Skip(TokenKind::RCURL);
}

void ParserImpl::DeriveMacroImportPkgNames(const AST::ImportContent& content, AST::PackageSpec& packageSpec)
{
    // Derive the package prefix for the cjpm dependency graph (not persisted to cjo).
    if (content.kind == ImportKind::IMPORT_MULTI) {
        // `a.b.{c, d}`: per-item names are symbols, not packages — pushing them
        // would create phantom cjpm dependencies on "c"/"d".
        auto prefix = content.GetPrefixPath();
        if (!prefix.empty()) {
            packageSpec.macroImportPkgNames.push_back(prefix);
        }
    } else if (content.kind == ImportKind::IMPORT_ALL) {
        // prefix.* — package name is the prefix.
        auto prefix = content.GetPrefixPath();
        if (!prefix.empty()) {
            packageSpec.macroImportPkgNames.push_back(prefix);
        }
    } else if (content.kind == ImportKind::IMPORT_SINGLE || content.kind == ImportKind::IMPORT_ALIAS) {
        // Alias form keeps the original name in `identifier`, so the package
        // prefix plus the imported symbol still identifies the target package.
        std::string pkgName = content.GetImportedPackageName();
        if (!pkgName.empty()) {
            packageSpec.macroImportPkgNames.push_back(std::move(pkgName));
        }
    }
}

void ParserImpl::ParseCommonImportSpec(PtrVector<ImportSpec>& imports, PtrVector<Annotation>& annos,
    PackageSpec* packageSpec)
{
    auto seeingMacroSingle = [&]() -> bool {
        // Only the bare 'macro' 'import' lookahead is recognized; a modifier prefix is
        // handled by seeingIllegalModifierMacroImport (no re-export allowed).
        if (!packageSpec || !packageSpec->hasMacro) {
            return false;
        }
        return Seeing({TokenKind::MACRO, TokenKind::IMPORT});
    };
    auto seeingMacroBlock = [&]() -> bool {
        if (!packageSpec || !packageSpec->hasMacro) {
            return false;
        }
        return Seeing({TokenKind::MACRO, TokenKind::LCURL});
    };
    auto seeingIllegalMacroImport = [&]() -> bool {
        // Recognize macro import syntax in a non-macro package to report the dedicated
        // diagnostic instead of a generic parse error.
        if (packageSpec && packageSpec->hasMacro) {
            return false;
        }
        return Seeing({TokenKind::MACRO, TokenKind::IMPORT}) || Seeing({TokenKind::MACRO, TokenKind::LCURL});
    };
    auto seeingIllegalModifierMacroImport = [&]() -> bool {
        // Syntax rule 4: no import modifier prefix (no re-export), in any package.
        static const std::vector<TokenKind> IMPORT_MODIFIERS = {TokenKind::PUBLIC, TokenKind::PROTECTED,
            TokenKind::INTERNAL, TokenKind::PRIVATE};
        if (!SeeingAny(IMPORT_MODIFIERS)) {
            return false;
        }
        for (auto modifier : IMPORT_MODIFIERS) {
            if (Seeing({modifier, TokenKind::MACRO, TokenKind::IMPORT}) ||
                Seeing({modifier, TokenKind::MACRO, TokenKind::LCURL})) {
                return true;
            }
        }
        return false;
    };

    while (SeeingImport() || SeeingBuiltinAnnotation() || seeingMacroSingle() || seeingMacroBlock() ||
        seeingIllegalMacroImport() || seeingIllegalModifierMacroImport()) {
        if (SeeingBuiltinAnnotation()) {
            ParseAnnotations(annos);
        }
        if (seeingMacroSingle()) {
            // Syntax rule 5: @When may not prefix a single macro import.
            for (const auto& anno : annos) {
                if (anno->kind == AnnotationKind::WHEN) {
                    ParseDiagnoseRefactor(DiagKindRefactor::macro_import_when_only_in_block, anno->begin);
                    break;
                }
            }
            annos.clear();
            ParseMacroImports(*packageSpec);
            continue;
        }
        if (seeingMacroBlock()) {
            annos.clear();
            ParseMacroImports(*packageSpec);
            continue;
        }
        if (seeingIllegalMacroImport()) {
            ParseDiagnoseRefactor(DiagKindRefactor::macro_import_in_non_macro_package, Peek().Begin());
            ConsumeUntilAny({TokenKind::NL, TokenKind::SEMI, TokenKind::END});
            annos.clear();
            continue;
        }
        if (seeingIllegalModifierMacroImport()) {
            if (!packageSpec || !packageSpec->hasMacro) {
                ParseDiagnoseRefactor(DiagKindRefactor::macro_import_in_non_macro_package, Peek().Begin());
            } else {
                ParseDiagnoseRefactor(DiagKindRefactor::macro_import_no_modifier_allowed, Peek().Begin());
            }
            ConsumeUntilAny({TokenKind::NL, TokenKind::SEMI, TokenKind::END});
            annos.clear();
            continue;
        }
        if (!SeeingImport()) {
            break;
        }
        ParseImportSpec(imports, annos);
        annos.clear();
        if (!newlineSkipped) {
            if (Seeing(TokenKind::END)) {
                break;
            }
            if (!Skip(TokenKind::SEMI)) {
                DiagExpectSemiOrNewline();
                ConsumeUntilAny({TokenKind::NL, TokenKind::SEMI});
            }
        }
        SkipBlank(TokenKind::SEMI);
    }
}

void ParserImpl::CheckImportSpec(PtrVector<ImportSpec>& imports)
{
    for (auto& import : imports) {
        if (import->TestAttr(Attribute::COMPILER_ADD)) {
            continue;
        }

        const auto& content = import->content;
        if (content.kind != ImportKind::IMPORT_MULTI) {
            auto fullPackageName = content.GetImportedPackageName();
            if (!content.TestAttr(Attribute::IS_BROKEN) && fullPackageName.size() > PACKAGE_NAME_LEN_LIMIT) {
                auto packageNameBeginPos =
                    content.prefixPaths.empty() ? content.identifier.Begin() : content.prefixPoses[0];
                DiagNameLengthOverflow(MakeRange(packageNameBeginPos, content.end), fullPackageName,
                    PACKAGE_NAME_LEN_LIMIT, fullPackageName.size());
            }
            continue;
        }
        auto commonPrefix = content.GetImportedPackageName();
        for (const auto& item : content.items) {
            auto suffix = item.GetImportedPackageName();
            auto fullPackageName = commonPrefix.empty() ? suffix : commonPrefix + "." + suffix;
            if (!item.TestAttr(Attribute::IS_BROKEN) && fullPackageName.size() > PACKAGE_NAME_LEN_LIMIT) {
                auto packageNameBeginPos = content.prefixPaths.empty()
                    ? item.prefixPaths.empty() ? item.identifier.Begin() : item.prefixPoses[0]
                    : content.prefixPoses[0];
                DiagNameLengthOverflow(MakeRange(packageNameBeginPos, content.end), fullPackageName,
                    PACKAGE_NAME_LEN_LIMIT, fullPackageName.size());
            }
        }
    }
}

void ParserImpl::ParseImportSpecInTop(PtrVector<ImportSpec>& imports, PtrVector<Annotation>& annos,
    PackageSpec* packageSpec)
{
    ParseCommonImportSpec(imports, annos, packageSpec);
    CheckImportSpec(imports);
}

void ParserImpl::ParseImportSpec(PtrVector<ImportSpec>& imports, const PtrVector<Annotation>& annos)
{
    auto importSpec = MakeOwned<ImportSpec>();
    if (SeeingAny({TokenKind::PUBLIC, TokenKind::PROTECTED, TokenKind::INTERNAL, TokenKind::PRIVATE})) {
        Next();
        auto modifier = MakeOwned<Modifier>(lastToken.kind, lastToken.Begin());
        modifier->end = lastToken.End();
        importSpec->modifier = std::move(modifier);
    }
    Skip(TokenKind::IMPORT);
    importSpec->EnableAttr(GetModifierAttr(importSpec->modifier, ASTKind::IMPORT_SPEC));
    importSpec->begin = importSpec->modifier ? importSpec->modifier->begin : lastToken.Begin();
    importSpec->importPos = lastToken.Begin();
    ParseImportContent(importSpec->content);
    importSpec->end = importSpec->content.end;

    // Check allowed annotation on import.
    std::for_each(annos.begin(), annos.end(), [&, this](const Ptr<Annotation>& anno) {
        if (NotIn(anno->kind, {AnnotationKind::WHEN})) {
            DiagUnexpectedAnnoOn(*anno, importSpec->importPos, anno->identifier, "import");
        }
    });

    importSpec->annotations = ASTCloner::CloneVector(annos);

    if (auto& import = imports.emplace_back(std::move(importSpec)); import->IsImportMulti()) {
        DesugarImportMulti(imports, *import);
    }
}

void ParserImpl::DesugarImportMulti(PtrVector<ImportSpec>& imports, ImportSpec& import) const
{
    CJC_ASSERT(import.IsImportMulti());
    // For lsp auto completion, desugar as much as possible
    const auto& items = import.content.items;
    if (std::find_if(items.begin(), items.end(),
            [](auto& it) { return it.TestAnyAttr(Attribute::IS_BROKEN, Attribute::HAS_BROKEN); }) != items.end()) {
        import.EnableAttr(Attribute::HAS_BROKEN);
    }
    for (auto& item : items) {
        auto desugaredImport = MakeOwned<ImportSpec>();
        CopyBasicInfo(&item, desugaredImport.get());

        // Copy modifier
        if (import.modifier) {
            desugaredImport->modifier = MakeOwned<Modifier>(import.modifier->modifier, import.modifier->begin);
            CopyBasicInfo(import.modifier.get(), desugaredImport->modifier.get());
            desugaredImport->modifier->isExplicit = import.modifier->isExplicit;
        }

        desugaredImport->importPos = import.importPos;
        desugaredImport->annotations = ASTCloner::CloneVector(import.annotations);

        CopyBasicInfo(&import.content, &desugaredImport->content);
        desugaredImport->content.end = item.end;
        desugaredImport->content.kind = item.kind;
        desugaredImport->content.prefixPaths = import.content.prefixPaths;
        desugaredImport->content.prefixPoses = import.content.prefixPoses;
        desugaredImport->content.prefixDotPoses = import.content.prefixDotPoses;
        desugaredImport->content.hasDoubleColon = import.content.hasDoubleColon;

        desugaredImport->content.prefixPaths.insert(
            desugaredImport->content.prefixPaths.end(), item.prefixPaths.begin(), item.prefixPaths.end());
        desugaredImport->content.prefixPoses.insert(
            desugaredImport->content.prefixPoses.end(), item.prefixPoses.begin(), item.prefixPoses.end());
        desugaredImport->content.prefixDotPoses.insert(
            desugaredImport->content.prefixDotPoses.end(), item.prefixDotPoses.begin(), item.prefixDotPoses.end());

        desugaredImport->content.identifier = item.identifier;
        desugaredImport->content.asPos = item.asPos;
        desugaredImport->content.aliasName = item.aliasName;

        desugaredImport->EnableAttr(Attribute::COMPILER_ADD, GetModifierAttr(import.modifier, ASTKind::IMPORT_SPEC));
        imports.emplace_back(std::move(desugaredImport));
    }
}

void ParserImpl::ParseImportContent(ImportContent& content)
{
    if (!ParseImportSingle(content)) {
        ParseImportMulti(content);
    }

    // Parse import-alias
    if (content.kind == ImportKind::IMPORT_SINGLE && Skip(TokenKind::AS)) {
        ParseImportAliasPart(content);
    }
}

bool ParserImpl::ParseImportSingle(ImportContent& content, bool inMultiImport)
{
    SrcIdentifier curIdent;
    bool firstIter{true};
    bool skipDc{false};
    do {
        skipDc = false;
        if (!curIdent.Empty()) {
            content.prefixPaths.emplace_back(curIdent.Val());
            content.prefixPoses.emplace_back(curIdent.Begin());
            CJC_ASSERT(!curIdent.Begin().IsZero());
            content.prefixDotPoses.emplace_back(lastToken.Begin());
        }

        if (!inMultiImport && Skip(TokenKind::LCURL)) {
            if (firstIter) {
                content.begin = lastToken.Begin();
            }
            content.leftCurlPos = lastToken.Begin();
            return false;
        }

        // Parse import-all
        if (!firstIter && Skip(TokenKind::MUL)) {
            curIdent = "*";
            curIdent.SetPos(lastToken.Begin(), lastToken.End());
            break;
        }

        // Parse import-single
        curIdent = ExpectPackageIdentWithPos(content);
        if (curIdent == INVALID_IDENTIFIER) {
            if (inMultiImport) {
                ConsumeUntilAny({TokenKind::NL, TokenKind::COMMA, TokenKind::RCURL});
                if (lastToken.kind == TokenKind::RCURL) {
                    content.rightCurlPos = lastToken.Begin();
                }
            } else {
                ConsumeUntilAny({TokenKind::NL, TokenKind::COMMA});
            }
            content.EnableAttr(Attribute::IS_BROKEN);
            break;
        }
        if (firstIter) {
            content.begin = curIdent.Begin();
            firstIter = false;
        }
        if (content.prefixPaths.empty() && !inMultiImport && Skip(TokenKind::DOUBLE_COLON)) {
            content.hasDoubleColon = true;
            skipDc = true;
            // illegal to use raw identifier as org name
            if (curIdent.IsRaw() && !content.TestAttr(Attribute::IS_BROKEN)) {
                ParseDiagnoseRefactor(DiagKindRefactor::parse_package_name_has_backtick,
                    MakeRange(curIdent.Begin() - 1, curIdent.End() + 1));
            }
        }
    } while (skipDc || Skip(TokenKind::DOT));

    content.kind = curIdent == "*" ? ImportKind::IMPORT_ALL : ImportKind::IMPORT_SINGLE;

    if (lastToken.kind == TokenKind::PACKAGE_IDENTIFIER) {
        DiagExpectedIdentifier(MakeRange(curIdent.Begin(), curIdent.End()));
        content.EnableAttr(Attribute::IS_BROKEN);
    }
    content.identifier = std::move(curIdent);
    content.end = curIdent.End();

    return true;
}

void ParserImpl::ParseImportMulti(ImportContent& content)
{
    content.kind = ImportKind::IMPORT_MULTI;

    bool firstIter{true};
    do {
        if (!firstIter) {
            bool skippedComma = Skip(TokenKind::COMMA);
            if (skippedComma) {
                content.commaPoses.emplace_back(lastToken.Begin());
            }
            // Import-multi should contain at least one item.
            if (Skip(TokenKind::RCURL)) {
                content.rightCurlPos = lastToken.Begin();
                content.end = lastToken.End();
                return;
            }
            if (!skippedComma) {
                DiagExpectCharacter(lastToken.End(), "','");
                content.EnableAttr(Attribute::IS_BROKEN);
            }
        }
        firstIter = false;

        ImportContent item;
        ParseImportSingle(item, true);
        if (item.kind == ImportKind::IMPORT_SINGLE && Skip(TokenKind::AS)) {
            ParseImportAliasPart(item);
        }
        if (item.TestAttr(Attribute::IS_BROKEN)) {
            content.EnableAttr(Attribute::HAS_BROKEN);
        }
        content.items.emplace_back(std::move(item));
    } while (SeeingAny({TokenKind::COMMA, TokenKind::IDENTIFIER, TokenKind::PACKAGE_IDENTIFIER, TokenKind::AS}));

    if (lastToken.kind != TokenKind::RCURL && !Skip(TokenKind::RCURL)) {
        DiagExpectedRightDelimiter("{", content.leftCurlPos);
        content.EnableAttr(Attribute::IS_BROKEN);
        content.end = lastToken.End();
    } else {
        content.rightCurlPos = lastToken.Begin();
        auto lastItem = content.items.rbegin();
        CJC_ASSERT(lastItem != content.items.rend());
        content.end = lastItem->end;
    }
}

void ParserImpl::ParseImportAliasPart(ImportContent& content)
{
    content.kind = ImportKind::IMPORT_ALIAS;
    content.asPos = lastToken.Begin();
    auto curIdent = ExpectIdentifierWithPos(content);
    if (curIdent == INVALID_IDENTIFIER) {
        ConsumeUntilAny({TokenKind::NL, TokenKind::COMMA});
        content.EnableAttr(Attribute::IS_BROKEN);
    } else {
        content.aliasName = std::move(curIdent);
        content.end = lastToken.End();
    }
}
