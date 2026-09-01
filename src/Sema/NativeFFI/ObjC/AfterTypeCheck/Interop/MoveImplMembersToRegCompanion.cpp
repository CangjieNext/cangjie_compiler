// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements moving instance fields, static members and the finalizer from @ObjCImpl declarations
 * to their corresponding registry companion classes.
 */

#include "Context.h"
#include "Handlers.h"
#include "NativeFFI/ObjC/Utils/ASTQuery.h"
#include "TypeCheckUtil.h"
#include "cangjie/AST/Utils.h"
#include <unordered_map>
#include <vector>

namespace Cangjie::Interop::ObjC {

using namespace Cangjie::AST;
using namespace Cangjie::Native::FFI;

namespace {

/**
 * A moved member and the proxy that takes its place in the @ObjCImpl.
 *
 * Both kinds of proxy share one lifecycle: the proxy is bound to the impl as soon as it is created, while its
 * insertion is postponed until every reference in the package has been re-resolved to it. Inserting earlier
 * would let the rewrite below re-resolve the proxy's own body to itself.
 *
 * A field is proxied by a `PropDecl` forwarding to the registry companion, a static function by a `FuncDecl`
 * of the same signature forwarding there. The finalizer, which the user cannot name, moves without a proxy.
 */
class ProxiedMembers {
public:
    void Add(Decl& origin, OwnedPtr<Decl> proxy) noexcept
    {
        byOrigin[&origin] = proxy.get();
        proxies.push_back(std::move(proxy));
    }

    Ptr<Decl> Find(Ptr<const Decl> origin) const noexcept
    {
        auto found = byOrigin.find(origin);
        return found == byOrigin.end() ? nullptr : found->second;
    }

    /// The proxies in the order their originals were declared.
    std::vector<OwnedPtr<Decl>>& InDeclarationOrder() noexcept
    {
        return proxies;
    }

private:
    std::vector<OwnedPtr<Decl>> proxies;
    std::unordered_map<Ptr<const Decl>, Ptr<Decl>> byOrigin;
};

/// The two classes a member travels between, plus the impl's `$reg` field that instance proxies read through.
struct MoveTarget {
    ClassDecl& impl;
    ClassDecl& regCompanion;
    VarDecl& regCompField;
};

OwnedPtr<Expr> CreateZeroValue(const ImportManager& importManager, TypeManager& typeManager, Ty& ty)
{
    static constexpr std::string_view ZERO_VALUE_INTRINSIC_NAME = "zeroValue";
    auto zeroValueDecl = importManager.GetCoreDecl<FuncDecl>(std::string(ZERO_VALUE_INTRINSIC_NAME));
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

/**
 * The registry companions mirror the impls' inheritance chain, and a moved member is reached from the
 * companions of the impl's subclasses, so it can no longer be private to its original owner.
 */
void OpenUpForRegCompanions(Decl& moved)
{
    moved.DisableAttr(Attribute::PRIVATE, Attribute::INTERNAL, Attribute::PUBLIC);
    moved.EnableAttr(Attribute::PROTECTED);
}

bool RewireMovedField(VarDecl& vd, const MoveTarget& target, InteropContext& ctx, ProxiedMembers& proxied)
{
    if (IsObjCImplRegistryCompanionField(vd)) {
        return false;
    }

    // Access to private static const is a simple load/store in CHIR, so we can leave it as it is
    if (vd.TestAttr(Attribute::STATIC) && vd.isConst) {
        return false;
    }

    auto proxyProp = CreatePropDecl();
    // The proxy takes the place of the field, so it is bound to the impl right away, while the insertion
    // itself is postponed until every reference has been re-resolved to it.
    ctx.astInserter.BindTo(target.impl, *proxyProp);

    if (vd.TestAttr(Attribute::STATIC)) {
        // Static state follows the finalizer and the static functions into the registry companion, so that
        // the impl is left holding nothing but proxies. A static field always carries its own initializer -
        // the parser rejects a static initializer in an @ObjCImpl - so it moves without losing it.
        ctx.astTransformer.TransformToObjCImplProxyProp(*proxyProp, vd, target.regCompanion);
    } else {
        // The proxy prop takes the place of the field, so it has to inherit the visibility and the
        // mutability the user declared. It is built before the field is turned into a `var` below, so that
        // a `let` field keeps a get-only prop and stays read-only on the Objective-C side.
        ctx.astTransformer.TransformToObjCImplProxyProp(*proxyProp, vd, target.regCompField);

        // The moved field itself is always a `var` carrying a value of its own from the start: the registry
        // companion is built by a generated constructor that knows nothing about the user's fields, and the
        // user constructor stays behind in the impl, from where it writes straight into `$reg.<field>` -
        // past the get-only prop above.
        vd.isVar = true;
        if (!vd.initializer) {
            vd.initializer = CreateZeroValue(ctx.importManager, ctx.typeManager, *vd.GetTy());
        }
    }

    OpenUpForRegCompanions(vd);
    proxied.Add(vd, std::move(proxyProp));

    return true;
}

bool RewireMovedFunc(FuncDecl& fd, const MoveTarget& target, InteropContext& ctx, ProxiedMembers& proxied)
{
    if (fd.IsFinalizer()) {
        // The finalizer owns the moved state and is never named, so it moves without a proxy.
        return true;
    }

    // Were a static initializer allowed, it would have to travel with the static fields it assigns: left
    // behind, it would leave them unset for anything reading them through the registry companion.
    CJC_ASSERT_WITH_MSG(!IsStaticInitializer(fd), "@ObjCImpl static initializer is rejected by the parser");

    if (fd.TestAttr(Attribute::STATIC)) {
        // we need to create a proxy func, because static functions are accessible in a finalizer
        auto proxyFd = ASTCloner::Clone(Ptr(&fd));
        // Cloned before the visibility is opened up, so the proxy keeps the one the user declared.
        ctx.astInserter.BindTo(target.impl, *proxyFd);
        ctx.astTransformer.TransformToObjCImplStaticProxyFunc(*proxyFd, fd, target.regCompanion);

        OpenUpForRegCompanions(fd);
        proxied.Add(fd, std::move(proxyFd));

        return true;
    }

    // no need to move non-static member functions
    return false;
}

/**
 * Re-resolves every reference to a moved member.
 *
 * Inside the impl a moved field is reached directly through the registry companion, because the impl owns the
 * `$reg` instance and the companion class alike; everywhere else the reference goes through the proxy.
 */
void RewriteObjCImplMembersAccess(InteropContext& ctx, const ProxiedMembers& proxied)
{
    bool insideImpl = false;
    bool hasPropsResolved = false;

    Walker(
        &ctx.pkg,
        [&](Ptr<Node> node) {
            if (!node->IsSamePackage(ctx.pkg)) {
                return VisitAction::SKIP_CHILDREN;
            }

            auto classDecl = As<ASTKind::CLASS_DECL>(node);

            // Skip registry companion bodies - the members moved there are accessed directly
            if (classDecl && IsObjCImplRegistryCompanion(*classDecl)) {
                return VisitAction::SKIP_CHILDREN;
            }

            // Only the @ObjCImpl class body itself opens the "inside the impl" scope. Testing the node's type
            // instead would match every expression *of* that type as well - `this`, a local of the impl type -
            // and the post-visit of the first of them would close the scope for the rest of the class body.
            if (classDecl && IsObjCImpl(*classDecl)) {
                insideImpl = true;
            }

            // A call keeps its own reference to the callee, besides the one in its base expression
            if (auto call = As<ASTKind::CALL_EXPR>(node); call && call->resolvedFunction) {
                if (auto proxy = proxied.Find(call->resolvedFunction)) {
                    call->resolvedFunction = StaticAs<ASTKind::FUNC_DECL>(proxy);
                }
            }

            // Find RefExpr or MemberAccess whose target is a member moved to a registry companion
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
            auto proxy = proxied.Find(target);
            if (!proxy) {
                return VisitAction::WALK_CHILDREN;
            }

            if (insideImpl && target->astKind == ASTKind::VAR_DECL) {
                // Inside the impl-owner (or its descendant): rewrite to $reg.<field>
                // or Companion.<field> for static
                auto userField = StaticAs<ASTKind::VAR_DECL>(target);
                auto isStatic = userField->TestAttr(Attribute::STATIC);
                auto& regCompanion = *StaticAs<ASTKind::CLASS_DECL>(userField->outerDecl);
                auto& impl = *StaticAs<ASTKind::CLASS_DECL>(proxy->outerDecl);
                auto curFile = node->curFile;

                OwnedPtr<Expr> receiver;
                if (isStatic) {
                    receiver = WithinFile(CreateRefExpr(regCompanion), curFile);
                } else {
                    auto regField = GetObjCImplRegCompanionField(impl);
                    receiver = WithinFile(CreateRefExpr(*regField), curFile);
                }

                auto fieldAccess = WithinFile(CreateMemberAccess(std::move(receiver), *userField), curFile);

                if (auto ref = As<ASTKind::REF_EXPR>(node)) {
                    ref->desugarExpr = std::move(fieldAccess);
                    return VisitAction::SKIP_CHILDREN;
                }

                auto ma = StaticAs<ASTKind::MEMBER_ACCESS>(node);
                auto baseRef = As<ASTKind::REF_EXPR>(ma->baseExpr.get());
                // Type.<field> -> Companion.<field>, this.<field> -> $reg.<field>
                if (isStatic || (baseRef && baseRef->isThis)) {
                    ma->desugarExpr = std::move(fieldAccess);
                    return VisitAction::SKIP_CHILDREN;
                }
            }

            // Outside the impl-owner: re-resolve to the proxy. The children are still walked, because the base
            // expression of a member access may itself refer to a moved member.
            *targetSlot = proxy;
            hasPropsResolved = hasPropsResolved || target->astKind == ASTKind::VAR_DECL;
            return VisitAction::WALK_CHILDREN;
        },
        [&](Ptr<Node> node) {
            if (auto classDecl = As<ASTKind::CLASS_DECL>(node); classDecl && IsObjCImpl(*classDecl)) {
                insideImpl = false;
            }
            return VisitAction::KEEP_DECISION;
        })
        .Walk();

    if (hasPropsResolved) {
        ctx.desugarPropRef(ctx.pkg);
    }
}

} // namespace

void MoveImplMembersToRegCompanion::HandleImpl(InteropContext& ctx)
{
    ProxiedMembers proxied;
    for (auto& impl : ctx.impls) {
        if (impl->TestAttr(Attribute::IS_BROKEN)) {
            continue;
        }

        auto regCompanion = ctx.implToRegCompanion[impl];
        CJC_NULLPTR_CHECK(regCompanion);
        auto regCompanionField = GetObjCImplRegCompanionField(*impl);
        CJC_NULLPTR_CHECK(regCompanionField);
        MoveTarget target{*impl, *regCompanion, *regCompanionField};

        auto& implMembers = impl->GetMemberDecls();
        for (auto memberIt = implMembers.begin(); memberIt != implMembers.end();) {
            auto member = memberIt->get();
            bool shouldMove = false;

            switch (member->astKind) {
                case ASTKind::VAR_DECL:
                    shouldMove = RewireMovedField(*StaticAs<ASTKind::VAR_DECL>(member), target, ctx, proxied);
                    break;
                case ASTKind::FUNC_DECL:
                    shouldMove = RewireMovedFunc(*StaticAs<ASTKind::FUNC_DECL>(member), target, ctx, proxied);
                    break;
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
    }

    RewriteObjCImplMembersAccess(ctx, proxied);

    // Every proxy has been bound to its impl since it was created; only now, with every reference re-resolved
    // to it, is it safe to make it a member.
    for (auto& proxy : proxied.InDeclarationOrder()) {
        ctx.astInserter.InsertInto(*StaticCast<ClassDecl>(proxy->outerDecl), std::move(proxy));
    }
}
} // namespace Cangjie::Interop::ObjC
