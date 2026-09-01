// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements the inserter of generated declarations of Cangjie <-> Objective-C interopability.
 */

#include "ASTInserter.h"
#include "cangjie/AST/Match.h"
#include "cangjie/AST/Walker.h"
#include "cangjie/Basic/Position.h"
#include "cangjie/Utils/CastingTemplate.h"
#include "cangjie/Utils/Utils.h"

using namespace Cangjie::AST;
using namespace Cangjie::Interop::ObjC;

namespace {
/// The location-dependent state an inserted node inherits from its target.
struct Location {
    Ptr<File> curFile;
    const std::string& fullPackageName;
    const Cangjie::Position& range;
};

void SetSynthesizedRange(Node& node, const Cangjie::Position& pos)
{
    node.begin = pos;
    node.end = pos;
}

void SetPackageInfo(Decl& decl, const std::string& fullPackageName)
{
    decl.fullPackageName = fullPackageName;
    decl.moduleName = Cangjie::Utils::GetRootPackageName(fullPackageName);
}

void BindNode(Node& node, const Location& loc)
{
    node.curFile = loc.curFile;
    SetSynthesizedRange(node, loc.range);
    if (auto decl = Cangjie::DynamicCast<Decl*>(&node)) {
        SetPackageInfo(*decl, loc.fullPackageName);
        // A synthesized identifier carries no source range, but diagnostics reported on it require a
        // non-zero one. Identifiers relocated from user code keep their own range.
        if (decl->identifier.ZeroPos()) {
            decl->identifier.SetPos(loc.range, loc.range);
        }
    }
}

void Bind(Node& node, const Location& loc, InsertMode mode)
{
    if (mode == InsertMode::SHALLOW) {
        BindNode(node, loc);
        return;
    }
    Walker(&node, [&loc](Ptr<Node> subNode) {
        BindNode(*subNode, loc);
        return VisitAction::WALK_CHILDREN;
    }).Walk();
}
} // namespace

void ASTInserter::InsertInto(File& target, OwnedPtr<Decl> decl, InsertMode mode) const noexcept
{
    Bind(*decl, {&target, target.GetFullPackageName(), target.end}, mode);
    target.decls.push_back(std::move(decl));
}

void ASTInserter::BindTo(ClassLikeDecl& target, Decl& decl, InsertMode mode) const noexcept
{
    CJC_NULLPTR_CHECK(target.curFile);
    decl.outerDecl = &target;
    decl.EnableAttr(Attribute::IN_CLASSLIKE);

    if (auto fd = Cangjie::DynamicCast<FuncDecl>(&decl); fd && fd->funcBody) {
        fd->funcBody->parentClassLike = &target;
    }

    Bind(decl, {target.curFile, target.GetFullPackageName(), target.end}, mode);
}

void ASTInserter::InsertInto(ClassLikeDecl& target, OwnedPtr<Decl> decl, InsertMode mode) const noexcept
{
    BindTo(target, *decl, mode);
    target.GetMemberDecls().push_back(std::move(decl));
}

void ASTInserter::InsertGetterInto(PropDecl& target, OwnedPtr<FuncDecl> getterDecl, InsertMode mode) const noexcept
{
    CJC_NULLPTR_CHECK(target.curFile);
    getterDecl->outerDecl = target.outerDecl;
    getterDecl->propDecl = &target;
    getterDecl->isGetter = true;

    Bind(*getterDecl, {target.curFile, target.GetFullPackageName(), target.end}, mode);
    target.getters.push_back(std::move(getterDecl));
}

void ASTInserter::InsertSetterInto(PropDecl& target, OwnedPtr<FuncDecl> setterDecl, InsertMode mode) const noexcept
{
    CJC_NULLPTR_CHECK(target.curFile);
    setterDecl->outerDecl = target.outerDecl;
    setterDecl->propDecl = &target;
    setterDecl->isSetter = true;

    Bind(*setterDecl, {target.curFile, target.GetFullPackageName(), target.end}, mode);
    target.setters.push_back(std::move(setterDecl));
}
