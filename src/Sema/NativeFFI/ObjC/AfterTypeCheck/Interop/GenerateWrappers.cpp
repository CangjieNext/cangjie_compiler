// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file generates toplevel C wrappers for Objective-C impls members (except constructors).
 */

#include "Handlers.h"
#include "NativeFFI/ObjC/Utils/ASTQuery.h"
#include "cangjie/AST/Match.h"

namespace Cangjie::Interop::ObjC {
using namespace Cangjie::AST;

void GenerateWrappers::HandleImpl(InteropContext& ctx)
{
    for (auto& impl : ctx.impls) {
        if (impl->TestAttr(Attribute::IS_BROKEN)) {
            continue;
        }

        for (auto& memberDecl : impl->GetMemberDecls()) {
            if (memberDecl->TestAnyAttr(Attribute::IS_BROKEN, Attribute::CONSTRUCTOR)) {
                continue;
            }
            if (!memberDecl->TestAnyAttr(Attribute::PUBLIC)) {
                continue;
            }

            if (IsGeneratedMember(*memberDecl)) {
                continue;
            }

            switch (memberDecl->astKind) {
                case ASTKind::FUNC_DECL:
                    GenerateWrapper(ctx, *impl, *StaticAs<ASTKind::FUNC_DECL>(memberDecl.get()));
                    break;
                case ASTKind::PROP_DECL:
                    GenerateWrapper(ctx, *impl, *StaticAs<ASTKind::PROP_DECL>(memberDecl.get()));
                    break;
                case ASTKind::VAR_DECL:
                    GenerateWrapper(ctx, *impl, *StaticAs<ASTKind::VAR_DECL>(memberDecl.get()));
                    break;
                default:
                    break;
            }
        }
    }
}

void GenerateWrappers::GenerateWrapper(InteropContext& ctx, ClassDecl& impl, FuncDecl& method)
{
    auto wrapper = ctx.factory.CreateMethodWrapper(method, impl);
    CJC_NULLPTR_CHECK(wrapper);
    ctx.genDecls.push_back(std::move(wrapper));
}

void GenerateWrappers::GenerateWrapper(InteropContext& ctx, ClassDecl& impl, PropDecl& prop)
{
    auto wrapper = ctx.factory.CreateGetterWrapper(prop, impl);
    CJC_NULLPTR_CHECK(wrapper);
    ctx.genDecls.push_back(std::move(wrapper));

    if (prop.isVar) {
        GenerateSetterWrapper(ctx, impl, prop);
    }
}

void GenerateWrappers::GenerateSetterWrapper(InteropContext& ctx, ClassDecl& impl, PropDecl& prop)
{
    auto wrapper = ctx.factory.CreateSetterWrapper(prop, impl);
    CJC_NULLPTR_CHECK(wrapper);
    ctx.genDecls.push_back(std::move(wrapper));
}

void GenerateWrappers::GenerateWrapper(InteropContext& ctx, ClassDecl& impl, VarDecl& field)
{
    auto wrapper = ctx.factory.CreateGetterWrapper(field, impl);
    CJC_NULLPTR_CHECK(wrapper);
    ctx.genDecls.push_back(std::move(wrapper));

    if (field.isVar) {
        GenerateSetterWrapper(ctx, impl, field);
    }
}

void GenerateWrappers::GenerateSetterWrapper(InteropContext& ctx, ClassDecl& impl, VarDecl& field)
{
    auto wrapper = ctx.factory.CreateSetterWrapper(field, impl);
    CJC_NULLPTR_CHECK(wrapper);
    ctx.genDecls.push_back(std::move(wrapper));
}
} // namespace Cangjie::Interop::ObjC
