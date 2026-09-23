// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "Utils.h"
#include "NativeFFI/Utils.h"
#include "TypeCheckUtil.h"
#include "cangjie/AST/AttributePack.h"
#include "cangjie/AST/Create.h"
#include "cangjie/AST/Match.h"
#include "cangjie/Utils/CheckUtils.h"
#include "cangjie/Sema/TypeManager.h"

using namespace Cangjie;
using namespace Cangjie::AST;
using namespace Cangjie::Interop::Java;
using namespace Cangjie::Native::FFI;

namespace Cangjie::Native::FFI::Java {
using namespace Cangjie::TypeCheckUtil;

namespace {
void CloneVisibilityAttribute(const Decl& from, Node& to)
{
    if (from.TestAttr(Attribute::PUBLIC)) {
        to.EnableAttr(Attribute::PUBLIC);
    } else if (from.TestAttr(Attribute::INTERNAL)) {
        to.EnableAttr(Attribute::INTERNAL);
    } else if (from.TestAttr(Attribute::PROTECTED)) {
        to.EnableAttr(Attribute::PROTECTED);
    } else if (from.TestAttr(Attribute::PRIVATE)) {
        to.EnableAttr(Attribute::PRIVATE);
    }
}
}

OwnedPtr<ClassDecl> CloneClassSkeleton(ClassLikeDecl& sample, std::string&& name)
{
    auto cd = WithinFile(MakeOwned<ClassDecl>(), sample.curFile);
    CloneVisibilityAttribute(sample, *cd);

    cd->EnableAttr(Attribute::COMPILER_ADD);
    cd->identifier = std::move(name);
    cd->identifier.SetPos(sample.identifier.Begin(), sample.identifier.End());

    cd->fullPackageName = sample.curFile->curPackage->fullPackageName;
    cd->begin = sample.begin;
    cd->end = sample.end;

    cd->body = WithinFile(MakeOwned<ClassBody>(), sample.curFile);
    cd->body->begin = sample.begin;
    cd->body->end = sample.end;
    cd->moduleName = Cangjie::Utils::GetRootPackageName(sample.fullPackageName);

    return cd;
}

void InsertMethodStub(FuncDecl& fd, const ImportManager& importManager, TypeManager& typeManager)
{
    CJC_ASSERT(fd.funcBody);
    auto argTy = GetStringDecl(importManager).GetTy();
    auto arg = CreateLitConstExpr(LitConstKind::STRING, "It's compiler generated stub.", argTy);
    std::vector<OwnedPtr<Expr>> args;
    args.emplace_back(std::move(arg));

    static auto& exception = GetExceptionDecl(importManager);
    auto throwExpr = CreateThrowException(exception, std::move(args), *fd.curFile, typeManager);

    std::vector<OwnedPtr<Node>> nodes;
    nodes.emplace_back(std::move(throwExpr));

    fd.funcBody->body = CreateBlock(std::move(nodes));
}

/**
 * Assigns the missing function type to each getter of @p pd. For getters whose
 * type is not yet Ty::IsTyCorrect (InitialTy, typical for CJMP-deserialized
 * properties before TypeCheck), sets it to GetFunctionTy({}, pd.GetTy()) and
 * propagates it onto the getter body. Getters that already have a correct type
 * are left untouched.
 * @param pd Mirror property whose getters are completed.
 * @param typeManager Used to build the function type.
 */
void AssignMirrorPropGetters(PropDecl& pd, TypeManager& typeManager)
{
    for (auto& getter : pd.getters) {
        if (!Ty::IsTyCorrect(getter->GetTy())) {
            getter->SetTy(typeManager.GetFunctionTy({}, pd.GetTy()));
            getter->funcBody->SetTy(getter->GetTy());
        }
    }
}

/**
 * Assigns @p pd's type to the single setter parameter when it is unresolved.
 * No-op if the setter has no param list/params or the param type is already
 * correct.
 * @param pd Mirror property that owns the setter.
 * @param setter Setter whose first parameter is completed.
 */
void AssignMirrorSetterParamType(PropDecl& pd, FuncDecl& setter)
{
    if (setter.funcBody->paramLists.empty() || setter.funcBody->paramLists[0]->params.empty()) {
        return;
    }
    auto& setterParam = setter.funcBody->paramLists[0]->params[0];
    if (!Ty::IsTyCorrect(setterParam->GetTy())) {
        setterParam->SetTy(pd.GetTy());
    }
}

/**
 * Assigns the missing function type (and parameter type) to each setter of @p
 * pd. For setters whose type is not yet Ty::IsTyCorrect, sets it to
 * GetFunctionTy({pd.GetTy()}, TYPE_UNIT) and propagates it onto the setter body,
 * then delegates the parameter type to AssignMirrorSetterParamType. Setters
 * that already have a correct type are left untouched (only the param is fixed).
 * @param pd Mirror property whose setters are completed.
 * @param typeManager Used to build the function type.
 */
void AssignMirrorPropSetters(PropDecl& pd, TypeManager& typeManager)
{
    for (auto& setter : pd.setters) {
        if (!Ty::IsTyCorrect(setter->GetTy())) {
            setter->SetTy(typeManager.GetFunctionTy(
                {pd.GetTy()}, TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT)));
            setter->funcBody->SetTy(setter->GetTy());
        }
        AssignMirrorSetterParamType(pd, *setter);
    }
}

/**
 * Fills in function types for the getter/setter of every DESUGARED_MIRROR_FIELD
 * property in a mirror class. Sema-layer companion to InsertMirrorVarProp,
 * called right after it from RewriteJavaMirrorFields because GetFunctionTy needs
 * a TypeManager that is unavailable in the AST layer. Only processes properties
 * whose own type is already Ty::IsTyCorrect, skipping ones still at InitialTy;
 * delegates to AssignMirrorPropGetters / AssignMirrorPropSetters. This is
 * required because CJMP-deserialized VarDecls have InitialTy until TypeCheck
 * resolves them - without it, setter parameters crash ASTTypeValidator and
 * CreateJavaEntityCall.
 * @param decl Mirror class whose converted properties are completed.
 * @param typeManager Used to build getter/setter function types.
 */
void AssignMirrorPropGetterSetterTypes(ClassDecl& decl, TypeManager& typeManager)
{
    for (auto& d : decl.body->decls) {
        if (d->astKind != ASTKind::PROP_DECL) {
            continue;
        }
        auto& pd = *StaticAs<ASTKind::PROP_DECL>(d.get());
        if (!pd.TestAttr(Attribute::DESUGARED_MIRROR_FIELD)) {
            continue;
        }
        if (!Ty::IsTyCorrect(pd.GetTy())) {
            continue;
        }
        AssignMirrorPropGetters(pd, typeManager);
        AssignMirrorPropSetters(pd, typeManager);
    }
}

} // namespace Cangjie::Native::FFI::Java
