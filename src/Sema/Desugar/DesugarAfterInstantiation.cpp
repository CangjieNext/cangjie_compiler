// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements the Desugar functions used after instantiation step.
 */

#include "DesugarInTypeCheck.h"

#include <atomic>
#include <memory>
#include <set>
#include <utility>
#include <vector>
#include <fstream>

#include "AutoBoxing.h"
#include "ExtendBoxMarker.h"
#include "TypeCheckUtil.h"
#include "TypeCheckerImpl.h"

#include "cangjie/AST/Clone.h"
#include "cangjie/AST/Create.h"
#include "cangjie/AST/Match.h"
#include "cangjie/AST/Types.h"
#include "cangjie/AST/Utils.h"
#include "cangjie/AST/Walker.h"
#include "cangjie/Basic/Match.h"
#include "cangjie/Modules/ImportManager.h"
#include "cangjie/Sema/TypeManager.h"
#include "cangjie/Utils/CheckUtils.h"
#include "cangjie/Utils/FileUtil.h"
#include "cangjie/Utils/Utils.h"

using namespace Cangjie;
using namespace AST;
using namespace Meta;
using namespace TypeCheckUtil;

// Perform desugar after generic instantiation.
void TypeChecker::PerformDesugarAfterInstantiation(ASTContext& ctx, Package& pkg) const
{
    impl->PerformDesugarAfterInstantiation(ctx, pkg);
}

namespace {
void UpdateDeclAttributes(Package& pkg, bool exportForTest)
{
    Walker(&pkg, [&exportForTest](auto node) {
        if (auto vd = DynamicCast<VarDecl*>(node); vd && vd->initializer) {
            vd->EnableAttr(Attribute::DEFAULT);
        }
        if (exportForTest) {
            if (auto fd = DynamicCast<FuncDecl*>(node); fd && !fd->TestAttr(Attribute::PRIVATE)) {
                auto isExtend = Is<ExtendDecl>(fd->outerDecl);
                auto isForeignFunc = fd->TestAttr(Attribute::FOREIGN);
                if (!isExtend && !isForeignFunc) {
                    return VisitAction::WALK_CHILDREN;
                }
                // Skip declarations added by the compiler because they wouldn't be accessible in tests anyway
                if (isExtend && fd->outerDecl->TestAttr(Attribute::COMPILER_ADD)) {
                    return VisitAction::WALK_CHILDREN;
                }
                fd->linkage = Linkage::EXTERNAL;
                if (fd->propDecl) {
                    fd->propDecl->linkage = Linkage::EXTERNAL;
                }
                if (isExtend) {
                    fd->outerDecl->EnableAttr(Attribute::FOR_TEST);
                } else {
                    fd->EnableAttr(Attribute::FOR_TEST);
                }
            }
        }
        return VisitAction::WALK_CHILDREN;
    }).Walk();
}

/**
 * For compiled with the `--coverage` option,
 * Clear line info for:
 *   - Compiler add by generic instantiation.
 */
void ClearLineInfoAfterSema(Package& pkg)
{
    std::function<VisitAction(Ptr<Node>)> clearGenericInst = [](Ptr<Node> node) -> VisitAction {
        node->begin.line = 0;
        node->begin.column = 0;
        node->begin.Mark(PositionStatus::IGNORE);
        return VisitAction::WALK_CHILDREN;
    };
    for (auto& decl : pkg.genericInstantiatedDecls) {
        Walker(decl, clearGenericInst).Walk();
    }
}

void IterateTyWithArgs(Ptr<Ty> ty, const std::function<void(Ptr<Ty>)>& func)
{
    if (!Ty::IsTyCorrect(ty)) {
        return;
    }
    func(ty);
    for (auto& tyArg : ty->typeArgs) {
        IterateTyWithArgs(tyArg, func);
    }
}
} // namespace

VisitAction TypeChecker::TypeCheckerImpl::CollectUsedDecl(Ptr<Decl> decl, unsigned walkerId)
{
    // 函数参数和泛型参数不算被使用，因为它们的类型可能会引用其他声明，但它们本身不需要被导入。但这个可能导致Mangle有问题？
    if (Is<FuncParam>(decl) || Is<GenericParamDecl>(decl)) {
        return VisitAction::WALK_CHILDREN;
    }
    // 使用到的泛型实例化声明要连着原始的泛型声明一起标记为被使用。
    if (decl->TestAttr(Attribute::GENERIC_INSTANTIATED) && decl->genericDecl &&
        decl->genericDecl->TestAttr(Attribute::IMPORTED)) {
        CollectUsedNode(decl->genericDecl, walkerId);
    }
    // 非导入的声明不标记。
    if (!decl->TestAttr(Attribute::IMPORTED)) {
        return VisitAction::WALK_CHILDREN;
    }
    // 已经标记过的声明不重复标记，避免死循环，且无需再重复处理子节点。
    if (decl->TestAttr(Attribute::USED_IMPORTS)) {
        return VisitAction::SKIP_CHILDREN;
    }
    decl->EnableAttr(Attribute::USED_IMPORTS);
    // 属性的解糖函数被使用，propDecl也算被使用。
    if (auto fd = DynamicCast<FuncDecl>(decl); fd && fd->propDecl) {
        fd->propDecl->EnableAttr(Attribute::USED_IMPORTS);
    }
    // 如果成员被使用，那么它所在的类型也要被标记为使用。
    if (decl->outerDecl) {
        CollectUsedNode(decl->outerDecl, walkerId);
    }
    if (!decl->IsNominalDecl()) {
        return VisitAction::WALK_CHILDREN;
    }
    // 类型的扩展都要保留。
    auto extends = typeManager.GetAllExtendsByTy(*decl->ty);
    for (auto extend : extends) {
        if (extend == decl || !extend->TestAttr(Attribute::IMPORTED)) {
            continue;
        }
        CollectUsedNode(extend, walkerId);
    }
    return VisitAction::WALK_CHILDREN;
}

void TypeChecker::TypeCheckerImpl::CollectUsedNode(Ptr<Node> node, unsigned walkerId)
{
    std::function<VisitAction(Ptr<Node>)> collectUsed = [this, &walkerId, &collectUsed](Ptr<Node> node) -> VisitAction {
        auto procTy = [&walkerId, &collectUsed](Ptr<Ty> ty) {
            if (auto decl = Ty::GetDeclOfTy(ty)) {
                Walker(decl, walkerId, collectUsed).Walk();
            }
        };
        IterateTyWithArgs(node->ty, procTy);
        if (auto type = DynamicCast<Type>(node); type && !Ty::IsInitialTy(type->aliasTy)) {
            IterateTyWithArgs(type->aliasTy, procTy);
        } else if (auto decl = DynamicCast<Decl>(node)) {
            return CollectUsedDecl(decl, walkerId);
        } else if (auto ae = DynamicCast<ArrayExpr>(node); ae && ae->initFunc) {
            Walker(ae->initFunc, walkerId, collectUsed).Walk();
        }
        Ptr<Decl> target = node->GetTarget();
        if (auto ce = DynamicCast<CallExpr>(node); ce && ce->resolvedFunction) {
            target = ce->resolvedFunction;
        }
        if (!target) {
            return VisitAction::WALK_CHILDREN;
        }
        Walker(target, walkerId, collectUsed).Walk();
        return VisitAction::WALK_CHILDREN;
    };
    Walker(node, walkerId, collectUsed).Walk();
}

/*
主体逻辑：
1. 先处理隐式使用的声明，对隐式使用的声明进行遍历，找出所有依赖；
2. 处理所有内建类型的扩展声明，由于扩展会影响虚表的生成，因此需要将扩展以及扩展所依赖的声明进行收集；
3. 遍历源码文件，对文件中的每个声明进行遍历，找出所有依赖；
4. 处理实例化出来的声明，并收集所有实例化声明的依赖
5. 删除未使用的源码导入声明及inline函数（一定是导入声明）。

依赖查询：
1. 通过本包或导入包的Decl进行查询。
*/
void TypeChecker::TypeCheckerImpl::CollectUsedPackageInFile(Ptr<Package> pkg)
{
    const std::unordered_set<std::string_view> implicitUsedDecls = {"CJ_CORE_ExecAtexitCallbacks", "getCommandLineArgs",
        "eprintln", "handleException", "NegativeArraySizeException", "createOverflowExceptionMsg",
        "createArithmeticExceptionMsg", "SpawnException"};
    unsigned walkerId = Walker::GetNextWalkerID();

    for (auto implicitUsedDecl : implicitUsedDecls) {
        auto decl = importManager.GetCoreDecl(std::string(implicitUsedDecl));
        CollectUsedNode(decl, walkerId);
    }
    for (auto& [_, builtinExtends] : typeManager.builtinTyToExtendMap) {
        for (auto& extend : builtinExtends) {
            CollectUsedNode(extend, walkerId);
        }
    }
    for (auto& file : curPackage->files) {
        CollectUsedNode(file.get(), walkerId);
    }
    for (auto& instDecl : curPackage->genericInstantiatedDecls) {
        CollectUsedNode(instDecl.get(), walkerId);
    }
    // 删除源码包中的无用的源码依赖。
    auto isUnusedDecls = [](Ptr<Decl> decl) { return !decl->TestAttr(Attribute::USED_IMPORTS); };
    Utils::EraseIf(curPackage->srcImportedNonGenericDecls, isUnusedDecls);
    Utils::EraseIf(curPackage->inlineFuncDecls, isUnusedDecls);
}

void TypeChecker::TypeCheckerImpl::PerformDesugarAfterInstantiation([[maybe_unused]] ASTContext& ctx, Package& pkg)
{
    if (pkg.files.empty()) {
        return;
    }
    PerformRecursiveTypesElimination();
    UpdateDeclAttributes(pkg, ci->invocation.globalOptions.exportForTest);
    if (ci->invocation.globalOptions.enableCoverage) {
        ClearLineInfoAfterSema(pkg);
    }
    Utils::ProfileRecorder removeUnusedRocord("PerformDesugarAfterInstantiation", "CollectUsedPackageInFile");
    CollectUsedPackageInFile(&pkg);
}

bool AutoBoxing::NeedBoxOption(Ty& child, Ty& target)
{
    if (Ty::IsInitialTy(&child) || Ty::IsInitialTy(&target) ||
        (typeManager.CheckTypeCompatibility(&child, &target, false, target.IsGeneric()) !=
            TypeCompatibility::INCOMPATIBLE) ||
        child.kind == TypeKind::TYPE_NOTHING || target.kind != TypeKind::TYPE_ENUM) {
        return false;
    }
    auto lCnt = CountOptionNestedLevel(child);
    auto rCnt = CountOptionNestedLevel(target);
    // If type contains generic ty, current is node inside @Java class. Otherwise, incompatible types need to be boxed.
    if (lCnt == rCnt && child.HasGeneric()) {
        return false;
    }
    auto enumTy = RawStaticCast<EnumTy*>(&target);
    if (enumTy->declPtr->fullPackageName != CORE_PACKAGE_NAME || enumTy->declPtr->identifier != STD_LIB_OPTION) {
        return false;
    }
    return true;
}

// Option Box happens twice before and after instantiation, and must before extend box.
void AutoBoxing::TryOptionBox(EnumTy& target, Expr& expr)
{
    if (expr.ty && target.typeArgs[0] && NeedBoxOption(*expr.ty, *target.typeArgs[0])) {
        TryOptionBox(*StaticCast<EnumTy*>(target.typeArgs[0]), expr);
    }
    auto ed = target.decl;
    Ptr<FuncDecl> optionDecl = nullptr;
    for (auto& it : ed->constructors) {
        if (it->identifier == OPTION_VALUE_CTOR) {
            optionDecl = StaticCast<FuncDecl*>(it.get());
            break;
        }
    }
    if (optionDecl == nullptr) {
        return;
    }

    auto baseFunc = CreateRefExpr(OPTION_VALUE_CTOR);
    baseFunc->EnableAttr(Attribute::IMPLICIT_ADD);
    baseFunc->ref.target = optionDecl;
    baseFunc->ty = typeManager.GetInstantiatedTy(optionDecl->ty, GenerateTypeMapping(*ed, target.typeArgs));

    std::vector<OwnedPtr<FuncArg>> arg;
    if (expr.desugarExpr) {
        arg.emplace_back(CreateFuncArg(std::move(expr.desugarExpr)));
    } else {
        arg.emplace_back(CreateFuncArg(ASTCloner::Clone(Ptr(&expr))));
    }

    auto ce = CreateCallExpr(std::move(baseFunc), std::move(arg));
    ce->callKind = AST::CallKind::CALL_DECLARED_FUNCTION;
    ce->ty = &target;
    ce->resolvedFunction = optionDecl;
    if (expr.astKind == ASTKind::BLOCK) {
        // For correct deserialization, we need to keep type of block.
        auto b = MakeOwnedNode<Block>();
        b->ty = ce->ty;
        b->body.emplace_back(std::move(ce));
        expr.desugarExpr = std::move(b);
    } else {
        expr.desugarExpr = std::move(ce);
    }
    AddCurFile(*expr.desugarExpr, expr.curFile);
    expr.ty = expr.desugarExpr->ty;
}

/**
 * Option Box happens before type check finished with no errors.
 * All nodes and sema types should be valid.
 */
void AutoBoxing::AddOptionBox(Package& pkg)
{
    std::function<VisitAction(Ptr<Node>)> preVisit = [this](Ptr<Node> node) -> VisitAction {
        return match(*node)([this](const VarDecl& vd) { return AddOptionBoxHandleVarDecl(vd); },
            [this](const AssignExpr& ae) { return AddOptionBoxHandleAssignExpr(ae); },
            [this](CallExpr& ce) { return AddOptionBoxHandleCallExpr(ce); },
            [this](const IfExpr& ie) { return AddOptionBoxHandleIfExpr(ie); },
            [this](TryExpr& te) { return AddOptionBoxHandleTryExpr(te); },
            [this](const ReturnExpr& re) { return AddOptionBoxHandleReturnExpr(re); },
            [this](ArrayLit& lit) { return AddOptionBoxHandleArrayLit(lit); },
            [this](MatchExpr& me) { return AddOptionBoxHandleMatchExpr(me); },
            [this](const TupleLit& tl) { return AddOptionBoxHandleTupleList(tl); },
            [this](ArrayExpr& ae) { return AddOptionBoxHandleArrayExpr(ae); },
            []() { return VisitAction::WALK_CHILDREN; });
    };
    Walker walker(&pkg, preVisit);
    walker.Walk();
}

VisitAction AutoBoxing::AddOptionBoxHandleTupleList(const TupleLit& tl)
{ // Tuple literal allows element been boxed.
    auto tupleTy = DynamicCast<TupleTy*>(tl.ty);
    if (tupleTy == nullptr) {
        return VisitAction::WALK_CHILDREN;
    }
    auto typeArgs = tupleTy->typeArgs;
    for (size_t i = 0; i < typeArgs.size(); ++i) {
        if (tl.children[i]->ty && typeArgs[i] && NeedBoxOption(*tl.children[i]->ty, *typeArgs[i])) {
            TryOptionBox(*StaticCast<EnumTy*>(typeArgs[i]), *tl.children[i]);
        }
    }
    return VisitAction::WALK_CHILDREN;
}

VisitAction AutoBoxing::AddOptionBoxHandleMatchExpr(MatchExpr& me)
{
    for (auto& single : me.matchCases) {
        CJC_ASSERT(me.ty && single->exprOrDecls);
        AddOptionBoxHandleBlock(*single->exprOrDecls, *me.ty);
    }
    for (auto& caseOther : me.matchCaseOthers) {
        CJC_ASSERT(me.ty && caseOther->exprOrDecls);
        AddOptionBoxHandleBlock(*caseOther->exprOrDecls, *me.ty);
    }
    return VisitAction::WALK_CHILDREN;
}

VisitAction AutoBoxing::AddOptionBoxHandleArrayLit(ArrayLit& lit)
{
    if (Ty::IsInitialTy(lit.ty) || !lit.ty->IsStructArray()) {
        return VisitAction::WALK_CHILDREN;
    }

    if (lit.ty->typeArgs.size() == 1) {
        auto targetTy = lit.ty->typeArgs[0];
        CJC_NULLPTR_CHECK(targetTy);
        for (auto& child : lit.children) {
            if (child->ty && NeedBoxOption(*child->ty, *targetTy)) {
                TryOptionBox(*StaticCast<EnumTy*>(targetTy), *child);
            }
        }
    }
    return VisitAction::WALK_CHILDREN;
}

VisitAction AutoBoxing::AddOptionBoxHandleIfExpr(const IfExpr& ie)
{
    if (!Ty::IsTyCorrect(ie.ty) || ie.ty->IsUnitOrNothing() || !ie.thenBody) {
        return VisitAction::WALK_CHILDREN;
    }
    AddOptionBoxHandleBlock(*ie.thenBody, *ie.ty);
    if (ie.hasElse && ie.elseBody) {
        if (auto block = DynamicCast<Block*>(ie.elseBody.get()); block) {
            AddOptionBoxHandleBlock(*block, *ie.ty);
        } else if (auto elseIfExpr = DynamicCast<IfExpr*>(ie.elseBody.get());
            elseIfExpr && Ty::IsTyCorrect(elseIfExpr->ty) && NeedBoxOption(*elseIfExpr->ty, *ie.ty)) {
            TryOptionBox(*StaticCast<EnumTy*>(ie.ty), *elseIfExpr);
            elseIfExpr->ty = ie.ty;
        }
    }
    return VisitAction::WALK_CHILDREN;
}

void AutoBoxing::AddOptionBoxHandleBlock(Block& block, Ty& ty)
{
    // If the block is empty or end with declaration, the last type is 'Unit',
    // otherwise the last type is the type of last expression.
    auto lastExprOrDecl = block.GetLastExprOrDecl();
    Ptr<Ty> lastTy = TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT);
    if (auto expr = DynamicCast<Expr*>(lastExprOrDecl)) {
        lastTy = expr->ty;
    }
    if (!lastTy || !NeedBoxOption(*lastTy, ty)) {
        return;
    }
    // If the block is empty or end with declaration, we need to insert a unitExpr for box.
    if (Is<Decl>(lastExprOrDecl) || block.body.empty()) {
        auto unitExpr = CreateUnitExpr(TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT));
        unitExpr->curFile = block.curFile;
        lastExprOrDecl = unitExpr.get();
        block.body.emplace_back(std::move(unitExpr));
    }

    if (auto lastExpr = DynamicCast<Expr*>(lastExprOrDecl)) {
        TryOptionBox(StaticCast<EnumTy&>(ty), *lastExpr);
        block.ty = lastExpr->ty;
    }
}

VisitAction AutoBoxing::AddOptionBoxHandleTryExpr(TryExpr& te)
{
    if (!Ty::IsTyCorrect(te.ty)) {
        return VisitAction::WALK_CHILDREN;
    }
    if (te.tryBlock) {
        AddOptionBoxHandleBlock(*te.tryBlock, *te.ty);
    }
    for (auto& ce : te.catchBlocks) {
        AddOptionBoxHandleBlock(*ce, *te.ty);
    }
    return VisitAction::WALK_CHILDREN;
}

VisitAction AutoBoxing::AddOptionBoxHandleArrayExpr(ArrayExpr& ae)
{
    bool ignore = !Ty::IsTyCorrect(ae.ty) || ae.initFunc || ae.args.size() < 1;
    if (ignore) {
        return VisitAction::WALK_CHILDREN;
    }
    auto targetTy = typeManager.GetTypeArgs(*ae.ty)[0];

    Ptr<FuncArg> arg = nullptr;
    if (ae.isValueArray) {
        // For VArray only one argument, and it need option box.
        arg = ae.args[0].get();
    } else if (ae.args.size() > 1) {
        // For RawArray(size, item:T) boxing argIndex is 1, only this case may need option box.
        arg = ae.args[1].get();
    }
    if (arg && arg->expr && arg->expr->ty && targetTy && NeedBoxOption(*arg->expr->ty, *targetTy)) {
        TryOptionBox(*StaticCast<EnumTy*>(targetTy), *arg->expr);
        arg->ty = arg->expr->ty;
    }
    return VisitAction::WALK_CHILDREN;
}

VisitAction AutoBoxing::AddOptionBoxHandleCallExpr(CallExpr& ce)
{
    bool ignored = !ce.baseFunc || !ce.baseFunc->ty || ce.baseFunc->ty->kind != TypeKind::TYPE_FUNC;
    if (ignored) {
        return VisitAction::WALK_CHILDREN;
    }
    auto funcTy = RawStaticCast<FuncTy*>(ce.baseFunc->ty);
    unsigned count = 0;
    auto callCheck = [&count, &funcTy, this](auto begin, auto end) {
        for (auto it = begin; it != end; ++it) {
            if (count >= funcTy->paramTys.size()) {
                break;
            }
            auto paramTy = funcTy->paramTys[count];
            // It's possible that childs have different box type, so does not break after match.
            if ((*it)->expr && (*it)->expr->ty && paramTy && NeedBoxOption(*(*it)->expr->ty, *paramTy)) {
                TryOptionBox(*StaticCast<EnumTy*>(paramTy), *(*it)->expr);
                (*it)->ty = (*it)->expr->ty;
            }
            ++count;
        }
    };
    if (ce.desugarArgs.has_value()) {
        callCheck(ce.desugarArgs->begin(), ce.desugarArgs->end());
    } else {
        callCheck(ce.args.begin(), ce.args.end());
    }
    return VisitAction::WALK_CHILDREN;
}

VisitAction AutoBoxing::AddOptionBoxHandleAssignExpr(const AssignExpr& ae)
{
    if (ae.desugarExpr) {
        return VisitAction::WALK_CHILDREN;
    }
    if (ae.rightExpr->ty && ae.leftValue->ty && NeedBoxOption(*ae.rightExpr->ty, *ae.leftValue->ty)) {
        TryOptionBox(*StaticCast<EnumTy*>(ae.leftValue->ty), *ae.rightExpr);
    }
    return VisitAction::WALK_CHILDREN;
}

VisitAction AutoBoxing::AddOptionBoxHandleVarDecl(const VarDecl& vd)
{
    if (vd.initializer && vd.initializer->ty && vd.ty && NeedBoxOption(*vd.initializer->ty, *vd.ty)) {
        TryOptionBox(*StaticCast<EnumTy*>(vd.ty), *vd.initializer);
    }
    return VisitAction::WALK_CHILDREN;
}

VisitAction AutoBoxing::AddOptionBoxHandleReturnExpr(const ReturnExpr& re)
{
    if (re.expr && re.refFuncBody && re.refFuncBody->ty && re.refFuncBody->ty->kind == TypeKind::TYPE_FUNC) {
        auto funcTy = RawStaticCast<FuncTy*>(re.refFuncBody->ty);
        if (re.expr->ty && funcTy->retTy && NeedBoxOption(*re.expr->ty, *funcTy->retTy)) {
            auto expr = re.desugarExpr ? re.desugarExpr.get() : re.expr.get();
            TryOptionBox(*StaticCast<EnumTy*>(funcTy->retTy), *expr);
        }
    }
    return VisitAction::WALK_CHILDREN;
}
