// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "GenerateJavaMirrorReferenceWrapperClass.h"
#include "JavaInteropManager.h"
#include "NativeFFI/Java/BeforeTypeCheck/Utils.h"
#include "cangjie/AST/AttributePack.h"
#include "cangjie/AST/Node.h"
#include "cangjie/Modules/ImportManager.h"
#include "cangjie/Utils/SafePointer.h"
#include "cangjie/AST/Utils.h"
#include "cangjie/AST/Create.h"
#include "NativeFFI/Utils.h"
#include "NativeFFI/Java/Utils.h"
#include "cangjie/Utils/CheckUtils.h"

using namespace Cangjie::AST;
using namespace Cangjie::Interop::Java;
using namespace Cangjie::Native::FFI;
using namespace std;

namespace Cangjie::Native::FFI::Java {

namespace {
void InsertSuperTypes(ClassDecl& wrapper, ClassLikeDecl& mirror)
{
    if (mirror.astKind == ASTKind::INTERFACE_DECL) { // add JObject as supertype
        auto jobject = WithinFile(CreateRefType(INTEROP_JOBJECT_NAME), mirror.curFile);
        jobject->begin = mirror.begin;
        jobject->end = mirror.end;
        wrapper.inheritedTypes.emplace_back(std::move(jobject));
    }
    wrapper.inheritedTypes.emplace_back(CreateRefType(mirror));
}

void InsertJavaMirrorAnnotation(ClassDecl& wrapper, const std::string& javaName, const ImportManager& importManager)
{
    auto anno = MakeOwned<Annotation>();
    anno->EnableAttr(Attribute::COMPILER_ADD);
    anno->kind = AnnotationKind::JAVA_MIRROR;
    anno->args.push_back(CreateFuncArg(
        CreateLitConstExpr(LitConstKind::STRING, javaName, GetStringDecl(importManager).GetTy())));

    wrapper.annotations.emplace_back(std::move(anno));
}
} // namespace

bool GenerateJavaMirrorReferenceWrapperClass::ShouldHaveMirrorReferenceWrapper(const Node& node) const
{
    return IsMirror(node) && (node.IsInterfaceDecl() || node.IsAbstractClass());
}

OwnedPtr<ClassDecl> GenerateJavaMirrorReferenceWrapperClass::GenerateWrapperClass(
    const ImportManager& importManager, ClassLikeDecl& mirror) const
{
    CJC_ASSERT(ShouldHaveMirrorReferenceWrapper(mirror));
    auto wrapper = CloneClassSkeleton(mirror, GetMirrorReferenceWrapperNameFromClassLike(mirror));
    wrapper->MarkAsJavaMirror();
    wrapper->EnableAttr(
        Attribute::JAVA_MIRROR_SYNTHETIC_WRAPPER,
        Attribute::NO_REFLECT_INFO);

    InsertSuperTypes(*wrapper, mirror);
    InsertJavaMirrorAnnotation(*wrapper, GetJavaFQName(mirror), importManager);
    return wrapper;
}

void GenerateJavaMirrorReferenceWrapperClass::Process(PreTypeCheckContext& ctx)
{
    for (auto mirror : ctx.javaMirrors) {
        if (!ShouldHaveMirrorReferenceWrapper(*mirror)) {
            continue;
        }
        ctx.AddGeneratedDecl(GenerateWrapperClass(ctx.importManager, *mirror));
    }
}

} // namespace Cangjie::Native::FFI::Java
