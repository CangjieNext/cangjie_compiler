// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * TODO: desc
 */

#include "ASTTransformer.h"
#include "cangjie/AST/Create.h"

using namespace Cangjie::AST;
using namespace Cangjie::Interop::ObjC;

namespace {
void CloneVisibilityAttribute(const Decl& from, Decl& to)
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
} // namespace

ASTTransformer::ASTTransformer(TypeManager& typeManager, ASTFactoryRefactor& astFactory) noexcept
    : nameGenerator(BaseMangler(), typeManager), astFactory(astFactory)
{
}

void ASTTransformer::TransformToRegistryCompanion(ClassDecl& regComp, const ClassDecl& impl) const noexcept
{
    CloneVisibilityAttribute(impl, regComp);
    regComp.EnableAttr(Attribute::OBJ_C_IMPL_REGISTRY_COMPANION);
    regComp.identifier = nameGenerator.GenerateRegistryCompanionName(impl);
}

void ASTTransformer::TransformToHandleWrapper(ClassDecl& handleWrapper, ClassLikeDecl& mirror) const noexcept
{
    CloneVisibilityAttribute(mirror, handleWrapper);
    handleWrapper.EnableAttr(Attribute::OBJ_C_MIRROR_INTERFACE_HANDLE_WRAPPER);
    handleWrapper.identifier = nameGenerator.GenerateHandleWrapperName(mirror);
    handleWrapper.inheritedTypes.push_back(CreateRefType(mirror));
}

void ASTTransformer::TransformToToString(FuncDecl& target) const noexcept
{
    target.identifier = TOSTRING_METHOD_IDENT;
    target.EnableAttr(Attribute::PUBLIC);
}

void ASTTransformer::TransformToInitFromString(FuncDecl& target) const noexcept
{
    auto param = astFactory.CreateFuncParam();
    param->identifier = "str";
    param->type = CreateRefType(STD_LIB_STRING);

    auto& paramLists = target.funcBody->paramLists;
    CJC_ASSERT_WITH_MSG(paramLists.size() > 0, "expect at least one paramList");
    auto& params = paramLists[0]->params;
    params.push_back(std::move(param));
    target.identifier = INIT_IDENT;
    target.EnableAttr(Attribute::PUBLIC, Attribute::CONSTRUCTOR);
}