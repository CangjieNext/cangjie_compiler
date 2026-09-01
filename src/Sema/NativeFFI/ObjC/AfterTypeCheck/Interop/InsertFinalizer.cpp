// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements generating and inserting a finalizer for each @ObjCMirror and @ObjCImpl class.
 */

#include "Handlers.h"
#include <cstdint>

namespace Cangjie::Interop::ObjC {
using namespace Cangjie::AST;
using namespace Cangjie::Native::FFI;

namespace {
enum class NeedsHasInitedField : uint8_t { NO, YES };

void CreateAndInsertFinalizer(
    DeprecatedASTFactory& deprecatedFactory, ClassDecl& cd, NeedsHasInitedField needsHasInitedField)
{
    if (needsHasInitedField == NeedsHasInitedField::YES) {
        auto hasInitedField = deprecatedFactory.CreateHasInitedField(cd);
        cd.body->decls.push_back(std::move(hasInitedField));
    }

    auto finalizer = deprecatedFactory.CreateFinalizer(cd);
    cd.body->decls.push_back(std::move(finalizer));
}
} // namespace

void InsertFinalizer::HandleImpl(InteropContext& ctx)
{
    for (auto& mirror : ctx.mirrors) {
        if (mirror->TestAttr(Attribute::IS_BROKEN)) {
            continue;
        }
        auto mirrorClass = As<ASTKind::CLASS_DECL>(mirror);
        if (!mirrorClass) {
            continue;
        }

        // Actually, only @ObjCMirror open class needs `$hasInited: Bool` field, but CHIR checks only
        // OBJ_C_MIRROR attribute and not class openness, so we have to insert it for
        // each @ObjCMirror class
        CreateAndInsertFinalizer(ctx.deprecatedFactory, *mirrorClass, NeedsHasInitedField::YES);
    }

    for (auto& wrapper : ctx.mirrorInterfaceHandleWrappers) {
        CreateAndInsertFinalizer(ctx.deprecatedFactory, *wrapper, NeedsHasInitedField::NO);
    }

    for (auto& impl : ctx.impls) {
        if (impl->TestAttr(Attribute::IS_BROKEN)) {
            continue;
        }

        // Actually, only @ObjCImpl open class needs `$hasInited: Bool` field, but CHIR checks only
        // OBJ_C_IMPL attribute and not class openness, so we have to insert it for
        // each @ObjCImpl class
        CreateAndInsertFinalizer(ctx.deprecatedFactory, *impl, NeedsHasInitedField::YES);
    }
}
} // namespace Cangjie::Interop::ObjC