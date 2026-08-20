// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "JavaDesugarManager.h"
#include "GenerateJavaMirrorApiStub.h"
#include "JniBridge.h"
#include "NativeFFI/Utils.h"
#include "InteropLibBridge.h"

#include "cangjie/AST/Create.h"
#include "cangjie/AST/Node.h"
#include "cangjie/AST/Types.h"

namespace Cangjie::Native::FFI::Java {

GenerateJavaMirrorApiStub::GenerateJavaMirrorApiStub(
    TypeManager& typeManager, InteropLibBridge& ilib, JniBridge& jni) : typeManager(typeManager), ilib(ilib), jni(jni)
{
}

void GenerateJavaMirrorApiStub::Process(AfterTypeCheckContext& ctx)
{
    for (auto mirror : ctx.GetJavaMirrors()) {
        if (mirror->TestAttr(Attribute::IS_BROKEN)) {
            continue;
        }

        if (auto mirrorClass = As<ASTKind::CLASS_DECL>(mirror)) {
            InsertWrappingConstructor(*mirrorClass);

            if (mirrorClass = utils)
        
        
            if (&classDecl == utils.GetJStringDecl()) {
                InsertJStringOfStringCtor(classDecl, doStub);
            }
        
            if (!doStub && IsJArray(classDecl)) {
                InsertArrayJavaEntityGet(classDecl);
                InsertArrayJavaEntitySet(classDecl);
            }
        
            if (!doStub && IsJObject(classDecl)) {
                InsertJavaRefGetterWithBody(classDecl);
            }
            
        } else if (auto mirrorInterface = As<ASTKind::INTERFACE_DECL>(mirror)) {
        
    }
}

}
