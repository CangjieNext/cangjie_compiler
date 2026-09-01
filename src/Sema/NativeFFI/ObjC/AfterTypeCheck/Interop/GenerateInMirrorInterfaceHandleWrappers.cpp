// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements generating member stubs in the handle wrapper class of each @ObjCMirror interface.
 */

#include "Handlers.h"
#include "NativeFFI/ObjC/Utils/ASTQuery.h"
#include "NativeFFI/Utils.h"
#include "cangjie/AST/Create.h"
#include "cangjie/AST/Match.h"
#include "cangjie/AST/Node.h"
#include "cangjie/Utils/CheckUtils.h"
#include <algorithm>

namespace Cangjie::Interop::ObjC {

using namespace Cangjie::AST;
using namespace Cangjie::Native::FFI;

namespace {
/**
 * Clones @p member into @p handleWrapper as a stub.
 *
 * The clone drops @ForeignName: the selector it names belongs to the mirrored protocol member, and the stub is
 * only the wrapper's own implementation of it.
 */
template <typename T> void GenerateHandleWrapperClassStub(ClassDecl& handleWrapper, T& member)
{
    auto stub = ASTCloner::Clone(Ptr(&member));

    auto& annos = stub->annotations;
    auto eraseBeginIt = std::remove_if(
        annos.begin(), annos.end(), [](auto& anno) { return anno->kind == AnnotationKind::FOREIGN_NAME; });
    annos.erase(eraseBeginIt, annos.end());

    RebindClonedStubToSynthetic(*stub, handleWrapper);
    handleWrapper.body->decls.push_back(std::move(stub));
}

void GenerateMirrorInterfaceHandleWrapperAbstractMemberImplStubs(ClassDecl& handleWrapper, const MemberMap& members)
{
    for (const auto& idMemberSignature : members) {
        const auto& signature = idMemberSignature.second;

        // Only abstract members need an implementation stub in the handle wrapper.
        if (!signature.decl->TestAttr(Attribute::ABSTRACT)) {
            continue;
        }

        switch (signature.decl->astKind) {
            case ASTKind::FUNC_DECL:
                GenerateHandleWrapperClassStub(handleWrapper, *StaticAs<ASTKind::FUNC_DECL>(signature.decl));
                break;
            case ASTKind::PROP_DECL:
                GenerateHandleWrapperClassStub(handleWrapper, *StaticAs<ASTKind::PROP_DECL>(signature.decl));
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
