// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "JavaDesugarManager.h"
#include "JavaInteropManager.h"
#include "GenerateJavaImplApiStub.h"
#include "GenerateInJavaImplRegistryCompanion.h"
#include "DesugarJavaImplSuperConstructorCall.h"
#include "DesugarJavaImplSuperMethodCall.h"
#include "GenerateInJavaImplReferenceWrapper.h"
#include "NativeFFI/Java/AfterTypeCheck/AfterTypeCheckContext.h"
#include "RewriteJavaImplReferenceWrapperFields.h"
#include "GenerateNativeBridgeForJavaImpl.h"
#include "DesugarTypeCheckingAndCasting.h"
#include "DesugarJArray.h"
#include "NativeFFI/Java/AfterTypeCheck/InteropLibBridge.h"
#include "NativeFFI/Java/AfterTypeCheck/JniBridge.h"
#include <unordered_map>

namespace Cangjie::Interop::Java {

void JavaDesugarManager::ProcessJavaMirrorImplStages(AfterTypeCheckContext& ctx,
    std::function<void(AST::Node&)> desugarPropRef)
{
    auto jniEnvPtrDecl = lib.GetJniEnvPtrDecl();
    auto jobjectDecl = lib.GetJobjectDecl();
    if (!jniEnvPtrDecl || !jobjectDecl) {
        return;
    }
    JniBridge jniBridge{typeManager, mangler, utils, *jniEnvPtrDecl, *jobjectDecl};

    for (auto& file : ctx.pkg.files) {
        GenerateInMirrors(*file, true);
    }

    Process<GenerateJavaImplApiStub>(ctx, typeManager, lib, jniBridge);

    for (auto& file : ctx.pkg.files) {
        GenerateInMirrors(*file, false);
    }

    Process<GenerateInJavaImplRegistryCompanion>(ctx, typeManager, lib);
    Process<DesugarJavaImplSuperConstructorCall>(ctx, typeManager, lib, jniBridge, diag, utils);
    Process<GenerateInJavaImplReferenceWrapper>(ctx, typeManager, importManager, lib);
    Process<DesugarJavaImplSuperMethodCall>(ctx, lib, utils);
    Process<RewriteJavaImplReferenceWrapperFields>(ctx, typeManager, utils, desugarPropRef);
    Process<GenerateNativeBridgeForJavaImpl>(ctx, typeManager, importManager, lib, jniBridge);

    for (auto& file : ctx.pkg.files) {
        DesugarMirrors(*file);
    }

    GenerateJavaSourceCode(ctx);
}

void JavaDesugarManager::DesugarCallSites(AfterTypeCheckContext& ctx)
{
    Process<DesugarJArray>(ctx, typeManager, importManager, lib);
    Process<DesugarTypeCheckingAndCasting>(ctx, lib, diag, utils);
}

void JavaInteropManager::DesugarPackage(Package& pkg,
    const std::unordered_map<Ptr<const InheritableDecl>,
    MemberMap>& memberMap,
    std::function<void(AST::Node&)> desugarPropRef)
{
    JavaDesugarManager desugarer{
        importManager, typeManager, diag, mangler, javagenOutputPath, outputPath, memberMap};

    AfterTypeCheckContext ctx{importManager, typeManager, pkg};
    if (hasMirrorOrImpl) {
        desugarer.ProcessJavaMirrorImplStages(ctx, desugarPropRef);
    }
    desugarer.DesugarCallSites(ctx);
}

} // namespace Cangjie::Interop::Java
