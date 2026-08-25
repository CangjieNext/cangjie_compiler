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

#include "ASTFactory.h"
#include "cangjie/AST/Create.h"

using namespace Cangjie::AST;
using namespace Cangjie::Interop::ObjC;

OwnedPtr<ClassDecl> ASTFactory::CreateClassDecl() const noexcept
{
    auto cd = MakeOwned<ClassDecl>();
    cd->body = MakeOwned<ClassBody>();

    cd->EnableAttr(Attribute::COMPILER_ADD);
    return cd;
}

OwnedPtr<FuncDecl> ASTFactory::CreateFuncDecl() const noexcept
{
    auto fd = MakeOwned<FuncDecl>();
    fd->funcBody = CreateFuncBody();

    fd->EnableAttr(Attribute::COMPILER_ADD);
    return fd;
}

OwnedPtr<FuncParam> ASTFactory::CreateFuncParam() const noexcept
{
    auto fp = MakeOwned<FuncParam>();
    fp->EnableAttr(Attribute::COMPILER_ADD);
    return fp;
}

OwnedPtr<FuncBody> ASTFactory::CreateFuncBody() const noexcept
{
    auto fb = MakeOwned<FuncBody>();

    std::vector<OwnedPtr<FuncParam>> params;
    const auto paramList = CreateFuncParamList(std::move(params));
    std::vector<OwnedPtr<FuncParamList>> paramLists;
    paramLists.push_back(std::move(paramList));

    fb->paramLists = paramLists;

    fb->EnableAttr(Attribute::COMPILER_ADD);
    return fb;
}

OwnedPtr<PropDecl> ASTFactory::CreatePropDecl() const noexcept
{
    auto pd = MakeOwned<PropDecl>();
    pd->EnableAttr(Attribute::COMPILER_ADD);
    return pd;
}
