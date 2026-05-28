// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "JavaInteropManager.h"
#include "NativeFFI/Java/BeforeTypeCheck/GenerateJavaImplRegistryCompanion.h"
#include "NativeFFI/Java/BeforeTypeCheck/GenerateJavaMirrorInterfaceDefaultMethodStub.h"
#include "NativeFFI/Java/BeforeTypeCheck/GenerateJavaMirrorReferenceWrapperClass.h"
#include "NativeFFI/Java/BeforeTypeCheck/RewriteJavaMirrorFields.h"
#include "cangjie/AST/Node.h"

namespace Cangjie::Native::FFI::Java {
using namespace Cangjie::AST;
using namespace Cangjie::Interop::Java;

void PreTypeCheckStage::operator()(PreTypeCheckContext& ctx)
{
    Process(ctx);
    for (auto& generatedDecl : ctx.generated) {
        /*
         * Make the generated declaration visible and available via ImportManager/CjoManager.
         */
        ctx.importManager.GetCjoManager()->AddGeneratedDeclToDeclMap(*generatedDecl);
    }
    std::move(ctx.generated.begin(), ctx.generated.end(), std::back_inserter(ctx.file.decls));
    ctx.generated.clear();
}

void PreTypeCheckContext::AddGeneratedDecl(OwnedPtr<Decl>&& decl)
{
    generated.emplace_back(std::move(decl));
}

JavaInteropManager::JavaInteropManager(const Cangjie::ImportManager& importManager, Cangjie::TypeManager& typeManager)
    : importManager(importManager), typeManager(typeManager)
{
}

void JavaInteropManager::Process(Package& pkg) const
{
    // 1. Classes generation should be done before other transformations as the first stage.
    //    Late transformations affect generated wrappers (fills bodies, etc).
    for (auto& file : pkg.files) {
        auto ctx = PreTypeCheckContext(importManager, typeManager, *file);
        Process<GenerateJavaMirrorReferenceWrapperClass>(ctx);
        Process<GenerateJavaImplRegistryCompanionClass>(ctx);
    }

    // 2. Transform java specific declarations.
    for (auto& file : pkg.files) {
        auto ctx = PreTypeCheckContext(importManager, typeManager, *file);
        Process<RewriteJavaMirrorFields>(ctx);
        Process<GenerateJavaMirrorInterfaceDefaultMethodStub>(ctx);
    }
}

} // namespace Cangjie::Native::FFI::Java
