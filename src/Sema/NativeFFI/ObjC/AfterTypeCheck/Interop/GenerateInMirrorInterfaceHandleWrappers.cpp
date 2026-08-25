// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements generate in mirror interface handle wrappers.
 */

#include "Handlers.h"
#include "NativeFFI/ObjC/Utils/ASTQuery.h"
#include "NativeFFI/Utils.h"
#include "cangjie/AST/Create.h"
#include "cangjie/AST/Match.h"
#include "cangjie/AST/Node.h"
#include "cangjie/Utils/CheckUtils.h"

namespace Cangjie::Interop::ObjC {

using namespace Cangjie::AST;
using namespace Cangjie::Native::FFI;

namespace {
/**
 * @brief Generates a function stub based on an existing function declaration.
 *
 * This function creates a clone of the provided function declaration (fd),
 * replaces its outerDecl to handle wrapper class, and then inserts the
 * modified function declaration into the specified synthetic class declaration.
 *
 * @param synthetic The class declaration where the cloned function stub will be inserted.
 * @param fd The original function declaration that will be cloned and modified.
 */
void GenerateHandleWrapperClassFuncStub(ClassDecl& handleWrapper, FuncDecl& fd)
{
    auto funcStub = ASTCloner::Clone(Ptr(&fd));

    // remove foreign anno from cloned func decl
    for (auto it = funcStub->annotations.begin(); it != funcStub->annotations.end(); ++it) {
        if ((*it)->kind == AnnotationKind::FOREIGN_NAME) {
            funcStub->annotations.erase(it);
            break;
        }
    }

    funcStub->outerDecl = Ptr(&handleWrapper);
    handleWrapper.body->decls.push_back(std::move(funcStub));
}

void GenerateHandleWrapperClassPropStub(ClassDecl& handleWrapper, PropDecl& pd)
{
    auto propStub = ASTCloner::Clone(Ptr(&pd));

    // remove foreign anno from cloned func decl
    for (auto it = propStub->annotations.begin(); it != propStub->annotations.end(); ++it) {
        if ((*it)->kind == AnnotationKind::FOREIGN_NAME) {
            propStub->annotations.erase(it);
            break;
        }
    }

    propStub->outerDecl = Ptr(&handleWrapper);
    handleWrapper.body->decls.push_back(std::move(propStub));
}

void GenerateMirrorInterfaceHandleWrapperAbstractMemberImplStubs(ClassDecl& synthetic, const MemberMap& members)
{
    for (const auto& idMemberSignature : members) {
        const auto& signature = idMemberSignature.second;

        // only abstract functions must be inside synthetic class
        if (!signature.decl->TestAttr(Attribute::ABSTRACT)) {
            continue;
        }

        switch (signature.decl->astKind) {
            case ASTKind::FUNC_DECL:
                GenerateHandleWrapperClassFuncStub(synthetic, *StaticAs<ASTKind::FUNC_DECL>(signature.decl));
                break;
            case ASTKind::PROP_DECL:
                GenerateHandleWrapperClassPropStub(synthetic, *StaticAs<ASTKind::PROP_DECL>(signature.decl));
                break;
            default:
                continue;
        }
    }
}
} // namespace

void GenerateInMirrorInterfaceHandleWrappers::HandleImpl(InteropContext& ctx)
{
    for (auto& wrapper : ctx.mirrorInterfaceHandleWrappers) {
        if (wrapper->TestAttr(Attribute::IS_BROKEN)) {
            continue;
        }

        GenerateMirrorInterfaceHandleWrapperAbstractMemberImplStubs(*wrapper, ctx.structMemberSignatures.at(wrapper));
    }
}
} // namespace Cangjie::Interop::ObjC
