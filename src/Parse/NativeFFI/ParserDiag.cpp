// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file combines diagnostics reports related to FFI parser.
 */

#include "../ParserImpl.h"
#include "FFIParserImpl.h"
#include "cangjie/AST/Node.h"
#include "cangjie/Basic/DiagnosticEngine.h"
#include "cangjie/Lex/Token.h"
#include "cangjie/Utils/CheckUtils.h"

namespace Cangjie {
using namespace AST;

static const std::string at = "@";
namespace {
std::string GetDeprecatedAnnotationReplacement(const Annotation& anno)
{
    static const auto foreign = TOKEN_KIND_VALUES[static_cast<int>(TokenKind::FOREIGN)];
    static const auto java = at + ANNO_KIND_TO_NAME.at(AnnotationKind::JAVA);
    static const auto javaForeign = java + " " + foreign;
    static const auto objc = at + ANNO_KIND_TO_NAME.at(AnnotationKind::OBJ_C);
    static const auto objcForeign = objc + " " + foreign;

    static const auto replacements = std::map<AnnotationKind, const std::string>{
        {AnnotationKind::JAVA_MIRROR, javaForeign},
        {AnnotationKind::JAVA_IMPL, java},
        {AnnotationKind::OBJ_C_MIRROR, objcForeign},
        {AnnotationKind::OBJ_C_IMPL, objc}
    };

    auto replacement = replacements.find(anno.kind);
    CJC_ASSERT(replacement != replacements.end());
    return replacement->second;
}
} // namespace

void FFIParserImpl::DiagDeprecatedAnnotation(const Annotation& anno) const
{
    p.ParseDiagnoseRefactor(DiagKindRefactor::parse_deprecated_interop_annotation, anno,
        at + anno.identifier,
        GetDeprecatedAnnotationReplacement(anno));
}

} // namespace Cangjie
