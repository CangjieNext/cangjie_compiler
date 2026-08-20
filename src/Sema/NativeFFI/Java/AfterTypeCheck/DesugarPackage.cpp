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
#include "NativeFFI/Java/AfterTypeCheck/GenerateJavaMirrorApiStub.h"
#include "NativeFFI/Java/AfterTypeCheck/GenerateJavaReferenceFieldInJObject.h"
#include "NativeFFI/Java/AfterTypeCheck/PopulateJavaMirrorStubs.h"
#include "RewriteJavaImplReferenceWrapperFields.h"
#include "GenerateNativeBridgeForJavaImpl.h"
#include "DesugarTypeCheckingAndCasting.h"
#include "DesugarJArray.h"
#include "NativeFFI/Java/AfterTypeCheck/InteropLibBridge.h"
#include <unordered_map>

namespace Cangjie::Interop::Java {

void JavaDesugarManager::ProcessJavaMirrorImplStages(AfterTypeCheckContext& ctx,
    std::function<void(AST::Node&)> desugarPropRef)
{
    Process<GenerateJavaReferenceFieldInJObject>(ctx, lib);
    Process<GenerateJavaMirrorApiStub>(ctx, typeManager, lib, utils, memberMap);
    Process<GenerateJavaImplApiStub>(ctx, typeManager, lib, jniBridge);

    for (auto& file : ctx.pkg.files) {
        GenerateInMirrors(*file);
    }

    Process<GenerateInJavaImplRegistryCompanion>(ctx, typeManager, lib);
    Process<DesugarJavaImplSuperConstructorCall>(ctx, typeManager, lib, jniBridge, diag, utils);
    Process<GenerateInJavaImplReferenceWrapper>(ctx, typeManager, importManager, lib, factory);
    Process<DesugarJavaImplSuperMethodCall>(ctx, lib, factory);
    Process<RewriteJavaImplReferenceWrapperFields>(ctx, typeManager, utils, desugarPropRef);
    Process<GenerateNativeBridgeForJavaImpl>(ctx, typeManager, importManager, lib, jniBridge);

    Process<PopulateJavaMirrorStubs>(ctx, typeManager, lib, factory);

    Process<DesugarJArray>(ctx, typeManager, importManager, lib);
    GenerateJavaSourceCode(ctx);
    Process<DesugarTypeCheckingAndCasting>(ctx, lib, diag, utils);
}

void JavaInteropManager::DesugarPackage(Package& pkg,
    const std::unordered_map<Ptr<const InheritableDecl>,
    MemberMap>& memberMap,
    std::function<void(AST::Node&)> desugarPropRef)
{
    if (!hasMirrorOrImpl) {
        return;
    }
    JavaDesugarManager desugarer{
        importManager, typeManager, diag, mangler, javagenOutputPath, outputPath, memberMap};

    if (!InteropLibBridge::IsInteropLibAccessible(importManager)) {
        return;
    }

    if (hasMirrorOrImpl) {
        AfterTypeCheckContext ctx{importManager, typeManager, pkg};
        desugarer.ProcessJavaMirrorImplStages(ctx, desugarPropRef);
    }
}

} // namespace Cangjie::Interop::Java
