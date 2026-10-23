// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * CJMP helpers shared across Sema / Frontend / CHIR.
 */

#ifndef CANGJIE_SEMA_CJMPUTILS_H
#define CANGJIE_SEMA_CJMPUTILS_H

#include "cangjie/AST/Node.h"

namespace Cangjie::Sema {
/// Build Package::canonicalCommonDecls (winners for CHIR ShouldAdopt).
/// Invoked after mangling (before CJO save / CHIR).
void BuildCanonicalCommonDeclMap(AST::Package& pkg);
} // namespace Cangjie::Sema

#endif
