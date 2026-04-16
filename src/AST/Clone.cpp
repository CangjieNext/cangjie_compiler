// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements AST clone apis.
 */

#include "cangjie/AST/Clone.h"

#include <memory>
#include <unordered_map>

#include "cangjie/AST/Create.h"
#include "cangjie/AST/Match.h"
#include "cangjie/AST/Nodes/FuncArg.h"
#include "cangjie/Basic/Match.h"
#include "cangjie/Basic/Print.h"

namespace Cangjie::AST {
using namespace Meta;

void DefaultVisitFunc(const Node& /* source */, const Node& /* target */)
{
}

void SetIsClonedSourceCode(const Node& /* source */, Node& target)
{
    target.EnableAttr(Attribute::IS_CLONED_SOURCE_CODE);
}

void CopyBasicInfo(Ptr<const Node> source, Ptr<Node> target)
{
    if (source && target) {
        target->begin = source->begin;
        target->end = source->end;
        target->curMacroCall = source->curMacroCall;
        target->isInMacroCall = source->isInMacroCall;
        target->comments = source->comments;
        CopyNodeScopeInfo(target, source);
    }
}

OwnedPtr<Generic> CloneGeneric(const Generic& generic, const VisitFunc& visitor)
{
    auto ret = MakeOwned<Generic>();
    for (auto& it : generic.typeParameters) {
        auto gpd = MakeOwned<GenericParamDecl>();
        auto d = ASTCloner::Clone(it.get(), visitor);
        gpd.reset(As<ASTKind::GENERIC_PARAM_DECL>(d.release()));
        ret->typeParameters.push_back(std::move(gpd));
    }
    for (auto& it : generic.genericConstraints) {
        ret->genericConstraints.push_back(ASTCloner::Clone(it.get(), visitor));
    }
    ret->leftAnglePos = generic.leftAnglePos;
    ret->rightAnglePos = generic.rightAnglePos;
    return ret;
}

MacroInvocation CloneMacroInvocation(const MacroInvocation& me)
{
    MacroInvocation mi;
    mi.fullName = me.fullName;
    mi.fullNameDotPos = me.fullNameDotPos;
    mi.identifier = me.identifier;
    mi.identifierPos = me.identifierPos;
    mi.leftSquarePos = me.leftSquarePos;
    mi.attrs = me.attrs;
    mi.rightSquarePos = me.rightSquarePos;
    mi.leftParenPos = me.leftParenPos;
    mi.args = me.args;
    mi.rightParenPos = me.rightParenPos;
    mi.newTokens = me.newTokens;
    mi.newTokensStr = me.newTokensStr;
    mi.parent = me.parent;
    mi.scope = me.scope;
    return mi;
}

namespace {
// This function should be called in each node which inherit Node directly.
void CopyNodeField(Ptr<Node> ret, const Node& e)
{
    ret->begin = e.begin;
    ret->end = e.end;
    ret->SetTy(e.GetTy());
    ret->curMacroCall = e.curMacroCall;
    ret->isInMacroCall = e.isInMacroCall;
    ret->comments = e.comments;
    CopyNodeScopeInfo(ret, &e);
    ret->CopyAttrs(e.GetAttrs());
}
} // namespace

struct ASTCloner::Impl {
    std::unordered_map<Ptr<Node>*, Ptr<Node>*> targetAddr2targetAddr;
    std::unordered_map<Ptr<Node>, Ptr<Node>> source2cloned;
    VisitFunc visitCallback;

    template <typename T> void targetAddrMapInsert(Ptr<T>& from, Ptr<T>& target)
    {
        if (from == nullptr) {
            return;
        }
        targetAddr2targetAddr[reinterpret_cast<Ptr<Node>*>(&from)] = reinterpret_cast<Ptr<Node>*>(&target);
    }

    OwnedPtr<Node> cloneWithRearrange(Ptr<Node> node, const VisitFunc& userVisitor);
    template <typename ExprT> OwnedPtr<ExprT> CloneExpr(Ptr<ExprT> expr);
    template <typename NodeT> OwnedPtr<NodeT> CloneNode(Ptr<NodeT> node);
    OwnedPtr<Type> CloneType(Ptr<Type> type);
    OwnedPtr<Decl> CloneDecl(Ptr<Decl> decl);
    OwnedPtr<Pattern> ClonePattern(Ptr<Pattern> pattern);
    OwnedPtr<QualifiedType> CloneQualifiedType(const QualifiedType& node);
    OwnedPtr<ParenType> CloneParenType(const ParenType& node);
    OwnedPtr<OptionType> CloneOptionType(const OptionType& node);
    OwnedPtr<FuncType> CloneFuncType(const FuncType& node);
    OwnedPtr<TupleType> CloneTupleType(const TupleType& node);
    OwnedPtr<ConstantType> CloneConstantType(const ConstantType& node);
    OwnedPtr<VArrayType> CloneVArrayType(const VArrayType& node);
    OwnedPtr<RefType> CloneRefType(const RefType& type);
    OwnedPtr<MacroExpandExpr> CloneMacroExpandExpr(const MacroExpandExpr& mee);
    OwnedPtr<TokenPart> CloneTokenPart(const TokenPart& tp);
    OwnedPtr<QuoteExpr> CloneQuoteExpr(const QuoteExpr& qe);
    OwnedPtr<IfExpr> CloneIfExpr(const IfExpr& ie);
    OwnedPtr<TryExpr> CloneTryExpr(const TryExpr& te);
    OwnedPtr<ThrowExpr> CloneThrowExpr(const ThrowExpr& te);
    OwnedPtr<PerformExpr> ClonePerformExpr(const PerformExpr& pe);
    OwnedPtr<ResumeExpr> CloneResumeExpr(const ResumeExpr& re);
    OwnedPtr<ReturnExpr> CloneReturnExpr(const ReturnExpr& re);
    OwnedPtr<WhileExpr> CloneWhileExpr(const WhileExpr& we);
    OwnedPtr<DoWhileExpr> CloneDoWhileExpr(const DoWhileExpr& dwe);
    OwnedPtr<AssignExpr> CloneAssignExpr(const AssignExpr& ae);
    OwnedPtr<IncOrDecExpr> CloneIncOrDecExpr(const IncOrDecExpr& ide);
    OwnedPtr<UnaryExpr> CloneUnaryExpr(const UnaryExpr& ue);
    OwnedPtr<BinaryExpr> CloneBinaryExpr(const BinaryExpr& be);
    OwnedPtr<RangeExpr> CloneRangeExpr(const RangeExpr& re);
    OwnedPtr<SubscriptExpr> CloneSubscriptExpr(const SubscriptExpr& se);
    OwnedPtr<MemberAccess> CloneMemberAccess(const MemberAccess& ma);
    OwnedPtr<CallExpr> CloneCallExpr(const CallExpr& ce);
    OwnedPtr<ParenExpr> CloneParenExpr(const ParenExpr& pe);
    OwnedPtr<LambdaExpr> CloneLambdaExpr(const LambdaExpr& le);
    OwnedPtr<LitConstExpr> CloneLitConstExpr(const LitConstExpr& lce);
    OwnedPtr<ArrayLit> CloneArrayLit(const ArrayLit& al);
    OwnedPtr<ArrayExpr> CloneArrayExpr(const ArrayExpr& ae);
    OwnedPtr<PointerExpr> ClonePointerExpr(const PointerExpr& ptre);
    OwnedPtr<TupleLit> CloneTupleLit(const TupleLit& tl);
    OwnedPtr<RefExpr> CloneRefExpr(const RefExpr& re);
    OwnedPtr<ForInExpr> CloneForInExpr(const ForInExpr& fie);
    OwnedPtr<MatchExpr> CloneMatchExpr(const MatchExpr& me);
    OwnedPtr<JumpExpr> CloneJumpExpr(const JumpExpr& je);
    OwnedPtr<TypeConvExpr> CloneTypeConvExpr(const TypeConvExpr& tce);
    OwnedPtr<SpawnExpr> CloneSpawnExpr(const SpawnExpr& se);
    OwnedPtr<SynchronizedExpr> CloneSynchronizedExpr(const SynchronizedExpr& se);
    OwnedPtr<InvalidExpr> CloneInvalidExpr(const InvalidExpr& ie);
    OwnedPtr<InterpolationExpr> CloneInterpolationExpr(const InterpolationExpr& ie);
    OwnedPtr<StrInterpolationExpr> CloneStrInterpolationExpr(const StrInterpolationExpr& sie);
    OwnedPtr<TrailingClosureExpr> CloneTrailingClosureExpr(const TrailingClosureExpr& tc);
    OwnedPtr<IsExpr> CloneIsExpr(const IsExpr& ie);
    OwnedPtr<AsExpr> CloneAsExpr(const AsExpr& ae);
    OwnedPtr<OptionalExpr> CloneOptionalExpr(const OptionalExpr& oe);
    OwnedPtr<OptionalChainExpr> CloneOptionalChainExpr(const OptionalChainExpr& oce);
    OwnedPtr<LetPatternDestructor> CloneLetPatternDestructor(const LetPatternDestructor& ldp);
    OwnedPtr<IfAvailableExpr> CloneIfAvailableExpr(const IfAvailableExpr& e);
    OwnedPtr<ConstPattern> CloneConstPattern(const ConstPattern& cp);
    OwnedPtr<VarPattern> CloneVarPattern(const VarPattern& vp);
    OwnedPtr<TuplePattern> CloneTuplePattern(const TuplePattern& tp);
    OwnedPtr<TypePattern> CloneTypePattern(const TypePattern& tp);
    OwnedPtr<EnumPattern> CloneEnumPattern(const EnumPattern& ep);
    OwnedPtr<ExceptTypePattern> CloneExceptTypePattern(const ExceptTypePattern& etp);
    OwnedPtr<CommandTypePattern> CloneCommandTypePattern(const CommandTypePattern& ctp);
    OwnedPtr<VarOrEnumPattern> CloneVarOrEnumPattern(const VarOrEnumPattern& vep);
    OwnedPtr<Block> CloneBlock(const Block& block);
    OwnedPtr<ClassBody> CloneClassBody(const ClassBody& cb);
    OwnedPtr<StructBody> CloneStructBody(const StructBody& sb);
    OwnedPtr<InterfaceBody> CloneInterfaceBody(const InterfaceBody& ib);
    OwnedPtr<GenericConstraint> CloneGenericConstraint(const GenericConstraint& gc);
    OwnedPtr<FuncBody> CloneFuncBody(const FuncBody& fb);
    OwnedPtr<VarDecl> CloneFuncParam(const FuncParam& fp);
    OwnedPtr<FuncParamList> CloneFuncParamList(const FuncParamList& fpl);
    OwnedPtr<FuncArg> CloneFuncArg(const FuncArg& fa);
    OwnedPtr<Annotation> CloneAnnotation(const Annotation& annotation);
    OwnedPtr<ImportSpec> CloneImportSpec(const ImportSpec& is);
    OwnedPtr<MatchCase> CloneMatchCase(const MatchCase& mc);
    OwnedPtr<MatchCaseOther> CloneMatchCaseOther(const MatchCaseOther& mco);
    OwnedPtr<Decl> CloneGenericParamDecl(const GenericParamDecl& gpd);
    OwnedPtr<Decl> CloneVarWithPatternDecl(const VarWithPatternDecl& vwpd);
    OwnedPtr<Decl> CloneVarDecl(const VarDecl& vd);
    OwnedPtr<Decl> CloneFuncDecl(const FuncDecl& fd);
    OwnedPtr<Decl> ClonePrimaryCtorDecl(const PrimaryCtorDecl& pcd);
    OwnedPtr<Decl> ClonePropDecl(const PropDecl& pd);
    OwnedPtr<Decl> CloneExtendDecl(const ExtendDecl& ed);
    OwnedPtr<Decl> CloneMacroExpandDecl(const MacroExpandDecl& med);
    OwnedPtr<Decl> CloneStructDecl(const StructDecl& sd);
    OwnedPtr<Decl> CloneClassDecl(const ClassDecl& cd);
    OwnedPtr<Decl> CloneInterfaceDecl(const InterfaceDecl& id);
    OwnedPtr<Decl> CloneEnumDecl(const EnumDecl& ed);
    OwnedPtr<Decl> CloneTypeAliasDecl(const TypeAliasDecl& tad);
};

// Clone nodes which inherit Node indirectly, should only be call by clone function which inherit Node directly.
OwnedPtr<Decl> ASTCloner::Impl::CloneGenericParamDecl(const GenericParamDecl& gpd)
{
    auto ret = MakeOwned<GenericParamDecl>();
    ret->outerDecl = gpd.outerDecl;
    ret->commaPos = gpd.commaPos;
    ret->EnableAttr(Attribute::COMPILER_ADD);
    return ret;
}

OwnedPtr<VarDecl> ASTCloner::Impl::CloneFuncParam(const FuncParam& fp)
{
    auto ret = MakeOwned<FuncParam>();
    ret->colonPos = fp.colonPos;
    ret->assignment = CloneExpr(fp.assignment.get());
    if (fp.desugarDecl) {
        auto decl = CloneDecl(fp.desugarDecl.get());
        ret->desugarDecl.reset(RawStaticCast<FuncDecl*>(decl.release()));
    }
    ret->isNamedParam = fp.isNamedParam;
    ret->isMemberParam = fp.isMemberParam;
    ret->commaPos = fp.commaPos;
    ret->notMarkPos = fp.notMarkPos;
    ret->EnableAttr(Attribute::COMPILER_ADD);
    return ret;
}

OwnedPtr<Decl> ASTCloner::Impl::CloneVarWithPatternDecl(const VarWithPatternDecl& vwpd)
{
    auto ret = MakeOwned<VarWithPatternDecl>();
    ret->type = CloneType(vwpd.type.get());
    ret->assignPos = vwpd.assignPos;
    ret->colonPos = vwpd.colonPos;
    ret->isVar = vwpd.isVar;
    ret->isConst = vwpd.isConst;
    ret->irrefutablePattern = ClonePattern(vwpd.irrefutablePattern.get());
    ret->initializer = CloneExpr(vwpd.initializer.get());
    return ret;
}

OwnedPtr<Decl> ASTCloner::Impl::CloneVarDecl(const VarDecl& vd)
{
    auto ret =
        match(vd)([this](const FuncParam& e) { return CloneFuncParam(e); }, []() { return MakeOwned<VarDecl>(); });
    // Clone field in VarDecl.
    ret->type = CloneType(vd.type.get());
    ret->colonPos = vd.colonPos;
    ret->initializer = CloneExpr(vd.initializer.get());
    ret->assignPos = vd.assignPos;
    ret->isVar = vd.isVar;
    ret->isConst = vd.isConst;
    ret->isResourceVar = vd.isResourceVar;
    ret->isIdentifierCompilerAdd = vd.isIdentifierCompilerAdd;
    ret->parentPattern = vd.parentPattern;
    ret->EnableAttr(Attribute::COMPILER_ADD);
    return ret;
}

OwnedPtr<Decl> ASTCloner::Impl::CloneFuncDecl(const FuncDecl& fd)
{
    auto ret = MakeOwned<FuncDecl>();
    ret->leftParenPos = fd.leftParenPos;
    ret->rightParenPos = fd.rightParenPos;
    CJC_NULLPTR_CHECK(fd.funcBody);
    ret->funcBody = CloneNode(fd.funcBody.get());
    ret->funcBody->funcDecl = ret.get(); // Reset funcDecl of funcBody to new function.
    ret->propDecl = fd.propDecl;
    ret->constructorCall = fd.constructorCall;
    ret->op = fd.op;
    ret->ownerFunc = fd.ownerFunc;
    ret->overflowStrategy = fd.overflowStrategy;
    ret->isFastNative = fd.isFastNative;
    ret->isGetter = fd.isGetter;
    ret->isSetter = fd.isSetter;
    ret->isInline = fd.isInline;
    ret->isConst = fd.isConst;
    ret->isFrozen = fd.isFrozen;
    ret->variadicArgIndex = fd.variadicArgIndex;
    ret->hasVariableLenArg = fd.hasVariableLenArg;
    ret->EnableAttr(Attribute::COMPILER_ADD);
    return ret;
}

OwnedPtr<Decl> ASTCloner::Impl::ClonePrimaryCtorDecl(const PrimaryCtorDecl& pcd)
{
    auto ret = MakeOwned<PrimaryCtorDecl>();
    CJC_NULLPTR_CHECK(pcd.funcBody);
    ret->funcBody = CloneNode(pcd.funcBody.get());
    ret->overflowStrategy = pcd.overflowStrategy;
    ret->isFastNative = pcd.isFastNative;
    ret->isConst = pcd.isConst;
    ret->EnableAttr(Attribute::COMPILER_ADD);
    return ret;
}

OwnedPtr<Decl> ASTCloner::Impl::ClonePropDecl(const PropDecl& pd)
{
    auto ret = MakeOwned<PropDecl>();
    ret->type = CloneType(pd.type.get());
    ret->colonPos = pd.colonPos;
    ret->leftCurlPos = pd.leftCurlPos;
    ret->rightCurlPos = pd.rightCurlPos;
    ret->isVar = pd.isVar;
    ret->isConst = pd.isConst;
    for (auto& func : pd.setters) {
        auto decl = CloneDecl(func.get());
        auto fd = MakeOwned<FuncDecl>();
        fd.reset(As<ASTKind::FUNC_DECL>(decl.release()));
        fd->EnableAttr(Attribute::COMPILER_ADD);
        ret->setters.push_back(std::move(fd));
    }
    for (auto& func : pd.getters) {
        auto decl = CloneDecl(func.get());
        auto fd = MakeOwned<FuncDecl>();
        fd.reset(As<ASTKind::FUNC_DECL>(decl.release()));
        fd->EnableAttr(Attribute::COMPILER_ADD);
        ret->getters.push_back(std::move(fd));
    }

    ret->EnableAttr(Attribute::COMPILER_ADD);
    return ret;
}

OwnedPtr<RefType> ASTCloner::Impl::CloneRefType(const RefType& type)
{
    auto ret = MakeOwned<RefType>();
    CopyNodeField(ret.get(), type);
    ret->ref = type.ref;
    ret->leftAnglePos = type.leftAnglePos;
    ret->typeArguments = std::vector<OwnedPtr<Type>>();
    for (auto& it : type.typeArguments) {
        ret->typeArguments.push_back(CloneType(it.get()));
    }
    ret->rightAnglePos = type.rightAnglePos;
    ret->commaPos = type.commaPos;
    ret->bitAndPos = type.bitAndPos;
    ret->EnableAttr(Attribute::COMPILER_ADD);
    return ret;
}

OwnedPtr<Decl> ASTCloner::Impl::CloneExtendDecl(const ExtendDecl& ed)
{
    auto ret = MakeOwned<ExtendDecl>();
    ret->extendedType = CloneType(ed.extendedType.get());
    for (auto& interface : ed.inheritedTypes) {
        ret->inheritedTypes.push_back(CloneType(interface.get()));
    }
    for (auto& func : ed.members) {
        ret->members.push_back(CloneDecl(func.get()));
    }
    if (ed.generic) {
        ret->generic = CloneGeneric(*ed.generic);
    }
    if (ed.bodyScope) {
        ret->bodyScope = MakeOwned<DummyBody>();
        CopyNodeScopeInfo(ret->bodyScope.get(), ed.bodyScope.get());
    }
    ret->EnableAttr(Attribute::COMPILER_ADD);
    return ret;
}

OwnedPtr<Decl> ASTCloner::Impl::CloneMacroExpandDecl(const MacroExpandDecl& med)
{
    auto ret = MakeOwned<MacroExpandDecl>();
    ret->invocation = CloneMacroInvocation(med.invocation);
    ret->EnableAttr(Attribute::COMPILER_ADD);
    return ret;
}

OwnedPtr<Decl> ASTCloner::Impl::CloneStructDecl(const StructDecl& sd)
{
    auto ret = MakeOwned<StructDecl>();
    for (auto& it : sd.inheritedTypes) {
        ret->inheritedTypes.push_back(CloneType(it.get()));
    }
    ret->body = CloneNode(sd.body.get());
    if (sd.generic) {
        ret->generic = CloneGeneric(*sd.generic);
    }
    ret->upperBoundPos = sd.upperBoundPos;
    ret->EnableAttr(Attribute::COMPILER_ADD);
    return ret;
}

OwnedPtr<Decl> ASTCloner::Impl::CloneClassDecl(const ClassDecl& cd)
{
    auto ret = MakeOwned<ClassDecl>();
    for (auto& it : cd.inheritedTypes) {
        ret->inheritedTypes.push_back(CloneType(it.get()));
    }
    ret->body = CloneNode(cd.body.get());
    if (cd.generic) {
        ret->generic = CloneGeneric(*cd.generic);
    }
    ret->upperBoundPos = cd.upperBoundPos;
    ret->EnableAttr(Attribute::COMPILER_ADD);
    return ret;
}

OwnedPtr<Decl> ASTCloner::Impl::CloneInterfaceDecl(const InterfaceDecl& id)
{
    auto ret = MakeOwned<InterfaceDecl>();

    for (auto& it : id.inheritedTypes) {
        ret->inheritedTypes.push_back(CloneType(it.get()));
    }
    ret->body = CloneNode(id.body.get());

    if (id.generic) {
        ret->generic = CloneGeneric(*id.generic);
    }
    ret->upperBoundPos = id.upperBoundPos;
    ret->EnableAttr(Attribute::COMPILER_ADD);
    return ret;
}

OwnedPtr<Decl> ASTCloner::Impl::CloneEnumDecl(const EnumDecl& ed)
{
    auto ret = MakeOwned<EnumDecl>();
    for (auto& it : ed.inheritedTypes) {
        ret->inheritedTypes.push_back(CloneType(it.get()));
    }
    for (auto& it : ed.constructors) {
        ret->constructors.push_back(CloneDecl(it.get()));
    }
    for (auto& func : ed.members) {
        ret->members.push_back(CloneDecl(func.get()));
    }
    if (ed.generic) {
        ret->generic = CloneGeneric(*ed.generic);
    }
    ret->hasArguments = ed.hasArguments;
    if (ed.bodyScope) {
        ret->bodyScope = MakeOwned<DummyBody>();
        CopyNodeScopeInfo(ret->bodyScope.get(), ed.bodyScope.get());
    }
    ret->leftCurlPos = ed.leftCurlPos;
    ret->bitOrPosVector = ed.bitOrPosVector;
    ret->rightCurlPos = ed.rightCurlPos;
    ret->upperBoundPos = ed.upperBoundPos;
    ret->EnableAttr(Attribute::COMPILER_ADD);
    return ret;
}

OwnedPtr<Decl> ASTCloner::Impl::CloneTypeAliasDecl(const TypeAliasDecl& tad)
{
    auto ret = MakeOwned<TypeAliasDecl>();
    ret->assignPos = tad.assignPos;
    ret->type = CloneType(tad.type.get());
    if (tad.generic != nullptr) {
        ret->generic = CloneGeneric(*tad.generic);
    }
    ret->EnableAttr(Attribute::COMPILER_ADD);
    return ret;
}

OwnedPtr<QualifiedType> ASTCloner::Impl::CloneQualifiedType(const QualifiedType& node)
{
    auto ret = MakeOwned<QualifiedType>();
    ret->baseType = CloneType(node.baseType.get());
    ret->field = node.field;
    ret->SetTarget(node.GetTarget());
    for (auto& it : node.typeArguments) {
        ret->typeArguments.push_back(CloneType(it.get()));
    }
    return ret;
}

OwnedPtr<ParenType> ASTCloner::Impl::CloneParenType(const ParenType& node)
{
    auto ret = MakeOwned<ParenType>();
    ret->type = CloneType(node.type.get());
    ret->leftParenPos = node.leftParenPos;
    ret->rightParenPos = node.rightParenPos;
    return ret;
}

OwnedPtr<OptionType> ASTCloner::Impl::CloneOptionType(const OptionType& node)
{
    auto ret = MakeOwned<OptionType>();
    ret->componentType = CloneType(node.componentType.get());
    ret->questNum = node.questNum;
    ret->questVector = node.questVector;
    if (node.desugarType != nullptr) {
        ret->desugarType = ASTCloner::Clone(node.desugarType.get(), visitCallback);
    }
    return ret;
}

OwnedPtr<FuncType> ASTCloner::Impl::CloneFuncType(const FuncType& node)
{
    auto ret = MakeOwned<FuncType>();
    for (auto& paramType : node.paramTypes) {
        ret->paramTypes.emplace_back(CloneType(paramType.get()));
    }
    ret->retType = CloneType(node.retType.get());
    ret->isC = node.isC;
    ret->leftParenPos = node.leftParenPos;
    ret->rightParenPos = node.rightParenPos;
    ret->arrowPos = node.arrowPos;
    return ret;
}

OwnedPtr<TupleType> ASTCloner::Impl::CloneTupleType(const TupleType& node)
{
    auto ret = MakeOwned<TupleType>();
    for (auto& it : node.fieldTypes) {
        ret->fieldTypes.push_back(CloneType(it.get()));
    }
    ret->leftParenPos = node.leftParenPos;
    ret->rightParenPos = node.rightParenPos;
    ret->commaPosVector = node.commaPosVector;
    return ret;
}

OwnedPtr<ConstantType> ASTCloner::Impl::CloneConstantType(const ConstantType& node)
{
    auto ret = MakeOwned<ConstantType>();
    ret->constantExpr = CloneExpr(node.constantExpr.get());
    ret->dollarPos = node.dollarPos;
    return ret;
}

OwnedPtr<VArrayType> ASTCloner::Impl::CloneVArrayType(const VArrayType& node)
{
    auto ret = MakeOwned<VArrayType>();
    ret->leftAnglePos = node.leftAnglePos;
    ret->typeArgument = CloneType(node.typeArgument.get());
    ret->constantType = CloneType(node.constantType.get());
    ret->rightAnglePos = node.rightAnglePos;
    return ret;
}

// Clone nodes which inherit Node directly.
OwnedPtr<Type> ASTCloner::Impl::CloneType(Ptr<Type> type)
{
    if (!type) {
        return OwnedPtr<Type>();
    }
    auto ret = match(*type)([this](const RefType& e) { return OwnedPtr<Type>(CloneRefType(e)); },
        [](const PrimitiveType& e) { return OwnedPtr<Type>(MakeOwned<PrimitiveType>(e)); },
        [this](const ParenType& e) { return OwnedPtr<Type>(CloneParenType(e)); },
        [this](const QualifiedType& e) { return OwnedPtr<Type>(CloneQualifiedType(e)); },
        [this](const OptionType& e) { return OwnedPtr<Type>(CloneOptionType(e)); },
        [this](const FuncType& e) { return OwnedPtr<Type>(CloneFuncType(e)); },
        [this](const TupleType& e) { return OwnedPtr<Type>(CloneTupleType(e)); },
        [this](const ConstantType& e) { return OwnedPtr<Type>(CloneConstantType(e)); },
        [this](const VArrayType& e) { return OwnedPtr<Type>(CloneVArrayType(e)); },
        [](const ThisType& e) { return OwnedPtr<Type>(MakeOwned<ThisType>(e)); },
        [](const InvalidType& e) { return OwnedPtr<Type>(MakeOwned<InvalidType>(e)); },
        [](const Type& e) { return OwnedPtr<Type>(MakeOwned<Type>(e)); },
        []() {
            // Invalid case.
            return MakeOwned<Type>();
        });
    CJC_ASSERT(ret && ret->astKind == type->astKind);
    ret->commaPos = type->commaPos;
    ret->bitAndPos = type->bitAndPos;

    ret->typeParameterName = type->typeParameterName;
    ret->typeParameterNameIsRawId = type->typeParameterNameIsRawId;
    ret->typePos = type->typePos;
    ret->colonPos = type->colonPos;

    CopyNodeField(ret.get(), *type);
    visitCallback(*type, *ret);
    ret->EnableAttr(Attribute::COMPILER_ADD);
    return ret;
}

OwnedPtr<MacroExpandExpr> ASTCloner::Impl::CloneMacroExpandExpr(const MacroExpandExpr& mee)
{
    auto expr = MakeOwned<MacroExpandExpr>();
    expr->identifier = mee.identifier;
    expr->invocation = CloneMacroInvocation(mee.invocation);
    for (auto& anno : mee.annotations) {
        expr->annotations.emplace_back(CloneNode(anno.get()));
    }
    expr->modifiers.insert(mee.modifiers.begin(), mee.modifiers.end());
    return expr;
}

OwnedPtr<TokenPart> ASTCloner::Impl::CloneTokenPart(const TokenPart& tp)
{
    auto expr = MakeOwned<TokenPart>();
    expr->tokens.assign(tp.tokens.begin(), tp.tokens.end());
    return expr;
}

OwnedPtr<QuoteExpr> ASTCloner::Impl::CloneQuoteExpr(const QuoteExpr& qe)
{
    auto expr = MakeOwned<QuoteExpr>();
    for (auto& i : qe.exprs) {
        expr->exprs.push_back(CloneExpr(i.get()));
    }
    expr->quotePos = qe.quotePos;
    expr->leftParenPos = qe.leftParenPos;
    expr->rightParenPos = qe.rightParenPos;
    return expr;
}

OwnedPtr<IfExpr> ASTCloner::Impl::CloneIfExpr(const IfExpr& ie)
{
    auto expr = MakeOwned<IfExpr>();
    expr->ifPos = ie.ifPos;
    expr->leftParenPos = ie.leftParenPos;
    expr->condExpr = CloneExpr(ie.condExpr.get());
    expr->rightParenPos = ie.rightParenPos;
    auto n = CloneExpr(ie.thenBody.get());
    expr->thenBody.reset(As<ASTKind::BLOCK>(n.release()));
    expr->hasElse = ie.hasElse;
    expr->isElseIf = ie.isElseIf;
    expr->elsePos = ie.elsePos;
    expr->elseBody = CloneExpr(ie.elseBody.get());
    return expr;
}

OwnedPtr<TryExpr> ASTCloner::Impl::CloneTryExpr(const TryExpr& te)
{
    auto expr = MakeOwned<TryExpr>();
    expr->tryPos = te.tryPos;
    expr->lParen = te.lParen;
    for (auto& resource : te.resourceSpec) {
        expr->resourceSpec.push_back(OwnedPtr<VarDecl>(As<ASTKind::VAR_DECL>(CloneDecl(resource.get()).release())));
    }
    expr->isDesugaredFromSyncBlock = te.isDesugaredFromSyncBlock;
    expr->isDesugaredFromTryWithResources = te.isDesugaredFromTryWithResources;
    expr->isIllegalResourceSpec = te.isIllegalResourceSpec;
    expr->tryBlock = CloneExpr(te.tryBlock.get());
    for (size_t i = 0; i < te.catchPosVector.size(); i++) {
        expr->catchPosVector.push_back(te.catchPosVector[i]);
    }
    CJC_ASSERT(te.catchBlocks.size() == te.catchPatterns.size());
    for (size_t i = 0; i < te.catchBlocks.size(); i++) {
        expr->catchBlocks.push_back(CloneExpr(te.catchBlocks[i].get()));
        expr->catchPatterns.push_back(ClonePattern(te.catchPatterns[i].get()));
    }

    for (const auto& handler : te.handlers) {
        auto cloned = Handler();
        cloned.pos = handler.pos;
        cloned.commandPattern = ClonePattern(handler.commandPattern.get());
        cloned.block = CloneExpr(handler.block.get());
        if (handler.desugaredLambda) {
            cloned.desugaredLambda = CloneLambdaExpr(*handler.desugaredLambda);
            CopyNodeField(cloned.desugaredLambda, *handler.desugaredLambda);
        }
        expr->handlers.emplace_back(std::move(cloned));
    }
    expr->finallyPos = te.finallyPos;
    expr->finallyBlock = CloneExpr(te.finallyBlock.get());
    expr->resourceSpecCommaPos = te.resourceSpecCommaPos;
    expr->rParen = te.rParen;
    expr->catchLParenPosVector = te.catchLParenPosVector;
    expr->catchRParenPosVector = te.catchRParenPosVector;
    if (te.tryLambda) {
        expr->tryLambda = CloneExpr(te.tryLambda.get());
        CopyNodeField(expr->tryLambda, *te.tryLambda);
    }
    if (te.finallyLambda) {
        expr->finallyLambda = CloneExpr(te.finallyLambda.get());
        CopyNodeField(expr->finallyLambda, *te.finallyLambda);
    }
    return expr;
}

OwnedPtr<ThrowExpr> ASTCloner::Impl::CloneThrowExpr(const ThrowExpr& te)
{
    auto expr = MakeOwned<ThrowExpr>();
    expr->throwPos = te.throwPos;
    expr->expr = CloneExpr(te.expr.get());
    return expr;
}

OwnedPtr<PerformExpr> ASTCloner::Impl::ClonePerformExpr(const PerformExpr& pe)
{
    auto expr = MakeOwned<PerformExpr>();
    expr->performPos = pe.performPos;
    expr->expr = CloneExpr(pe.expr.get());
    return expr;
}

OwnedPtr<ResumeExpr> ASTCloner::Impl::CloneResumeExpr(const ResumeExpr& re)
{
    auto expr = MakeOwned<ResumeExpr>();
    expr->resumePos = re.resumePos;
    expr->withPos = re.withPos;
    expr->withExpr = CloneExpr(re.withExpr.get());
    expr->throwingPos = re.throwingPos;
    expr->throwingExpr = CloneExpr(re.throwingExpr.get());
    return expr;
}

OwnedPtr<ReturnExpr> ASTCloner::Impl::CloneReturnExpr(const ReturnExpr& re)
{
    auto expr = MakeOwned<ReturnExpr>();
    expr->returnPos = re.returnPos;
    expr->expr = CloneExpr(re.expr.get());
    expr->refFuncBody = re.refFuncBody;
    return expr;
}

OwnedPtr<WhileExpr> ASTCloner::Impl::CloneWhileExpr(const WhileExpr& we)
{
    auto expr = MakeOwned<WhileExpr>();
    expr->whilePos = we.whilePos;
    expr->leftParenPos = we.leftParenPos;
    expr->condExpr = CloneExpr(we.condExpr.get());
    expr->body = CloneExpr(we.body.get());
    expr->rightParenPos = we.rightParenPos;
    return expr;
}

OwnedPtr<DoWhileExpr> ASTCloner::Impl::CloneDoWhileExpr(const DoWhileExpr& dwe)
{
    auto expr = MakeOwned<DoWhileExpr>();
    expr->doPos = dwe.doPos;
    expr->body = CloneExpr(dwe.body.get());
    expr->whilePos = dwe.whilePos;
    expr->leftParenPos = dwe.leftParenPos;
    expr->condExpr = CloneExpr(dwe.condExpr.get());
    expr->rightParenPos = dwe.rightParenPos;
    return expr;
}

OwnedPtr<AssignExpr> ASTCloner::Impl::CloneAssignExpr(const AssignExpr& ae)
{
    auto expr = MakeOwned<AssignExpr>();
    expr->leftValue = CloneExpr(ae.leftValue.get());
    expr->op = ae.op;
    expr->rightExpr = CloneExpr(ae.rightExpr.get());
    expr->isCompound = ae.isCompound;
    expr->assignPos = ae.assignPos;
    return expr;
}

OwnedPtr<IncOrDecExpr> ASTCloner::Impl::CloneIncOrDecExpr(const IncOrDecExpr& ide)
{
    auto expr = MakeOwned<IncOrDecExpr>();
    expr->op = ide.op;
    expr->operatorPos = ide.operatorPos;
    expr->expr = CloneExpr(ide.expr.get());
    return expr;
}

OwnedPtr<UnaryExpr> ASTCloner::Impl::CloneUnaryExpr(const UnaryExpr& ue)
{
    auto expr = MakeOwned<UnaryExpr>();
    expr->op = ue.op;
    expr->expr = CloneExpr(ue.expr.get());
    expr->operatorPos = ue.operatorPos;
    return expr;
}

OwnedPtr<BinaryExpr> ASTCloner::Impl::CloneBinaryExpr(const BinaryExpr& be)
{
    auto expr = MakeOwned<BinaryExpr>();
    expr->op = be.op;
    expr->leftExpr = CloneExpr(be.leftExpr.get());
    expr->rightExpr = CloneExpr(be.rightExpr.get());
    expr->operatorPos = be.operatorPos;
    return expr;
}

OwnedPtr<RangeExpr> ASTCloner::Impl::CloneRangeExpr(const RangeExpr& re)
{
    auto expr = MakeOwned<RangeExpr>();
    expr->startExpr = CloneExpr(re.startExpr.get());
    expr->rangePos = re.rangePos;
    expr->stopExpr = CloneExpr(re.stopExpr.get());
    expr->colonPos = re.colonPos;
    expr->stepExpr = CloneExpr(re.stepExpr.get());
    expr->isClosed = re.isClosed;
    expr->decl = re.decl;
    return expr;
}

OwnedPtr<SubscriptExpr> ASTCloner::Impl::CloneSubscriptExpr(const SubscriptExpr& se)
{
    auto expr = MakeOwned<SubscriptExpr>();
    expr->baseExpr = CloneExpr(se.baseExpr.get());
    expr->leftParenPos = se.leftParenPos;
    for (auto& it : se.indexExprs) {
        expr->indexExprs.push_back(CloneExpr(it.get()));
    }
    expr->commaPos = se.commaPos;
    expr->rightParenPos = se.rightParenPos;
    expr->isTupleAccess = se.isTupleAccess;
    return expr;
}

OwnedPtr<MemberAccess> ASTCloner::Impl::CloneMemberAccess(const MemberAccess& ma)
{
    auto expr = MakeOwned<MemberAccess>();
    expr->baseExpr = CloneExpr(ma.baseExpr.get());
    expr->dotPos = ma.dotPos;
    expr->field = ma.field;
    expr->SetTarget(ma.GetTarget());
    expr->SetTargets(ma.GetTargets());
    expr->isPattern = ma.isPattern;
    expr->isAlone = ma.isAlone;
    expr->instTys = ma.instTys;
    expr->matchedParentTy = ma.matchedParentTy;
    expr->callOrPattern = ma.callOrPattern;
    expr->foundUpperBoundMap = ma.foundUpperBoundMap;
    expr->isExposedAccess = ma.isExposedAccess;
    for (auto& it : ma.typeArguments) {
        expr->typeArguments.push_back(CloneType(it.get()));
    }
    return expr;
}

OwnedPtr<CallExpr> ASTCloner::Impl::CloneCallExpr(const CallExpr& ce)
{
    auto expr = MakeOwned<CallExpr>();
    expr->baseFunc = CloneExpr(ce.baseFunc.get());
    expr->leftParenPos = ce.leftParenPos;
    expr->needCheckToTokens = ce.needCheckToTokens;
    std::unordered_map<Ptr<FuncArg>, Ptr<FuncArg>> cloneTable; // For replace desugarArgs
    for (auto& it : ce.args) {
        expr->args.push_back(CloneNode(it.get()));
        cloneTable[it.get()] = expr->args.back().get();
    }
    if (ce.desugarArgs.has_value()) {
        expr->defaultArgs = std::vector<OwnedPtr<FuncArg>>();
        for (auto& it : ce.defaultArgs) {
            expr->defaultArgs.push_back(CloneNode(it.get()));
            cloneTable[it.get()] = expr->defaultArgs.back().get();
        }
        expr->desugarArgs = std::vector<Ptr<FuncArg>>();
        for (auto& it : ce.desugarArgs.value()) {
            expr->desugarArgs.value().push_back(cloneTable[it]);
        }
    }
    expr->rightParenPos = ce.rightParenPos;
    expr->resolvedFunction = ce.resolvedFunction;
    expr->callKind = ce.callKind;
    expr->sugarKind = ce.sugarKind;
    if (auto ma = DynamicCast<MemberAccess*>(expr->baseFunc.get()); ma) {
        ma->callOrPattern = expr.get();
    }
    return expr;
}

OwnedPtr<ParenExpr> ASTCloner::Impl::CloneParenExpr(const ParenExpr& pe)
{
    auto expr = MakeOwned<ParenExpr>();
    expr->leftParenPos = pe.leftParenPos;
    expr->expr = CloneExpr(pe.expr.get());
    expr->rightParenPos = pe.rightParenPos;
    return expr;
}

OwnedPtr<LambdaExpr> ASTCloner::Impl::CloneLambdaExpr(const LambdaExpr& le)
{
    auto expr = MakeOwned<LambdaExpr>(CloneNode(le.funcBody.get()));
    if (le.TestAttr(Attribute::MOCK_SUPPORTED)) {
        expr->EnableAttr(Attribute::MOCK_SUPPORTED);
    }
    return expr;
}

OwnedPtr<LitConstExpr> ASTCloner::Impl::CloneLitConstExpr(const LitConstExpr& lce)
{
    auto expr = MakeOwned<LitConstExpr>(lce.kind, lce.stringValue);
    expr->codepoint = lce.codepoint;
    expr->delimiterNum = lce.delimiterNum;
    expr->stringKind = lce.stringKind;
    expr->rawString = lce.rawString;
    if (lce.ref) {
        expr->ref = CloneRefType(*lce.ref);
        visitCallback(*lce.ref, *expr->ref);
    }
    if (lce.siExpr) {
        expr->siExpr = CloneExpr(lce.siExpr.get());
    }
    return expr;
}

OwnedPtr<InterpolationExpr> ASTCloner::Impl::CloneInterpolationExpr(const InterpolationExpr& ie)
{
    auto expr = MakeOwned<InterpolationExpr>();
    expr->rawString = ie.rawString;
    expr->dollarPos = ie.dollarPos;
    expr->block = CloneExpr(ie.block.get());
    return expr;
}

OwnedPtr<StrInterpolationExpr> ASTCloner::Impl::CloneStrInterpolationExpr(const StrInterpolationExpr& sie)
{
    auto expr = MakeOwned<StrInterpolationExpr>();
    expr->rawString = sie.rawString;
    expr->strParts = sie.strParts;
    for (auto& it : sie.strPartExprs) {
        expr->strPartExprs.push_back(CloneExpr(it.get()));
    }
    return expr;
}

OwnedPtr<ArrayLit> ASTCloner::Impl::CloneArrayLit(const ArrayLit& al)
{
    auto expr = MakeOwned<ArrayLit>();
    expr->leftSquarePos = al.leftSquarePos;
    for (auto& it : al.children) {
        expr->children.push_back(CloneExpr(it.get()));
    }
    expr->commaPosVector = al.commaPosVector;
    expr->rightSquarePos = al.rightSquarePos;
    expr->initFunc = al.initFunc;
    return expr;
}

OwnedPtr<ArrayExpr> ASTCloner::Impl::CloneArrayExpr(const ArrayExpr& ae)
{
    auto expr = MakeOwned<ArrayExpr>();
    expr->type = CloneType(ae.type.get());
    expr->leftParenPos = ae.leftParenPos;
    expr->args.resize(ae.args.size());
    for (size_t i = 0; i < ae.args.size(); ++i) {
        expr->args[i] = CloneNode(ae.args[i].get());
    }
    expr->commaPosVector = ae.commaPosVector;
    expr->rightParenPos = ae.rightParenPos;
    expr->initFunc = ae.initFunc;
    expr->isValueArray = ae.isValueArray;
    return expr;
}

OwnedPtr<PointerExpr> ASTCloner::Impl::ClonePointerExpr(const PointerExpr& ptre)
{
    auto expr = MakeOwned<PointerExpr>();
    expr->type = CloneType(ptre.type.get());
    expr->arg = CloneNode(ptre.arg.get());
    return expr;
}

OwnedPtr<TupleLit> ASTCloner::Impl::CloneTupleLit(const TupleLit& tl)
{
    auto expr = MakeOwned<TupleLit>();
    expr->leftParenPos = tl.leftParenPos;
    for (auto& it : tl.children) {
        expr->children.push_back(CloneExpr(it.get()));
    }
    expr->commaPosVector = tl.commaPosVector;
    expr->rightParenPos = tl.rightParenPos;
    return expr;
}

OwnedPtr<RefExpr> ASTCloner::Impl::CloneRefExpr(const RefExpr& re)
{
    auto expr = CreateRefExpr(re.ref.identifier);
    expr->leftAnglePos = re.leftAnglePos;
    expr->SetTarget(re.GetTarget());
    expr->SetTargets(re.GetTargets());
    for (auto& it : re.typeArguments) {
        expr->typeArguments.push_back(CloneType(it.get()));
    }
    expr->rightAnglePos = re.rightAnglePos;
    expr->isSuper = re.isSuper;
    expr->isThis = re.isThis;
    expr->isAlone = re.isAlone;
    expr->instTys = re.instTys;
    expr->matchedParentTy = re.matchedParentTy;
    expr->callOrPattern = re.callOrPattern;
    return expr;
}

OwnedPtr<ForInExpr> ASTCloner::Impl::CloneForInExpr(const ForInExpr& fie)
{
    auto expr = MakeOwned<ForInExpr>();
    expr->leftParenPos = fie.leftParenPos;
    expr->rightParenPos = fie.rightParenPos;
    expr->pattern = ClonePattern(fie.pattern.get());
    expr->inPos = fie.inPos;
    expr->inExpression = CloneExpr(fie.inExpression.get());
    expr->wherePos = fie.wherePos;
    expr->patternGuard = CloneExpr(fie.patternGuard.get());
    expr->patternInDesugarExpr = fie.patternInDesugarExpr;
    expr->body = CloneExpr(fie.body.get());
    expr->forInKind = fie.forInKind;
    return expr;
}

OwnedPtr<MatchExpr> ASTCloner::Impl::CloneMatchExpr(const MatchExpr& me)
{
    auto expr = MakeOwned<MatchExpr>();
    expr->matchMode = me.matchMode;
    expr->sugarKind = me.sugarKind;
    expr->selector = CloneExpr(me.selector.get());
    for (auto& i : me.matchCases) {
        expr->matchCases.push_back(CloneNode(i.get()));
    }
    for (auto& i : me.matchCaseOthers) {
        expr->matchCaseOthers.push_back(CloneNode(i.get()));
    }
    expr->leftParenPos = me.leftParenPos;
    expr->rightParenPos = me.rightParenPos;
    expr->leftCurlPos = me.leftCurlPos;
    expr->rightCurlPos = me.rightCurlPos;
    return expr;
}

OwnedPtr<JumpExpr> ASTCloner::Impl::CloneJumpExpr(const JumpExpr& je)
{
    auto expr = MakeOwned<JumpExpr>();
    expr->isBreak = je.isBreak;
    expr->refLoop = je.refLoop;
    return expr;
}

OwnedPtr<TypeConvExpr> ASTCloner::Impl::CloneTypeConvExpr(const TypeConvExpr& tce)
{
    auto expr = MakeOwned<TypeConvExpr>();
    expr->type = OwnedPtr<Type>(CloneType(tce.type.get()).release());
    expr->expr = CloneExpr(tce.expr.get());
    expr->leftParenPos = tce.leftParenPos;
    expr->rightParenPos = tce.rightParenPos;
    return expr;
}

OwnedPtr<SpawnExpr> ASTCloner::Impl::CloneSpawnExpr(const SpawnExpr& se)
{
    auto expr = MakeOwned<SpawnExpr>();
    expr->task = CloneExpr(se.task.get());
    expr->arg = CloneExpr(se.arg.get());
    if (se.futureObj) {
        auto obj = CloneDecl(se.futureObj.get());
        expr->futureObj = OwnedPtr<VarDecl>(As<ASTKind::VAR_DECL>(obj.release()));
    }
    expr->spawnPos = se.spawnPos;
    expr->leftParenPos = se.leftParenPos;
    expr->rightParenPos = se.rightParenPos;
    return expr;
}

OwnedPtr<SynchronizedExpr> ASTCloner::Impl::CloneSynchronizedExpr(const SynchronizedExpr& se)
{
    auto expr = MakeOwned<SynchronizedExpr>();
    expr->mutex = CloneExpr(se.mutex.get());
    expr->body = CloneExpr(se.body.get());
    expr->syncPos = se.syncPos;
    expr->leftParenPos = se.leftParenPos;
    expr->rightParenPos = se.rightParenPos;
    return expr;
}

OwnedPtr<InvalidExpr> ASTCloner::Impl::CloneInvalidExpr(const InvalidExpr& ie)
{
    auto expr = MakeOwned<InvalidExpr>(ie.begin);
    expr->value = ie.value;
    return expr;
}

OwnedPtr<TrailingClosureExpr> ASTCloner::Impl::CloneTrailingClosureExpr(const TrailingClosureExpr& tc)
{
    auto expr = MakeOwned<TrailingClosureExpr>();
    expr->leftLambda = tc.leftLambda;
    expr->rightLambda = tc.rightLambda;
    if (tc.expr) {
        expr->expr = CloneExpr(tc.expr.get());
    }
    if (tc.lambda) {
        expr->lambda = CloneExpr(tc.lambda.get());
    }
    return expr;
}

OwnedPtr<IsExpr> ASTCloner::Impl::CloneIsExpr(const IsExpr& ie)
{
    auto expr = MakeOwned<IsExpr>();
    expr->leftExpr = CloneExpr(ie.leftExpr.get());
    expr->isType = CloneType(ie.isType.get());
    expr->isPos = ie.isPos;
    return expr;
}

OwnedPtr<AsExpr> ASTCloner::Impl::CloneAsExpr(const AsExpr& ae)
{
    auto expr = MakeOwned<AsExpr>();
    expr->leftExpr = CloneExpr(ae.leftExpr.get());
    expr->asType = CloneType(ae.asType.get());
    expr->asPos = ae.asPos;
    return expr;
}

OwnedPtr<OptionalExpr> ASTCloner::Impl::CloneOptionalExpr(const OptionalExpr& oe)
{
    auto expr = MakeOwned<OptionalExpr>();
    expr->baseExpr = CloneExpr(oe.baseExpr.get());
    expr->questPos = oe.questPos;
    return expr;
}

OwnedPtr<OptionalChainExpr> ASTCloner::Impl::CloneOptionalChainExpr(const OptionalChainExpr& oce)
{
    auto expr = MakeOwned<OptionalChainExpr>();
    expr->expr = CloneExpr(oce.expr.get());
    return expr;
}

OwnedPtr<LetPatternDestructor> ASTCloner::Impl::CloneLetPatternDestructor(const LetPatternDestructor& ldp)
{
    auto expr = MakeOwned<LetPatternDestructor>();
    expr->backarrowPos = ldp.backarrowPos;
    for (auto& p : ldp.patterns) {
        expr->patterns.push_back(ClonePattern(p.get()));
    }
    expr->orPos = ldp.orPos;
    expr->initializer = CloneExpr(ldp.initializer.get());
    return expr;
}

OwnedPtr<IfAvailableExpr> ASTCloner::Impl::CloneIfAvailableExpr(const IfAvailableExpr& e)
{
    auto arg = CloneFuncArg(*e.GetArg());
    auto lambda1 = CloneLambdaExpr(*e.GetLambda1());
    auto lambda2 = CloneLambdaExpr(*e.GetLambda2());
    auto expr = MakeOwned<IfAvailableExpr>(std::move(arg), std::move(lambda1), std::move(lambda2));
    return expr;
}

/**
 * NOTE: To guarantee the members of Expr is copied, the sub method should not be called outside 'CloneExpr'.
 */
template <typename ExprT> OwnedPtr<ExprT> ASTCloner::Impl::CloneExpr(Ptr<ExprT> expr)
{
    if (!expr) {
        return OwnedPtr<ExprT>();
    }
    auto clonedExpr = match(*expr)(
        // PrimitiveExpr, AdjointExpr are ignored.
        [this](const IfExpr& ie) { return OwnedPtr<Expr>(CloneIfExpr(ie)); },
        [](const PrimitiveTypeExpr& pte) { return OwnedPtr<Expr>(MakeOwned<PrimitiveTypeExpr>(pte.typeKind)); },
        [this](const MacroExpandExpr& mee) { return OwnedPtr<Expr>(CloneMacroExpandExpr(mee)); },
        [this](const TokenPart& tp) { return OwnedPtr<Expr>(CloneTokenPart(tp)); },
        [this](const QuoteExpr& qe) { return OwnedPtr<Expr>(CloneQuoteExpr(qe)); },
        [this](const TryExpr& te) { return OwnedPtr<Expr>(CloneTryExpr(te)); },
        [this](const ThrowExpr& te) { return OwnedPtr<Expr>(CloneThrowExpr(te)); },
        [this](const PerformExpr& te) { return OwnedPtr<Expr>(ClonePerformExpr(te)); },
        [this](const ResumeExpr& re) { return OwnedPtr<Expr>(CloneResumeExpr(re)); },
        [this](const ReturnExpr& re) { return OwnedPtr<Expr>(CloneReturnExpr(re)); },
        [this](const WhileExpr& we) { return OwnedPtr<Expr>(CloneWhileExpr(we)); },
        [this](const DoWhileExpr& dwe) { return OwnedPtr<Expr>(CloneDoWhileExpr(dwe)); },
        [this](const AssignExpr& ae) { return OwnedPtr<Expr>(CloneAssignExpr(ae)); },
        [this](const IncOrDecExpr& ide) { return OwnedPtr<Expr>(CloneIncOrDecExpr(ide)); },
        [this](const UnaryExpr& ue) { return OwnedPtr<Expr>(CloneUnaryExpr(ue)); },
        [this](const BinaryExpr& be) { return OwnedPtr<Expr>(CloneBinaryExpr(be)); },
        [this](const RangeExpr& re) { return OwnedPtr<Expr>(CloneRangeExpr(re)); },
        [this](const SubscriptExpr& se) { return OwnedPtr<Expr>(CloneSubscriptExpr(se)); },
        [this](const MemberAccess& ma) { return OwnedPtr<Expr>(CloneMemberAccess(ma)); },
        [this](const CallExpr& ce) { return OwnedPtr<Expr>(CloneCallExpr(ce)); },
        [this](const ParenExpr& pe) { return OwnedPtr<Expr>(CloneParenExpr(pe)); },
        [this](const LambdaExpr& le) { return OwnedPtr<Expr>(CloneLambdaExpr(le)); },
        [this](const LitConstExpr& lce) { return OwnedPtr<Expr>(CloneLitConstExpr(lce)); },
        [this](const ArrayLit& al) { return OwnedPtr<Expr>(CloneArrayLit(al)); },
        [this](const ArrayExpr& asl) { return OwnedPtr<Expr>(CloneArrayExpr(asl)); },
        [this](const PointerExpr& ptre) { return OwnedPtr<Expr>(ClonePointerExpr(ptre)); },
        [this](const TupleLit& tl) { return OwnedPtr<Expr>(CloneTupleLit(tl)); },
        [this](const RefExpr& re) { return OwnedPtr<Expr>(CloneRefExpr(re)); },
        [this](const ForInExpr& fie) { return OwnedPtr<Expr>(CloneForInExpr(fie)); },
        [this](const MatchExpr& me) { return OwnedPtr<Expr>(CloneMatchExpr(me)); },
        [this](const JumpExpr& je) { return OwnedPtr<Expr>(CloneJumpExpr(je)); },
        [this](const TypeConvExpr& e) { return OwnedPtr<Expr>(CloneTypeConvExpr(e)); },
        [this](const SpawnExpr& se) { return OwnedPtr<Expr>(CloneSpawnExpr(se)); },
        [this](const SynchronizedExpr& se) { return OwnedPtr<Expr>(CloneSynchronizedExpr(se)); },
        [this](const InvalidExpr& ie) { return OwnedPtr<Expr>(CloneInvalidExpr(ie)); },
        [this](const Block& b) { return OwnedPtr<Expr>(CloneBlock(b)); },
        [this](const InterpolationExpr& ie) { return OwnedPtr<Expr>(CloneInterpolationExpr(ie)); },
        [this](const StrInterpolationExpr& sie) { return OwnedPtr<Expr>(CloneStrInterpolationExpr(sie)); },
        [this](const TrailingClosureExpr& tc) { return OwnedPtr<Expr>(CloneTrailingClosureExpr(tc)); },
        [this](const IsExpr& ie) { return OwnedPtr<Expr>(CloneIsExpr(ie)); },
        [this](const AsExpr& ae) { return OwnedPtr<Expr>(CloneAsExpr(ae)); },
        [this](const OptionalExpr& oe) { return OwnedPtr<Expr>(CloneOptionalExpr(oe)); },
        [this](const OptionalChainExpr& oe) { return OwnedPtr<Expr>(CloneOptionalChainExpr(oe)); },
        [](const WildcardExpr& /* we */) { return OwnedPtr<Expr>(MakeOwned<WildcardExpr>()); },
        [this](const LetPatternDestructor& ld) { return OwnedPtr<Expr>(CloneLetPatternDestructor(ld)); },
        [this](const IfAvailableExpr& ie) { return OwnedPtr<Expr>(CloneIfAvailableExpr(ie)); },
        [&expr]() {
            // Invalid and ignored cases.
            auto invalidExpr = MakeOwned<InvalidExpr>(expr->begin);
            return OwnedPtr<Expr>(invalidExpr.release());
        });
    CJC_ASSERT(clonedExpr);
    if (clonedExpr->astKind != ASTKind::INVALID_EXPR) {
        CopyNodeField(clonedExpr.get(), *expr);
        // Clone field in Expr.
        clonedExpr->isConst = expr->isConst;
        clonedExpr->isBaseFunc = expr->isBaseFunc;
        clonedExpr->isInFlowExpr = expr->isInFlowExpr;
        clonedExpr->constNumValue = expr->constNumValue;
        clonedExpr->hasSemi = expr->hasSemi;
        clonedExpr->mapExpr = (expr->mapExpr == expr) ? clonedExpr.get() : expr->mapExpr;
        clonedExpr->sourceExpr = expr->sourceExpr;
        clonedExpr->overflowStrategy = expr->overflowStrategy;
        if (expr->desugarExpr && !clonedExpr->desugarExpr) {
            clonedExpr->desugarExpr = CloneExpr(expr->desugarExpr.get());
        }
        clonedExpr->EnableAttr(Attribute::COMPILER_ADD);
        visitCallback(*expr, *clonedExpr);
    }
    auto result = DynamicCast<ExprT*>(clonedExpr.release());
    CJC_NULLPTR_CHECK(result);
    return OwnedPtr<ExprT>(result);
}

/**
 * NOTE: To guarantee the members of Decl is copied, the sub method should not be called outside 'CloneDecl'.
 */
OwnedPtr<Decl> ASTCloner::Impl::CloneDecl(Ptr<Decl> decl)
{
    if (!decl) {
        return OwnedPtr<Decl>();
    }
    auto ret = match(*decl)([this](const GenericParamDecl& e) { return CloneGenericParamDecl(e); },
        [this](const PropDecl& e) { return ClonePropDecl(e); }, [this](const VarDecl& e) { return CloneVarDecl(e); },
        [this](const FuncDecl& e) { return CloneFuncDecl(e); },
        [this](const StructDecl& e) { return CloneStructDecl(e); },
        [this](const ClassDecl& e) { return CloneClassDecl(e); },
        [this](const InterfaceDecl& e) { return CloneInterfaceDecl(e); },
        [this](const EnumDecl& e) { return CloneEnumDecl(e); },
        [this](const PrimaryCtorDecl& e) { return ClonePrimaryCtorDecl(e); },
        [this](const ExtendDecl& e) { return CloneExtendDecl(e); },
        [this](const MacroExpandDecl& e) { return CloneMacroExpandDecl(e); },
        [this](const VarWithPatternDecl& e) { return CloneVarWithPatternDecl(e); },
        [this](const TypeAliasDecl& e) { return CloneTypeAliasDecl(e); },
        // MainDecl and MacroDecl are ignored. Since they will be desugared before semantic typecheck.
        [](const MacroDecl& e) { return OwnedPtr<Decl>(MakeOwned<InvalidDecl>(e.begin).release()); },
        [](const MainDecl& e) { return OwnedPtr<Decl>(MakeOwned<InvalidDecl>(e.begin).release()); },
        [](const InvalidDecl& e) { return OwnedPtr<Decl>(MakeOwned<InvalidDecl>(e.begin).release()); },
        [&decl]() {
            // Invalid and ignored cases.
            auto invalidDecl = MakeOwned<InvalidDecl>(decl->begin);
            return OwnedPtr<Decl>(invalidDecl.release());
        });
    CJC_ASSERT(ret);
    CopyNodeField(ret.get(), *decl);
    // Clone field in Decl.
    ret->modifiers.insert(decl->modifiers.begin(), decl->modifiers.end());
    ret->identifier = decl->identifier;
    ret->identifierForLsp = decl->identifierForLsp;
    ret->keywordPos = decl->keywordPos;
    ret->moduleName = decl->moduleName;
    ret->fullPackageName = decl->fullPackageName;
    ret->outerDecl = decl->outerDecl;
    ret->specificImplementation = decl->specificImplementation;
    ret->checkFlag = decl->checkFlag;
    ret->captureIndex = decl->captureIndex;
    ret->linkage = decl->linkage;
    for (auto& anno : decl->annotations) {
        ret->annotations.emplace_back(CloneNode(anno.get()));
    }
    ret->annotationsArray = CloneNode(decl->annotationsArray.get());
    visitCallback(*decl, *ret);
    return ret;
}

OwnedPtr<ConstPattern> ASTCloner::Impl::CloneConstPattern(const ConstPattern& cp)
{
    auto ret = MakeOwned<ConstPattern>();
    ret->literal = CloneExpr(cp.literal.get());
    ret->operatorCallExpr = CloneExpr(cp.operatorCallExpr.get());
    return ret;
}

OwnedPtr<VarPattern> ASTCloner::Impl::CloneVarPattern(const VarPattern& vp)
{
    OwnedPtr<VarPattern> ret;
    if (vp.varDecl) {
        ret = MakeOwned<VarPattern>(vp.varDecl->identifier, vp.begin);
        auto d = CloneDecl(vp.varDecl.get());
        ret->varDecl.reset(As<ASTKind::VAR_DECL>(d.release()));
    } else {
        ret = MakeOwned<VarPattern>();
    }
    ret->desugarExpr = CloneExpr(vp.desugarExpr.get());
    return ret;
}

OwnedPtr<TuplePattern> ASTCloner::Impl::CloneTuplePattern(const TuplePattern& tp)
{
    auto ret = MakeOwned<TuplePattern>();
    for (auto& i : tp.patterns) {
        ret->patterns.push_back(ClonePattern(i.get()));
    }
    ret->leftBracePos = tp.leftBracePos;
    ret->rightBracePos = tp.rightBracePos;
    ret->commaPosVector = tp.commaPosVector;
    return ret;
}

OwnedPtr<TypePattern> ASTCloner::Impl::CloneTypePattern(const TypePattern& tp)
{
    auto ret = MakeOwned<TypePattern>();
    ret->pattern = ClonePattern(tp.pattern.get());
    ret->type = CloneType(tp.type.get());
    ret->colonPos = tp.colonPos;

    if (tp.desugarExpr != nullptr) {
        ret->desugarExpr = CloneExpr(tp.desugarExpr.get());
    }
    if (tp.desugarVarPattern != nullptr) {
        ret->desugarVarPattern = ASTCloner::Clone(tp.desugarVarPattern.get(), visitCallback);
    }

    ret->needRuntimeTypeCheck = tp.needRuntimeTypeCheck;
    ret->matchBeforeRuntime = tp.matchBeforeRuntime;
    return ret;
}

OwnedPtr<EnumPattern> ASTCloner::Impl::CloneEnumPattern(const EnumPattern& ep)
{
    auto ret = MakeOwned<EnumPattern>();
    ret->constructor = CloneExpr(ep.constructor.get());
    for (auto& p : ep.patterns) {
        ret->patterns.emplace_back(ClonePattern(p.get()));
    }
    ret->leftParenPos = ep.leftParenPos;
    ret->rightParenPos = ep.rightParenPos;
    ret->commaPosVector = ep.commaPosVector;
    return ret;
}

OwnedPtr<ExceptTypePattern> ASTCloner::Impl::CloneExceptTypePattern(const ExceptTypePattern& etp)
{
    auto ret = MakeOwned<ExceptTypePattern>();
    ret->pattern = ClonePattern(etp.pattern.get());
    for (auto& i : etp.types) {
        ret->types.push_back(CloneType(i.get()));
    }
    ret->patternPos = etp.patternPos;
    ret->colonPos = etp.colonPos;
    ret->bitOrPosVector = etp.bitOrPosVector;
    return ret;
}

OwnedPtr<CommandTypePattern> ASTCloner::Impl::CloneCommandTypePattern(const CommandTypePattern& ctp)
{
    auto ret = MakeOwned<CommandTypePattern>();
    ret->pattern = ClonePattern(ctp.pattern.get());
    for (auto& i : ctp.types) {
        ret->types.push_back(CloneType(i.get()));
    }
    ret->patternPos = ctp.patternPos;
    ret->colonPos = ctp.colonPos;
    ret->bitOrPosVector = ctp.bitOrPosVector;
    return ret;
}

OwnedPtr<VarOrEnumPattern> ASTCloner::Impl::CloneVarOrEnumPattern(const VarOrEnumPattern& vep)
{
    auto ret = MakeOwned<VarOrEnumPattern>(vep.identifier);
    if (vep.pattern) {
        ret->pattern = ClonePattern(vep.pattern.get());
    }
    return ret;
}

/**
 * NOTE: To guarantee the members of Pattern is copied, the sub method should not be called outside 'ClonePattern'.
 */
OwnedPtr<Pattern> ASTCloner::Impl::ClonePattern(Ptr<Pattern> pattern)
{
    if (!pattern) {
        return OwnedPtr<Pattern>();
    }
    auto ret = match(*pattern)([this](const ConstPattern& e) { return OwnedPtr<Pattern>(CloneConstPattern(e)); },
        [](const WildcardPattern& e) { return OwnedPtr<Pattern>(MakeOwned<WildcardPattern>(e)); },
        [this](const VarPattern& e) { return OwnedPtr<Pattern>(CloneVarPattern(e)); },
        [this](const TuplePattern& e) { return OwnedPtr<Pattern>(CloneTuplePattern(e)); },
        [this](const TypePattern& e) { return OwnedPtr<Pattern>(CloneTypePattern(e)); },
        [this](const EnumPattern& e) { return OwnedPtr<Pattern>(CloneEnumPattern(e)); },
        [this](const ExceptTypePattern& e) { return OwnedPtr<Pattern>(CloneExceptTypePattern(e)); },
        [this](const CommandTypePattern& e) { return OwnedPtr<Pattern>(CloneCommandTypePattern(e)); },
        [this](const VarOrEnumPattern& e) { return OwnedPtr<Pattern>(CloneVarOrEnumPattern(e)); },
        [](const InvalidPattern& e) { return OwnedPtr<Pattern>(MakeOwned<InvalidPattern>(e)); },
        []() { return OwnedPtr<Pattern>(MakeOwned<InvalidPattern>()); });
    CJC_ASSERT(ret && ret->astKind == pattern->astKind);
    CopyNodeField(ret.get(), *pattern);
    // Clone field in Pattern.
    ret->ctxExpr = pattern->ctxExpr;
    ret->EnableAttr(Attribute::COMPILER_ADD);
    visitCallback(*pattern, *ret);
    return ret;
}

OwnedPtr<Block> ASTCloner::Impl::CloneBlock(const Block& block)
{
    auto ret = MakeOwned<Block>();
    CopyNodeField(ret.get(), block);
    // Clone field in Block.
    ret->unsafePos = block.unsafePos;
    ret->leftCurlPos = block.leftCurlPos;
    for (auto& it : block.body) {
        ret->body.push_back(CloneNode(it.get()));
    }
    ret->rightCurlPos = block.rightCurlPos;
    ret->EnableAttr(Attribute::COMPILER_ADD);
    return ret;
}

OwnedPtr<ClassBody> ASTCloner::Impl::CloneClassBody(const ClassBody& cb)
{
    auto ret = MakeOwned<ClassBody>();
    CopyNodeField(ret.get(), cb);
    // Clone field in ClassBody.
    ret->leftCurlPos = cb.leftCurlPos;
    for (auto& it : cb.decls) {
        ret->decls.push_back(CloneDecl(it.get()));
    }
    ret->rightCurlPos = cb.rightCurlPos;
    ret->EnableAttr(Attribute::COMPILER_ADD);
    return ret;
}

OwnedPtr<StructBody> ASTCloner::Impl::CloneStructBody(const StructBody& sb)
{
    auto ret = MakeOwned<StructBody>();
    CopyNodeField(ret.get(), sb);
    ret->leftCurlPos = sb.leftCurlPos;
    for (auto& it : sb.decls) {
        ret->decls.push_back(CloneDecl(it.get()));
    }
    ret->rightCurlPos = sb.rightCurlPos;
    ret->EnableAttr(Attribute::COMPILER_ADD);
    return ret;
}

OwnedPtr<InterfaceBody> ASTCloner::Impl::CloneInterfaceBody(const InterfaceBody& ib)
{
    auto ret = MakeOwned<InterfaceBody>();
    CopyNodeField(ret.get(), ib);
    // Clone field in InterfaceBody.
    ret->leftCurlPos = ib.leftCurlPos;
    for (auto& it : ib.decls) {
        ret->decls.push_back(CloneDecl(it.get()));
    }
    ret->rightCurlPos = ib.rightCurlPos;
    ret->EnableAttr(Attribute::COMPILER_ADD);
    return ret;
}

OwnedPtr<GenericConstraint> ASTCloner::Impl::CloneGenericConstraint(const GenericConstraint& gc)
{
    auto ret = MakeOwned<GenericConstraint>();
    CopyNodeField(ret.get(), gc);
    // Clone field in GenericConstraint.
    ret->type.reset(As<ASTKind::REF_TYPE>(CloneType(gc.type.get()).release()));
    for (auto& upperBound : gc.upperBounds) {
        ret->upperBounds.push_back(CloneType(upperBound.get()));
    }
    ret->wherePos = gc.wherePos;
    ret->operatorPos = gc.operatorPos;
    ret->bitAndPos = gc.bitAndPos;
    ret->commaPos = gc.commaPos;
    ret->isImplicitlyIntroduced = gc.isImplicitlyIntroduced;
    ret->EnableAttr(Attribute::COMPILER_ADD);
    return ret;
}

OwnedPtr<FuncBody> ASTCloner::Impl::CloneFuncBody(const FuncBody& fb)
{
    auto ret = MakeOwned<FuncBody>();
    CopyNodeField(ret.get(), fb);
    // Clone field in FuncBody.
    for (auto& it : fb.paramLists) {
        ret->paramLists.push_back(CloneNode(it.get()));
    }
    ret->doubleArrowPos = fb.doubleArrowPos;
    ret->colonPos = fb.colonPos;
    ret->retType = CloneType(fb.retType.get());
    ret->body = CloneExpr(fb.body.get());
    if (fb.generic) {
        ret->generic = CloneGeneric(*fb.generic);
    }
    for (auto& it : fb.capturedVars) {
        ret->capturedVars.insert(it);
    }
    ret->captureKind = fb.captureKind;
    ret->outerFunc = fb.outerFunc;
    ret->parentClassLike = fb.parentClassLike;
    ret->parentStruct = fb.parentStruct;
    ret->parentEnum = fb.parentEnum;
    ret->EnableAttr(Attribute::COMPILER_ADD);
    return ret;
}

OwnedPtr<FuncParamList> ASTCloner::Impl::CloneFuncParamList(const FuncParamList& fpl)
{
    auto ret = MakeOwned<FuncParamList>();
    CopyNodeField(ret.get(), fpl);
    // Clone field in FuncParamList.
    ret->leftParenPos = fpl.leftParenPos;
    for (auto& it : fpl.params) {
        auto funcParam = MakeOwned<FuncParam>();
        auto d = CloneDecl(it.get());
        funcParam.reset(As<ASTKind::FUNC_PARAM>(d.release()));
        ret->params.push_back(std::move(funcParam));
    }
    ret->variadicArgIndex = fpl.variadicArgIndex;
    ret->hasVariableLenArg = fpl.hasVariableLenArg;
    ret->rightParenPos = fpl.rightParenPos;
    ret->EnableAttr(Attribute::COMPILER_ADD);
    return ret;
}

OwnedPtr<FuncArg> ASTCloner::Impl::CloneFuncArg(const FuncArg& fa)
{
    auto ret = MakeOwned<FuncArg>();
    CopyNodeField(ret.get(), fa);
    // Clone field in FuncArg.
    ret->name = fa.name;
    ret->withInout = fa.withInout;
    ret->colonPos = fa.colonPos;
    ret->expr = CloneExpr(fa.expr.get());
    ret->commaPos = fa.commaPos;
    ret->EnableAttr(Attribute::COMPILER_ADD);
    return ret;
}

OwnedPtr<Annotation> ASTCloner::Impl::CloneAnnotation(const Annotation& annotation)
{
    auto ret = MakeOwned<Annotation>(annotation.identifier, annotation.kind, annotation.begin);
    CopyNodeField(ret.get(), annotation);
    ret->kind = annotation.kind;
    ret->definedPackage = annotation.definedPackage;
    ret->identifier = annotation.identifier;
    ret->isCompileTimeVisible = annotation.isCompileTimeVisible;
    ret->attrs = annotation.attrs;
    ret->attrCommas = annotation.attrCommas;
    ret->rsquarePos = annotation.rsquarePos;
    ret->lsquarePos = annotation.lsquarePos;
    for (auto& arg : annotation.args) {
        ret->args.emplace_back(CloneNode(arg.get()));
    }
    ret->condExpr = CloneExpr(annotation.condExpr.get());
    ret->baseExpr = CloneExpr(annotation.baseExpr.get());
    return ret;
}

OwnedPtr<ImportSpec> ASTCloner::Impl::CloneImportSpec(const ImportSpec& is)
{
    auto ret = MakeOwned<ImportSpec>();
    CopyNodeField(ret.get(), is);
    if (is.modifier) {
        // Clone Modifier
        ret->modifier = MakeOwned<Modifier>(is.modifier->modifier, is.modifier->begin);
        CopyNodeField(ret->modifier.get(), *is.modifier);
        ret->modifier->isExplicit = is.modifier->isExplicit;
    }
    std::function<void(const ImportContent&, ImportContent&)> cloneContent
        = [&cloneContent](const ImportContent& src, ImportContent& dst) {
        CopyNodeField(&dst, src);
        dst.kind = src.kind;
        dst.prefixPaths = src.prefixPaths;
        dst.prefixPoses = src.prefixPoses;
        dst.prefixDotPoses = src.prefixDotPoses;
        dst.identifier = src.identifier;
        dst.asPos = src.asPos;
        dst.aliasName = src.aliasName;
        dst.leftCurlPos = src.leftCurlPos;
        if (!src.items.empty()) {
            dst.items.resize(src.items.size());
            for (size_t i = 0; i < src.items.size(); ++i) {
                cloneContent(src.items[i], dst.items[i]);
            }
        }
        dst.commaPoses = src.commaPoses;
        dst.rightCurlPos = src.rightCurlPos;
    };
    cloneContent(is.content, ret->content);
    for (auto& it : is.annotations) {
        ret->annotations.emplace_back(CloneNode(it.get()));
    }
    return ret;
}

OwnedPtr<MatchCase> ASTCloner::Impl::CloneMatchCase(const MatchCase& mc)
{
    auto ret = MakeOwned<MatchCase>();
    CopyNodeField(ret.get(), mc);
    // Clone field in MatchCase.
    for (auto& pattern : mc.patterns) {
        ret->patterns.emplace_back(ClonePattern(pattern.get()));
    }
    ret->patternGuard = CloneExpr(mc.patternGuard.get());
    ret->exprOrDecls = CloneExpr(mc.exprOrDecls.get());
    ret->wherePos = mc.wherePos;
    ret->arrowPos = mc.arrowPos;
    ret->EnableAttr(Attribute::COMPILER_ADD);
    ret->bitOrPosVector = mc.bitOrPosVector;
    return ret;
}

OwnedPtr<MatchCaseOther> ASTCloner::Impl::CloneMatchCaseOther(const MatchCaseOther& mco)
{
    auto ret = MakeOwned<MatchCaseOther>();
    CopyNodeField(ret.get(), mco);
    // Clone field in MatchCaseOther.
    ret->matchExpr = CloneExpr(mco.matchExpr.get());
    ret->exprOrDecls = CloneExpr(mco.exprOrDecls.get());
    ret->arrowPos = mco.arrowPos;
    ret->EnableAttr(Attribute::COMPILER_ADD);
    return ret;
}

// Clone Node.
template <typename NodeT> OwnedPtr<NodeT> ASTCloner::Impl::CloneNode(Ptr<NodeT> node)
{
    if (!node) {
        return OwnedPtr<NodeT>();
    }
    auto clonedNode = match(*node)(
        // NOTE: Package, File, PackageSpec, ImportSpec, Modifier are ignored.
        // EnumBody struct is empty, no need to be cloned
        [this](Type& type) { return OwnedPtr<Node>(CloneType(Ptr(&type))); },
        [this](Expr& expr) { return OwnedPtr<Node>(CloneExpr(Ptr(&expr))); },
        [this](Decl& decl) { return OwnedPtr<Node>(CloneDecl(Ptr(&decl))); },
        [this](Pattern& pattern) { return OwnedPtr<Node>(ClonePattern(Ptr(&pattern))); },
        [this](const ClassBody& cb) { return OwnedPtr<Node>(CloneClassBody(cb)); },
        [this](const StructBody& rb) { return OwnedPtr<Node>(CloneStructBody(rb)); },
        [this](const InterfaceBody& ib) { return OwnedPtr<Node>(CloneInterfaceBody(ib)); },
        [this](const GenericConstraint& gc) { return OwnedPtr<Node>(CloneGenericConstraint(gc)); },
        [this](const FuncBody& fb) { return OwnedPtr<Node>(CloneFuncBody(fb)); },
        [this](const FuncParamList& fpl) { return OwnedPtr<Node>(CloneFuncParamList(fpl)); },
        [this](const FuncArg& fa) { return OwnedPtr<Node>(CloneFuncArg(fa)); },
        [this](const MatchCase& mc) { return OwnedPtr<Node>(CloneMatchCase(mc)); },
        [this](const MatchCaseOther& mco) { return OwnedPtr<Node>(CloneMatchCaseOther(mco)); },
        [this](const Annotation& ann) { return OwnedPtr<Node>(CloneAnnotation(ann)); },
        [](const DummyBody&) { return OwnedPtr<Node>(OwnedPtr<DummyBody>()); },
        [this](const ImportSpec& is) { return OwnedPtr<Node>(CloneImportSpec(is)); },
        []() {
            // Invalid cases.
            return OwnedPtr<Node>(MakeOwned<InvalidExpr>());
        });
    CJC_ASSERT(clonedNode);
    visitCallback(*node, *clonedNode);
    clonedNode->EnableAttr(Attribute::COMPILER_ADD);
    auto result = DynamicCast<NodeT*>(clonedNode.release());
    return OwnedPtr<NodeT>(result);
}

OwnedPtr<Node> ASTCloner::Impl::cloneWithRearrange(Ptr<Node> node, const VisitFunc& userVisitor)
{
    VisitFunc collectMap = [this, userVisitor](Node& from, Node& target) {
        // Collect decl to decl map.
        if (auto decl = DynamicCast<Decl*>(&from); decl) {
            source2cloned[decl] = &target;
            auto& targetDecl = static_cast<Decl&>(target);
            targetAddrMapInsert(decl->outerDecl, targetDecl.outerDecl);
            targetAddrMapInsert(decl->genericDecl, targetDecl.genericDecl);
            // unorder_set<Ptr<Node>> users field in Decl is ignored.
        }
        if (auto expr = DynamicCast<Expr*>(&from); expr) {
            source2cloned[expr] = &target;
            auto& targetExpr = static_cast<Expr&>(target);
            targetAddrMapInsert(expr->sourceExpr, targetExpr.sourceExpr);
            targetAddrMapInsert(expr->mapExpr, targetExpr.mapExpr);
        }
        if (auto funcBody = DynamicCast<FuncBody*>(&from); funcBody) {
            source2cloned[funcBody] = &target;
            auto& targetFuncBody = static_cast<FuncBody&>(target);
            targetAddrMapInsert(funcBody->funcDecl, targetFuncBody.funcDecl);
            targetAddrMapInsert(funcBody->outerFunc, targetFuncBody.outerFunc);
            targetAddrMapInsert(funcBody->parentClassLike, targetFuncBody.parentClassLike);
            targetAddrMapInsert(funcBody->parentStruct, targetFuncBody.parentStruct);
            targetAddrMapInsert(funcBody->parentEnum, targetFuncBody.parentEnum);
        }
        if (auto pattern = DynamicCast<Pattern*>(&from); pattern) {
            source2cloned[pattern] = &target;
            auto& targetPattern = static_cast<Pattern&>(target);
            targetAddrMapInsert(pattern->ctxExpr, targetPattern.ctxExpr);
        }

        // Package and File are ignored since usually we do not need to clone them.

        // Collect target variable address to target variable address map.
        // For sub-class of Expr, there is only 1 layer under it, we just enumerate them.
        // For sub-class of Decl, there are 2 layers under it. FuncParam extends Vardecl
        // which extends Decl.
        // Macro related ASTs also are ignored

        switch (from.astKind) {
            // sub-class of Expr struct
            case ASTKind::REF_EXPR: {
                auto& reFrom = static_cast<RefExpr&>(from);
                auto& reTarget = static_cast<RefExpr&>(target);
                targetAddrMapInsert(reFrom.ref.target, reTarget.ref.target);
                targetAddrMapInsert(reFrom.callOrPattern, reTarget.callOrPattern);
                break;
            }
            case ASTKind::ARRAY_EXPR: {
                auto& aeFrom = static_cast<ArrayExpr&>(from);
                auto& aeTarget = static_cast<ArrayExpr&>(target);
                targetAddrMapInsert(aeFrom.initFunc, aeTarget.initFunc);
                break;
            }
            case ASTKind::ARRAY_LIT: {
                auto& aeFrom = static_cast<ArrayLit&>(from);
                auto& aeTarget = static_cast<ArrayLit&>(target);
                targetAddrMapInsert(aeFrom.initFunc, aeTarget.initFunc);
                break;
            }
            case ASTKind::MEMBER_ACCESS: {
                auto& maFrom = static_cast<MemberAccess&>(from);
                if (auto base = DynamicCast<RefExpr*>(maFrom.baseExpr.get()); !base || !base->isThis) {
                    break; // Do not rearrange target if current is not accessing 'this'.
                }
                auto& maTarget = static_cast<MemberAccess&>(target);
                targetAddrMapInsert(maFrom.target, maTarget.target);
                targetAddrMapInsert(maFrom.callOrPattern, maTarget.callOrPattern);
                for (size_t i = 0; i < maFrom.targets.size(); i++) {
                    targetAddrMapInsert(maFrom.targets[i], maTarget.targets[i]);
                }
                // NOTE: foundUpperBoundMap is ignored
                break;
            }
            case ASTKind::FOR_IN_EXPR: {
                auto& fiFrom = static_cast<ForInExpr&>(from);
                auto& fiTarget = static_cast<ForInExpr&>(target);
                targetAddrMapInsert(fiFrom.patternInDesugarExpr, fiTarget.patternInDesugarExpr);
                break;
            }
            case ASTKind::CALL_EXPR: {
                auto& ceFrom = static_cast<CallExpr&>(from);
                if (auto ma = DynamicCast<MemberAccess*>(ceFrom.baseFunc.get())) {
                    if (auto base = DynamicCast<RefExpr*>(ma->baseExpr.get()); !base || !base->isThis) {
                        // Do not rearrange target if current is not call of refExpr or memberAccess of 'this'.
                        break;
                    }
                }
                auto& ceTarget = static_cast<CallExpr&>(target);
                targetAddrMapInsert(ceFrom.resolvedFunction, ceTarget.resolvedFunction);
                break;
            }
            case ASTKind::RETURN_EXPR: {
                auto& rtFrom = static_cast<ReturnExpr&>(from);
                auto& rtTarget = static_cast<ReturnExpr&>(target);
                targetAddrMapInsert(rtFrom.refFuncBody, rtTarget.refFuncBody);
                break;
            }
            case ASTKind::JUMP_EXPR: {
                auto& jeFrom = static_cast<JumpExpr&>(from);
                auto& jeTarget = static_cast<JumpExpr&>(target);
                targetAddrMapInsert(jeFrom.refLoop, jeTarget.refLoop);
                break;
            }
            case ASTKind::REF_TYPE: {
                auto& reFrom = static_cast<RefType&>(from);
                auto& reTarget = static_cast<RefType&>(target);
                targetAddrMapInsert(reFrom.ref.target, reTarget.ref.target);
                break;
            }
            case ASTKind::QUALIFIED_TYPE: {
                auto& qtFrom = static_cast<QualifiedType&>(from);
                auto& qtTarget = static_cast<QualifiedType&>(target);
                targetAddrMapInsert(qtFrom.target, qtTarget.target);
                break;
            }
            // case ASTKind::FUNC_BODY handled in if case
            case ASTKind::FUNC_DECL: {
                auto& fdFrom = static_cast<FuncDecl&>(from);
                auto& fdTarget = static_cast<FuncDecl&>(target);
                targetAddrMapInsert(fdFrom.ownerFunc, fdTarget.ownerFunc);
                targetAddrMapInsert(fdFrom.propDecl, fdTarget.propDecl);
                break;
            }
            case ASTKind::VAR_DECL: {
                auto& vdFrom = static_cast<VarDecl&>(from);
                auto& vdTarget = static_cast<VarDecl&>(target);
                targetAddrMapInsert(vdFrom.parentPattern, vdTarget.parentPattern);
                break;
            }
            default:
                break;
        }
        userVisitor(from, target);
    };
    visitCallback = collectMap;
    OwnedPtr<Node> targetNode = CloneNode(node);
    // Rearrange pointer to node pointer's target from source node pointer to cloned node pointer.
    for (auto& [s, t] : targetAddr2targetAddr) {
        if (source2cloned.find(*s) != source2cloned.end()) {
            *t = source2cloned[*s];
        }
    }
    return targetNode;
}

OwnedPtr<Node> RunCloneAst(Ptr<Node> node, const VisitFunc& visitFunc)
{
    ASTCloner::Impl impl;
    return impl.cloneWithRearrange(node, visitFunc);
}
} // namespace Cangjie::AST
