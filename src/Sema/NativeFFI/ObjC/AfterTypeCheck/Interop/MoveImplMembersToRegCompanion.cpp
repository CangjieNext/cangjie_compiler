// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements moving instance fields and finalizer from @ObjCImpl declarations
 * to their corresponding registry companion classes.
 */

#include "Context.h"
#include "Handlers.h"
#include "NativeFFI/ObjC/Utils/ASTQuery.h"
#include "TypeCheckUtil.h"
#include <unordered_map>

namespace Cangjie::Interop::ObjC {

using namespace Cangjie::AST;
using namespace Cangjie::Native::FFI;

namespace {

OwnedPtr<Expr> CreateZeroValue(const ImportManager& importManager, TypeManager& typeManager, Ty& ty)
{
    static constexpr std::string_view ZERO_VALUE_INTRINSIC_NAME = "zeroValue";
    static auto zeroValueDecl = importManager.GetCoreDecl<FuncDecl>(std::string(ZERO_VALUE_INTRINSIC_NAME));
    CJC_ASSERT(zeroValueDecl);
    auto zeroValueCall = MakeOwned<CallExpr>();

    auto fdRef = CreateRefExpr(*zeroValueDecl);
    fdRef->instTys.push_back(&ty);
    auto refTy = typeManager.GetInstantiatedTy(
        zeroValueDecl->GetTy(), TypeCheckUtil::GenerateTypeMapping(*zeroValueDecl, fdRef->instTys));
    fdRef->SetTy(refTy);

    zeroValueCall->baseFunc = std::move(fdRef);
    zeroValueCall->SetTy(&ty);
    zeroValueCall->callKind = CallKind::CALL_INTRINSIC_FUNCTION;
    zeroValueCall->resolvedFunction = zeroValueDecl;
    return zeroValueCall;
}

bool RewireMovedField(PropDecl& implProxyProp, VarDecl& vd, VarDecl& regCompField, InteropContext& ctx)
{
    if (IsObjCImplRegistryCompanionField(vd)) {
        return false;
    }

    if (vd.TestAttr(Attribute::STATIC)) {
        return false;
    }

    vd.DisableAttr(Attribute::PRIVATE, Attribute::INTERNAL, Attribute::PUBLIC);
    vd.EnableAttr(Attribute::PROTECTED);
    vd.isVar = true;
    if (!vd.initializer) {
        vd.initializer = CreateZeroValue(ctx.importManager, ctx.typeManager, *vd.GetTy());
    }

    ctx.astTransformer.TransformToObjCImplProxyProp(implProxyProp, vd, regCompField);

    return true;
}

bool RewireMovedFunc(
    FuncDecl& fd, ClassDecl& regCompanion, InteropContext& ctx, std::vector<OwnedPtr<FuncDecl>>& implProxyFuncs)
{
    if (fd.IsFinalizer()) {
        // no need to rewire
        return true;
    }

    if (fd.TestAttr(Attribute::STATIC)) {
        // we need to create a proxy func, because static functions are accessible in a finalizer
        auto proxyFd = ASTCloner::Clone(Ptr(&fd));
        ctx.astTransformer.TransformToObjCImplStaticProxyFunc(*proxyFd, fd, regCompanion);
        implProxyFuncs.push_back(std::move(proxyFd));

        fd.DisableAttr(Attribute::PRIVATE, Attribute::INTERNAL, Attribute::PUBLIC);
        fd.EnableAttr(Attribute::PROTECTED);
        return true;
    }

    // no need to move non-static member functions
    return false;
}

void RewriteObjCImplFieldsAccess(
    InteropContext& ctx, std::unordered_map<Ptr<VarDecl>, OwnedPtr<PropDecl>>& fieldToProxyProp)
{
    bool insideImpl = false;
    bool hasPropsResolved = false;

    Walker(
        &ctx.pkg,
        [&](Ptr<Node> node) {
            if (!node->IsSamePackage(ctx.pkg)) {
                return VisitAction::SKIP_CHILDREN;
            }

            // Skip registry companion bodies - fields are accessed directly there
            if (IsObjCImplRegistryCompanion(*node->GetTy())) {
                return VisitAction::SKIP_CHILDREN;
            }

            // Track whether we are inside an @ObjCImpl
            if (IsObjCImpl(*node->GetTy())) {
                insideImpl = true;
            }

            // Find RefExpr or MemberAccess whose target is a VarDecl in a registry companion
            Ptr<Decl>* targetSlot = nullptr;
            if (auto ref = As<ASTKind::REF_EXPR>(node)) {
                targetSlot = &ref->ref.target;
            } else if (auto ma = As<ASTKind::MEMBER_ACCESS>(node)) {
                targetSlot = &ma->target;
            }

            if (!targetSlot || !(*targetSlot)) {
                return VisitAction::WALK_CHILDREN;
            }

            auto target = *targetSlot;
            if (!target->outerDecl || !IsObjCImplRegistryCompanion(*target->outerDecl)) {
                return VisitAction::WALK_CHILDREN;
            }

            // Skip if the target is from a different package - it should already resolve to proxy prop
            if (!target->IsSamePackage(ctx.pkg)) {
                return VisitAction::WALK_CHILDREN;
            }

            // Only rewrite VarDecl targets (actual fields, not properties)
            if (target->astKind != ASTKind::VAR_DECL) {
                return VisitAction::WALK_CHILDREN;
            }

            auto userField = StaticAs<ASTKind::VAR_DECL>(target);
            auto& proxyProp = fieldToProxyProp[userField];

            if (insideImpl) {
                // Inside the impl-owner (or its descendant): rewrite to $reg.<field>
                // or Companion.<field> for static
                auto isStatic = userField->TestAttr(Attribute::STATIC);
                auto& regCompanion = *StaticAs<ASTKind::CLASS_DECL>(userField->outerDecl);
                auto& impl = *StaticAs<ASTKind::CLASS_DECL>(proxyProp->outerDecl);
                auto curFile = node->curFile;

                OwnedPtr<Expr> receiver;
                if (isStatic) {
                    receiver = WithinFile(CreateRefExpr(regCompanion), curFile);
                } else {
                    auto regField = GetObjCImplRegCompanionField(impl);
                    CJC_NULLPTR_CHECK(regField);
                    receiver = WithinFile(CreateRefExpr(*regField), curFile);
                }

                auto fieldAccess = WithinFile(CreateMemberAccess(std::move(receiver), *userField), curFile);

                if (auto ref = As<ASTKind::REF_EXPR>(node)) {
                    ref->desugarExpr = std::move(fieldAccess);
                    return VisitAction::SKIP_CHILDREN;
                }

                if (auto ma = As<ASTKind::MEMBER_ACCESS>(node)) {
                    if (isStatic) {
                        // Type.<field> -> Companion.<field>
                        ma->desugarExpr = std::move(fieldAccess);
                        return VisitAction::SKIP_CHILDREN;
                    }
                    // this.<field> -> $reg.<field>
                    if (auto baseRef = As<ASTKind::REF_EXPR>(ma->baseExpr.get()); baseRef && baseRef->isThis) {
                        ma->desugarExpr = std::move(fieldAccess);
                        return VisitAction::SKIP_CHILDREN;
                    }
                }
            }

            // Outside the impl-owner: re-resolve to proxy property
            *targetSlot = proxyProp;
            hasPropsResolved = true;
            return VisitAction::SKIP_CHILDREN;
        },
        [&](Ptr<Node> node) {
            if (IsObjCImpl(*node->GetTy())) {
                insideImpl = false;
            }
            return VisitAction::WALK_CHILDREN;
        })
        .Walk();

    if (hasPropsResolved) {
        ctx.desugarPropRef(ctx.pkg);
    }
}

} // namespace

void MoveImplMembersToRegCompanion::HandleImpl(InteropContext& ctx)
{
    std::unordered_map<Ptr<VarDecl>, OwnedPtr<PropDecl>> fieldToProxyProp;
    for (auto& impl : ctx.impls) {
        if (impl->TestAttr(Attribute::IS_BROKEN)) {
            continue;
        }

        auto regCompanion = ctx.implToRegCompanion[impl];
        auto regCompanionField = GetObjCImplRegCompanionField(*impl);
        std::vector<OwnedPtr<FuncDecl>> implProxyFuncs;
        auto& implMembers = impl->GetMemberDecls();
        for (auto memberIt = implMembers.begin(); memberIt != implMembers.end();) {
            auto member = memberIt->get();
            bool shouldMove = false;

            switch (member->astKind) {
                case ASTKind::VAR_DECL: {
                    auto& vd = *StaticAs<ASTKind::VAR_DECL>(member);
                    auto proxyProp = CreatePropDecl();
                    shouldMove = RewireMovedField(*proxyProp, vd, *regCompanionField, ctx);
                    if (shouldMove) {
                        fieldToProxyProp[&vd] = std::move(proxyProp);
                    }
                    break;
                }
                case ASTKind::FUNC_DECL: {
                    auto& fd = *StaticAs<ASTKind::FUNC_DECL>(member);
                    shouldMove = RewireMovedFunc(fd, *regCompanion, ctx, implProxyFuncs);
                    break;
                }
                default:
                    break;
            }

            if (shouldMove) {
                ctx.astInserter.InsertInto(*regCompanion, std::move(*memberIt));
                memberIt = implMembers.erase(memberIt);
            } else {
                ++memberIt;
            }
        }

        for (auto& proxyFd : implProxyFuncs) {
            ctx.astInserter.InsertInto(*impl, std::move(proxyFd));
        }
    }
    RewriteObjCImplFieldsAccess(ctx, fieldToProxyProp);
    for (auto& [_, proxyProp] : fieldToProxyProp) {
        ctx.astInserter.InsertInto(*StaticCast<ClassDecl>(proxyProp->outerDecl), std::move(proxyProp));
    }
    fieldToProxyProp.clear();
}
} // namespace Cangjie::Interop::ObjC