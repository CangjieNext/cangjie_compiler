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
    ctx.astInserter.BindTo(target.impl, *proxyProp);

    if (vd.TestAttr(Attribute::STATIC)) {
        // Static state follows the finalizer and the static functions into the registry companion, so that
        // the impl is left holding nothing but proxies. A static field always carries its own initializer -
        // the parser rejects a static initializer in an @ObjCImpl - so it moves without losing it.
        ctx.astTransformer.TransformToObjCImplProxyProp(*proxyProp, vd, target.regCompanion);
    } else {
        // The proxy inherits the visibility and the mutability the user declared. It is built before the
        // field is turned into a `var` below, so that a `let` field keeps a get-only prop and stays
        // read-only on the Objective-C side.
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
        // A static function stays reachable from the finalizer, so it needs a proxy left behind.
        auto proxyFd = ASTCloner::Clone(Ptr(&fd));
        // Cloned before the visibility is opened up, so the proxy keeps the one the user declared.
        ctx.astInserter.BindTo(target.impl, *proxyFd);
        ctx.astTransformer.TransformToObjCImplStaticProxyFunc(*proxyFd, fd, target.regCompanion);

        OpenUpForRegCompanions(fd);
        proxied.Add(fd, std::move(proxyFd));

        return true;
    }

    // Non-static member functions stay in the @ObjCImpl.
    return false;
}

/**
 * Where a reference is read from decides how a member that has moved is reached from there.
 *
 * Only a class body itself opens a region: testing the node's type instead would match every expression *of*
 * that type as well - `this`, a local of the impl type - and leaving the first of them would close the region
 * for the rest of the class body. @ObjCImpls and their companions are top-level, so regions do not nest.
 */
enum class Region {
    ELSEWHERE,     ///< through the proxy that stayed behind in the @ObjCImpl
    IMPL,          ///< straight through `$reg` or the companion class - the impl owns both
    REG_COMPANION, ///< the member is a sibling here; only the qualifier it carried along can be stale
};

Region RegionOf(const ClassDecl& cd)
{
    if (IsObjCImplRegistryCompanion(cd)) {
        return Region::REG_COMPANION;
    }
    return IsObjCImpl(cd) ? Region::IMPL : Region::ELSEWHERE;
}

/**
 * Whether the member is named on some other instance - `other.<member>` - rather than on the class itself or
 * on `this`, which inside a registry companion already denotes the object that holds the moved member.
 */
bool IsNamedOnAnotherInstance(Node& node)
{
    auto ma = As<ASTKind::MEMBER_ACCESS>(&node);
    if (!ma) {
        return false;
    }

    auto base = As<ASTKind::REF_EXPR>(ma->baseExpr.get());
    if (base && (base->isThis || base->isSuper)) {
        return false;
    }

    return !base || DynamicCast<ClassDecl>(base->ref.target) == nullptr;
}

/**
 * `Impl.<member>` -> `Impl$reg.<member>`.
 *
 * A member travels into the companion with the qualifiers the user wrote untouched, so a static call spelled
 * `A.staticFunc()` still names the @ObjCImpl even though the callee now lives in `A$reg`. The class named
 * this way is not necessarily the one the member ended up in - one @ObjCImpl can name another's static, and a
 * subclass can name a static it inherits - so the qualifier becomes the companion of the class the user
 * *named*. That keeps a static dispatch pointed at the same class while satisfying CHIR, which wants the
 * receiver type to be the callee's owner or below it.
 */
void RequalifyToRegCompanion(const InteropContext& ctx, Node& node)
{
    auto ma = As<ASTKind::MEMBER_ACCESS>(&node);
    if (!ma) {
        // An unqualified name resolves against the enclosing class, which moved along with the member.
        return;
    }

    // `this.<member>` and `super.<member>` name the instance, not the class, and keep working.
    auto base = As<ASTKind::REF_EXPR>(ma->baseExpr.get());
    if (!base || base->isThis || base->isSuper) {
        return;
    }

    auto named = ctx.implToRegCompanion.find(base->ref.target);
    if (named == ctx.implToRegCompanion.end()) {
        return;
    }

    ma->baseExpr = WithinFile(CreateRefExpr(*named->second), node.curFile);
}

/**
 * `Impl.<field>` -> `Impl$reg.<field>`, `this.<field>` -> `$reg.<field>`.
 *
 * Reports whether the access was rewritten: a field named on some other instance - `other.<field>` - has no
 * direct form here and goes through the proxy like any reference from outside.
 */
bool RewriteAsDirectFieldAccess(Node& node, VarDecl& userField, const Decl& proxy)
{
    auto isStatic = userField.TestAttr(Attribute::STATIC);
    auto curFile = node.curFile;

    OwnedPtr<Expr> receiver;
    if (isStatic) {
        receiver = WithinFile(CreateRefExpr(*GetOwnerClass(userField)), curFile);
    } else {
        receiver = WithinFile(CreateRefExpr(*GetObjCImplRegCompanionField(*GetOwnerClass(proxy))), curFile);
    }

    auto fieldAccess = WithinFile(CreateMemberAccess(std::move(receiver), userField), curFile);

    if (auto ref = As<ASTKind::REF_EXPR>(&node)) {
        fieldAccess->sourceExpr = ref;
        ref->desugarExpr = std::move(fieldAccess);
        return true;
    }

    auto ma = StaticAs<ASTKind::MEMBER_ACCESS>(&node);
    auto baseRef = As<ASTKind::REF_EXPR>(ma->baseExpr.get());
    if (isStatic || (baseRef && baseRef->isThis)) {
        fieldAccess->sourceExpr = ma;
        ma->desugarExpr = std::move(fieldAccess);
        return true;
    }

    return false;
}

/**
 * Re-resolves every reference to a moved member.
 *
 * Inside the impl a moved field is reached directly through the registry companion, because the impl owns the
 * `$reg` instance and the companion class alike; inside the companion the member is a sibling and only its
 * qualifier can be stale; everywhere else the reference goes through the proxy.
 */
void RewriteObjCImplMembersAccess(InteropContext& ctx, const ProxiedMembers& proxied)
{
    auto region = Region::ELSEWHERE;
    bool hasPropsResolved = false;

    Walker(
        &ctx.pkg,
        [&region, &ctx, &hasPropsResolved, &proxied](Ptr<Node> node) {
            if (!node->IsSamePackage(ctx.pkg)) {
                return VisitAction::SKIP_CHILDREN;
            }

            if (auto classDecl = As<ASTKind::CLASS_DECL>(node)) {
                region = RegionOf(*classDecl);
            }

            // A call keeps its own reference to the callee, besides the one in its base expression. Inside a
            // companion both have to stay pointed at the moved declaration itself, or the call would forward
            // back out to the proxy it is standing next to.
            if (auto call = As<ASTKind::CALL_EXPR>(node);
                call && call->resolvedFunction && region != Region::REG_COMPANION) {
                if (auto proxy = proxied.Find(call->resolvedFunction)) {
                    call->resolvedFunction = StaticAs<ASTKind::FUNC_DECL>(proxy);
                }
            }

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

            // Inside the companion the member is a sibling, so a name resolved against the enclosing class
            // or against the class itself is already right and only its qualifier can be stale. A field
            // named on some other instance has no direct form here either, and falls through to the proxy.
            if (region == Region::REG_COMPANION && !IsNamedOnAnotherInstance(*node)) {
                RequalifyToRegCompanion(ctx, *node);
                return VisitAction::WALK_CHILDREN;
            }

            if (region == Region::IMPL && target->astKind == ASTKind::VAR_DECL &&
                RewriteAsDirectFieldAccess(*node, *StaticAs<ASTKind::VAR_DECL>(target), *proxy)) {
                return VisitAction::SKIP_CHILDREN;
            }

            // The children are still walked, because the base expression of a member access may itself refer
            // to a moved member.
            *targetSlot = proxy;
            hasPropsResolved = hasPropsResolved || target->astKind == ASTKind::VAR_DECL;
            return VisitAction::WALK_CHILDREN;
        },
        [&region](Ptr<Node> node) {
            if (Is<ClassDecl>(node)) {
                region = Region::ELSEWHERE;
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

    // Only now, with every reference re-resolved to it, is a proxy safe to insert - see ProxiedMembers.
    for (auto& proxy : proxied.InDeclarationOrder()) {
        ctx.astInserter.InsertInto(*StaticCast<ClassDecl>(proxy->outerDecl), std::move(proxy));
    }
}
} // namespace Cangjie::Interop::ObjC
