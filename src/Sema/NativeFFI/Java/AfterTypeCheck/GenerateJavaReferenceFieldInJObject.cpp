// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "GenerateJavaReferenceFieldInJObject.h"
#include "NativeFFI/Utils.h"
#include "InteropLibBridge.h"

#include "cangjie/AST/Create.h"
#include "cangjie/AST/Node.h"
#include "cangjie/AST/Types.h"
#include "cangjie/AST/Utils.h"
#include "cangjie/Utils/CheckUtils.h"

namespace Cangjie::Native::FFI::Java {

VarDecl& GenerateJavaReferenceFieldInJObject::InsertJavaReferenceField(ClassDecl& jobject) const
{
    CJC_ASSERT(IsJObject(jobject)); // java.lang.JObject

    auto field = WithinFile(CreateJavaReferenceField(jobject), jobject.curFile);
    auto& res = *field;
    field->outerDecl = &jobject;
    field->fullPackageName = jobject.fullPackageName;
    field->moduleName = jobject.moduleName;
    field->begin = jobject.body->begin;
    field->end = jobject.body->begin;
    field->EnableAttr(Attribute::IN_CLASSLIKE, Attribute::PUBLIC);
    jobject.GetMemberDecls().emplace_back(std::move(field));
    return res;
}

OwnedPtr<VarDecl> GenerateJavaReferenceFieldInJObject::CreateJavaReferenceField(ClassDecl& jobject) const
{
    auto& javaEntityDecl = *ilib.GetJavaEntityDecl();

    auto javaref = CreateVarDecl(JAVA_REF_FIELD_NAME, nullptr, CreateRefType(javaEntityDecl));
    javaref->SetTy(javaEntityDecl.GetTy());
    javaref->EnableAttr(Attribute::INITIALIZATION_CHECKED, Attribute::INITIALIZED);
    javaref->curFile = jobject.curFile;

    Modifier mod = Modifier(TokenKind::PUBLIC, javaref->begin);
    mod.curFile = jobject.curFile;
    javaref->modifiers.emplace(std::move(mod));

    javaref->isVar = false;
    return javaref;
}

GenerateJavaReferenceFieldInJObject::GenerateJavaReferenceFieldInJObject(InteropLibBridge& ilib) : ilib(ilib)
{
}

void GenerateJavaReferenceFieldInJObject::Process(AfterTypeCheckContext& ctx)
{
    for (auto mirror : ctx.GetJavaMirrors()) {
        if (mirror->TestAttr(Attribute::IS_BROKEN)) {
            continue;
        }

        if (IsJObject(*mirror)) {
            InsertJavaReferenceField(StaticCast<ClassDecl>(*mirror));
        }
    }
}

} // namespace Cangjie::Native::FFI::Java
