// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements typecheck apis for array exprs.
 */

#include "TypeCheckerImpl.h"

#include "Diags.h"
#include "JoinAndMeet.h"
#include "TypeCheckUtil.h"

#include "cangjie/AST/ASTCasting.h"
#include "cangjie/AST/ASTContext.h"
#include "cangjie/AST/Clone.h"
#include "cangjie/AST/Create.h"
#include "cangjie/AST/Match.h"
#include "cangjie/AST/Node.h"
#include "cangjie/AST/RecoverDesugar.h"
#include "cangjie/AST/Symbol.h"
#include "cangjie/AST/Utils.h"
#include "cangjie/Basic/DiagnosticEngine.h"
#include "cangjie/Basic/Match.h"
#include "cangjie/Sema/Desugar.h"
#include "cangjie/AST/ModalInfo.h"
#include "cangjie/Sema/TypeManager.h"
#include "cangjie/Utils/ConstantsUtils.h"

namespace Cangjie {
using namespace Sema;
using namespace TypeCheckUtil;
class BuiltinDeclCreater {
public:
    BuiltinDeclCreater(TypeChecker::TypeCheckerImpl& typeChecker, AST::Package& pkg, DiagnosticEngine& diag)
        : c(typeChecker), pkg(pkg), tm{typeChecker.typeManager}, d{diag}
    {
    }

    void CreateBuiltinDecls()
    {
        CreateRawArrayDecl();
        CreateVArrayDecl();
        CreateCPointerDecl();
        CreateCStringDecl();
        CreateCFuncDecl();
    }

private:
    void FinalizeBuiltInDeclTy(BuiltInDecl& bid)
    {
        c.SetDeclTy(bid);
    }

    static constexpr int SYN_LINE = 1;
    int column{1}; // to make functions have unique pos, and make them after their class decl

    DataTy CTypeUpperBoundTy()
    {
        auto id = c.importManager.GetCoreDecl<InterfaceDecl>(CTYPE_NAME);
        if (!id || !id->GetTy().IsCorrect()) {
            return {TypeManager::GetInvalidTy()};
        }
        return tm.GetInterfaceTy(*id, {});
    }

    void AssignColumnsGenericConstraint(GenericConstraint& gc, File* file)
    {
        CJC_NULLPTR_CHECK(file);
        CopyFileID(&gc, file);
        gc.curFile = file;
        gc.begin.line = SYN_LINE;
        gc.begin.column = column++;
        if (gc.type) {
            CopyFileID(gc.type.get(), file);
            gc.type->curFile = file;
            gc.type->begin.line = SYN_LINE;
            gc.type->begin.column = column++;
            gc.type->end.line = SYN_LINE;
            gc.type->end.column = column++;
        }
        for (auto& ub : gc.upperBounds) {
            CopyFileID(ub.get(), file);
            ub->curFile = file;
            ub->begin.line = SYN_LINE;
            ub->begin.column = column++;
            ub->end.line = SYN_LINE;
            ub->end.column = column++;
        }
        gc.end.line = SYN_LINE;
        gc.end.column = column++;
    }

    void AddParamList(FuncDecl& fd)
    {
        Ptr<const Node> file = fd.curFile;
        auto pl = MakeOwnedNode<FuncParamList>();
        CopyFileID(pl.get(), file);
        pl->curFile = fd.curFile.get();
        pl->begin.line = SYN_LINE;
        pl->begin.column = column++;
        pl->EnableAttr(Attribute::COMPILER_ADD);
        fd.funcBody->paramLists.push_back(std::move(pl));
    }

    void CloseCtor(FuncDecl& fd)
    {
        CJC_ASSERT(!fd.funcBody->paramLists.empty());
        auto& pl = *fd.funcBody->paramLists.back();
        pl.end.line = SYN_LINE;
        pl.end.column = column++;
        fd.funcBody->end.line = SYN_LINE;
        fd.funcBody->end.column = column++;
        fd.end.line = SYN_LINE;
        fd.end.column = column++;
    }

    Ptr<FuncDecl> MakeCtor(BuiltInDecl& decl)
    {
        Ptr<const Node> file = decl.curFile;
        auto ret = MakeOwnedNode<FuncDecl>();
        CopyFileID(ret.get(), file);
        ret->begin.line = SYN_LINE;
        ret->begin.column = column++;
        ret->identifier = "init";
        ret->outerDecl = &decl;
        ret->fullPackageName = decl.fullPackageName;
        ret->curFile = decl.curFile;
        if (!decl.fullPackageName.empty()) {
            ret->moduleName = Utils::GetRootPackageName(decl.fullPackageName);
        }
        ret->EnableAttr(Attribute::CONSTRUCTOR, Attribute::PUBLIC);
        ret->funcBody = MakeOwnedNode<FuncBody>();
        CopyFileID(ret->funcBody.get(), file);
        ret->funcBody->begin.line = SYN_LINE;
        ret->funcBody->begin.column = column++;
        ret->funcBody->funcDecl = ret.get();
        decl.members.push_back(std::move(ret));
        return StaticCast<FuncDecl>(decl.members.back().get());
    }

    Ptr<FuncParam> MakeFuncParam(FuncDecl& decl, std::string&& id, ModalTy ty)
    {
        auto file = decl.curFile;
        auto ret = MakeOwnedNode<FuncParam>();
        CopyFileID(ret.get(), file);
        ret->begin.line = SYN_LINE;
        ret->begin.column = column++;
        ret->identifier = std::move(id);
        ret->type = MakeOwnedNode<Type>();
        CopyFileID(ret->type.get(), file);
        ret->type->curFile = decl.curFile.get();
        ret->type->begin.line = SYN_LINE;
        ret->type->begin.column = column++;
        ret->type->SetTy(ty);
        ret->type->end.line = SYN_LINE;
        ret->type->end.column = column++;
        ret->SetTy(ty);
        ret->outerDecl = &decl;
        ret->fullPackageName = decl.fullPackageName;
        ret->curFile = decl.outerDecl ? decl.outerDecl->curFile : nullptr;
        ret->end.line = SYN_LINE;
        ret->end.column = column++;
        CJC_ASSERT(!decl.funcBody->paramLists.empty());
        decl.funcBody->paramLists.back()->params.push_back(std::move(ret));
        return decl.funcBody->paramLists.back()->params.back();
    }

    static LocalModalAST ToLocalModalAST(ModalInfo modal)
    {
        switch (modal.local) {
            case LocalModal::NOT:
                return LocalModalAST::NOT;
            case LocalModal::HALF:
                return LocalModalAST::HALF;
            case LocalModal::FULL:
                return LocalModalAST::FULL;
            default:
                return LocalModalAST::NOT;
        }
    }

    ModalTy RawArrayInstTy(BuiltInDecl& bid, ModalInfo modal)
    {
        auto elemTy = tm.GetGenericsTy(*bid.generic->typeParameters[0]);
        return c.GetBuiltInArrayType({elemTy}, modal);
    }

    void AddCtorThisParam(FuncDecl& fd, BuiltInDecl& bid, ModalInfo modal)
    {
        if (modal == ModalInfo{}) {
            return;
        }
        CJC_ASSERT(!fd.funcBody->paramLists.empty());
        auto& pl = *fd.funcBody->paramLists.back();
        auto thisParam = MakeOwnedNode<ThisParam>();
        CopyFileID(thisParam.get(), fd.curFile);
        thisParam->curFile = fd.curFile;
        thisParam->begin = {SYN_LINE, column++};
        thisParam->modal.SetLocal(ToLocalModalAST(modal), {SYN_LINE, column++});
        thisParam->outerDecl = &bid;
        thisParam->end = {SYN_LINE, column++};
        thisParam->SetTy(RawArrayInstTy(bid, modal));
        pl.thisParam = std::move(thisParam);
    }

    void CreateCStringMembers(BuiltInDecl& decl)
    {
        /// public init(cString: CPointer<UInt8>)
        auto cptrUInt8 = tm.GetPointerTy(TypeManager::GetPrimitiveTy(TypeKind::TYPE_UINT8));
        auto fd = MakeCtor(decl);
        AddParamList(*fd);
        MakeFuncParam(*fd, "cString", {cptrUInt8});
        CloseCtor(*fd);
    }

    TypeChecker::TypeCheckerImpl& c;
    AST::Package& pkg;
    TypeManager& tm;
    DiagnosticEngine& d;

    void CreateRawArrayDecl()
    {
        File* f = pkg.files[0].get();
        auto bid = MakeOwnedNode<BuiltInDecl>(BuiltInType::ARRAY);
        CopyFileID(bid.get(), f);
        bid->begin.line = SYN_LINE;
        bid->begin.column = column++;
        bid->identifier = RAW_ARRAY_NAME;
        bid->generic = MakeOwnedNode<Generic>();
        CopyFileID(bid->generic.get(), f);
        bid->generic->curFile = f;
        bid->generic->begin.line = SYN_LINE;
        bid->generic->begin.column = column++;
        auto gpd = MakeOwnedNode<GenericParamDecl>();
        CopyFileID(gpd.get(), f);
        gpd->curFile = f;
        gpd->begin.line = SYN_LINE;
        gpd->begin.column = column++;
        gpd->identifier = "T";
        gpd->outerDecl = bid.get();
        gpd->end.line = SYN_LINE;
        gpd->end.column = column++;
        bid->generic->typeParameters.emplace_back(std::move(gpd));
        bid->generic->end.line = SYN_LINE;
        bid->generic->end.column = column++;
        bid->EnableAttr(Attribute::GLOBAL, Attribute::GENERIC);
        bid->curFile = f;
        bid->fullPackageName = CORE_PACKAGE_NAME;
        bid->end.line = SYN_LINE;
        bid->end.column = column++;
        FinalizeBuiltInDeclTy(*bid);
        for (int i = 0; i < MODAL_INFO_COUNT; ++i) {
            CreateRawArrayMembers(*bid, ToModalInfo(i));
        }
        pkg.files[0]->decls.emplace_back(std::move(bid));
    }

    void CreateRawArrayMembers(BuiltInDecl& bid, ModalInfo modal)
    {
        CreateRawArrayCtor0(bid, modal);
        CreateRawArrayCtorRepeat(bid, modal);
        CreateRawArrayCtorLambda(bid, modal);
        CreateRawArrayCtorCollection(bid, modal);
    }

    void CreateRawArrayCtorLambda(BuiltInDecl& bid, ModalInfo modal)
    {
        // init(size: Int64, initElement: (Int64) -> T @mode): RawArray<T> @mode
        auto fd = MakeCtor(bid);
        AddParamList(*fd);
        AddCtorThisParam(*fd, bid, modal);
        MakeFuncParam(*fd, "size", {TypeManager::GetPrimitiveTy(TypeKind::TYPE_INT64)});
        auto elemTy = ModalTy{tm.GetGenericsTy(*bid.generic->typeParameters[0]), modal};
        auto lamTy = ModalTy{tm.GetFunctionTy({ModalTy{TypeManager::GetPrimitiveTy(TypeKind::TYPE_INT64)}}, elemTy)};
        MakeFuncParam(*fd, "initElement", lamTy);
        fd->DisableAttr(Attribute::PUBLIC);
        CloseCtor(*fd);
    }

    void CreateRawArrayCtorCollection(BuiltInDecl& bid, ModalInfo modal)
    {
        // init(collection: Collection<T> @mode): RawArray<T> @mode
        auto fd = MakeCtor(bid);
        AddParamList(*fd);
        AddCtorThisParam(*fd, bid, modal);
        auto colDecl = c.importManager.GetCoreDecl<InterfaceDecl>("Collection");
        auto tty = tm.GetGenericsTy(*bid.generic->typeParameters[0]);
        Ptr<Ty> colTy;
        if (!colDecl) {
            d.Diagnose(bid, DiagKind::sema_no_core_object, "Collection");
            colTy = TypeManager::GetInvalidTy();
        } else {
            colTy = tm.GetInterfaceTy(*colDecl, {tty});
        }
        MakeFuncParam(*fd, "collection", ModalTy{colTy, modal});
        fd->DisableAttr(Attribute::PUBLIC);
        CloseCtor(*fd);
    }

    void CreateRawArrayCtorRepeat(BuiltInDecl& bid, ModalInfo modal)
    {
        // init(size: Int64, repeat!: T @mode): RawArray<T> @mode
        auto fd = MakeCtor(bid);
        AddParamList(*fd);
        AddCtorThisParam(*fd, bid, modal);
        MakeFuncParam(*fd, "size", {TypeManager::GetPrimitiveTy(TypeKind::TYPE_INT64)});
        auto elemTy = ModalTy{tm.GetGenericsTy(*bid.generic->typeParameters[0]), modal};
        auto repeat = MakeFuncParam(*fd, "repeat", elemTy);
        repeat->isNamedParam = true;
        fd->DisableAttr(Attribute::PUBLIC);
        CloseCtor(*fd);
    }

    void CreateRawArrayCtor0(BuiltInDecl& bid, ModalInfo modal)
    {
        // init(): RawArray<T> @mode
        auto fd = MakeCtor(bid);
        AddParamList(*fd);
        AddCtorThisParam(*fd, bid, modal);
        fd->DisableAttr(Attribute::PUBLIC);
        CloseCtor(*fd);
    }

    void CreateCPointerDecl()
    {
        auto bid = MakeOwnedNode<BuiltInDecl>(BuiltInType::POINTER);
        bid->begin.column = column++;
        bid->identifier = CPOINTER_NAME;
        bid->generic = MakeOwnedNode<Generic>();
        auto gpd = MakeOwnedNode<GenericParamDecl>();
        gpd->identifier = "T";
        gpd->outerDecl = bid.get();
        bid->generic->typeParameters.emplace_back(std::move(gpd));
        bid->generic->genericConstraints.emplace_back(CreateConstraintForFFI(CTYPE_NAME));
        bid->fullPackageName = CORE_PACKAGE_NAME;

        bid->EnableAttr(Attribute::PUBLIC, Attribute::GLOBAL, Attribute::GENERIC);
        CopyFileID(bid.get(), pkg.files[0].get());
        bid->curFile = pkg.files[0].get();
        bid->fullPackageName = CORE_PACKAGE_NAME;
        bid->end.column = column++;
        FinalizeBuiltInDeclTy(*bid);
        pkg.files[0]->decls.emplace_back(std::move(bid));
    }

    void CreateCStringDecl()
    {
        File* f = pkg.files[0].get();
        auto bid = MakeOwnedNode<BuiltInDecl>(BuiltInType::CSTRING);
        CopyFileID(bid.get(), f);
        bid->begin.line = SYN_LINE;
        bid->begin.column = column++;
        bid->identifier = CSTRING_NAME;
        bid->EnableAttr(Attribute::PUBLIC, Attribute::GLOBAL);
        bid->curFile = f;
        bid->fullPackageName = CORE_PACKAGE_NAME;
        CreateCStringMembers(*bid);
        bid->end.line = SYN_LINE;
        bid->end.column = column++;
        FinalizeBuiltInDeclTy(*bid);
        pkg.files[0]->decls.emplace_back(std::move(bid));
    }

    void CreateVArrayDecl()
    {
        auto bid = MakeOwnedNode<BuiltInDecl>(BuiltInType::VARRAY);
        bid->begin.column = column++;
        bid->identifier = VARRAY_NAME;
        bid->generic = MakeOwnedNode<Generic>();
        auto gpd = MakeOwnedNode<GenericParamDecl>();
        gpd->identifier = "T";
        gpd->outerDecl = bid.get();
        bid->generic->typeParameters.emplace_back(std::move(gpd));
        bid->EnableAttr(Attribute::GLOBAL, Attribute::GENERIC, Attribute::PUBLIC);
        CopyFileID(bid.get(), pkg.files[0].get());
        bid->curFile = pkg.files[0].get();
        bid->fullPackageName = CORE_PACKAGE_NAME;
        bid->end.column = column++;
        FinalizeBuiltInDeclTy(*bid);
        pkg.files[0]->decls.emplace_back(std::move(bid));
    }

    void CreateCFuncDecl()
    {
        File* f = pkg.files[0].get();
        auto bid = MakeOwnedNode<BuiltInDecl>(BuiltInType::CFUNC);
        CopyFileID(bid.get(), f);
        bid->begin.line = SYN_LINE;
        bid->begin.column = column++;
        bid->identifier = CFUNC_NAME;
        bid->generic = MakeOwnedNode<Generic>();
        CopyFileID(bid->generic.get(), f);
        bid->generic->curFile = f;
        bid->generic->begin.line = SYN_LINE;
        bid->generic->begin.column = column++;
        auto gpd = MakeOwnedNode<GenericParamDecl>();
        CopyFileID(gpd.get(), f);
        gpd->curFile = f;
        gpd->begin.line = SYN_LINE;
        gpd->begin.column = column++;
        gpd->identifier = "T";
        gpd->outerDecl = bid.get();
        gpd->end.line = SYN_LINE;
        gpd->end.column = column++;
        bid->generic->typeParameters.push_back(std::move(gpd));
        bid->generic->end.line = SYN_LINE;
        bid->generic->end.column = column++;
        bid->EnableAttr(Attribute::GLOBAL, Attribute::GENERIC, Attribute::PUBLIC);
        bid->curFile = f;
        bid->fullPackageName = CORE_PACKAGE_NAME;
        bid->end.line = SYN_LINE;
        bid->end.column = column++;
        FinalizeBuiltInDeclTy(*bid);
        pkg.files[0]->decls.emplace_back(std::move(bid));
    }
};

void TypeChecker::TypeCheckerImpl::CreateBuiltinDecls(AST::Package& pkg)
{
    BuiltinDeclCreater imp{*this, pkg, diag};
    imp.CreateBuiltinDecls();
}

ModalTy TypeChecker::TypeCheckerImpl::GetArrayTypeByInterface(ModalTy interfaceTy)
{
    ModalTy invalid = {TypeManager::GetInvalidTy()};
    auto arrayStruct = importManager.GetCoreDecl<StructDecl>("Array");
    if (!arrayStruct) {
        return invalid;
    }
    auto arrTys = promotion.Downgrade(arrayStruct->GetTy(), interfaceTy);
    if (arrTys.empty()) {
        return invalid;
    }
    return *arrTys.begin();
}

bool TypeChecker::TypeCheckerImpl::ChkArrayLit(ASTContext& ctx, ModalTy target, ArrayLit& al)
{
    auto targetTy = TypeCheckUtil::UnboxOptionType(target);
    // Set type first, if check succeed, type will be updated.
    al.SetTy({TypeManager::GetInvalidTy()});
    if (targetTy->IsInterface()) {
        targetTy = GetArrayTypeByInterface(targetTy);
        if (targetTy->IsInvalid()) {
            // If failed to get valid array type, there are two cases:
            // 1. array lit is empty, type is unable to be inferred.
            // 2. array lit is not empty, synthesize arrayLit's type and check with target.
            auto arrayLitTy = SynArrayLit(ctx, al);
            if (typeManager.IsSubtype(arrayLitTy, target)) {
                return true;
            }
            if (arrayLitTy.IsCorrect()) {
                DiagMismatchedTypes(diag, al, target);
            }
            return false;
        }
    } else if (!targetTy.IsCorrect() || (!targetTy->IsStructArray() && !Is<VArrayTy>(targetTy.Ty()))) {
        DiagMismatchedTypesWithFoundTy(diag, al, targetTy->String(), "Array");
        return false;
    }

    if (targetTy->typeArgs.empty()) {
        return false;
    }

    bool matched = true;
    auto vt = DynamicCast<VArrayTy*>(targetTy.Ty());
    if (vt && vt->size != static_cast<int64_t>(al.children.size())) {
        auto builder = diag.DiagnoseRefactor(DiagKindRefactor::sema_varray_size_match, al);
        builder.AddMainHintArguments(std::to_string(vt->size), std::to_string(al.children.size()));
        matched = false;
    }

    ModalTy arrayElemTy = targetTy->typeArgs[0];
    for (auto& child : al.children) {
        if (!Check(ctx, arrayElemTy, child.get())) {
            matched = false;
            break;
        }
    }
    if (matched) {
        al.SetTy(targetTy);
    }
    return matched;
}

ModalTy TypeChecker::TypeCheckerImpl::SynArrayLit(ASTContext& ctx, ArrayLit& al)
{
    if (al.children.empty()) {
        diag.Diagnose(al, DiagKind::sema_empty_arrayLit_type_undefined);
        al.SetTy({TypeManager::GetInvalidTy()});
        return al.GetTy();
    }
    std::set<ModalTy> arrayElemTys;
    bool hasInvalidElemTy = false;
    for (auto& child : al.children) {
        if (Synthesize({ctx, SynPos::EXPR_ARG}, child.get()) && !ReplaceIdealTy(*child)) {
            hasInvalidElemTy = true;
        }
        arrayElemTys.insert(child->GetTy());
    }

    auto arrayStruct = importManager.GetCoreDecl<StructDecl>("Array");
    if (hasInvalidElemTy || arrayStruct == nullptr) {
        // If there exists invalid element ty or 'core' package is not imported correctly,
        // error will be throwed in other process.
        al.SetTy({TypeManager::GetInvalidTy()});
        return al.GetTy();
    }

    auto joinRes = JoinAndMeet(typeManager, arrayElemTys, {}, &importManager, al.curFile).JoinAsVisibleTy();
    if (auto ty = std::get_if<ModalTy>(&joinRes)) {
        al.SetTy({typeManager.GetStructTy(*arrayStruct, {ty->Ty()}), ty->Mode()});
    } else {
        al.SetTy({TypeManager::GetInvalidTy()});
        auto errMsg = JoinAndMeet::CombineErrMsg(std::get<std::stack<std::string>>(joinRes));
        diag.Diagnose(al, DiagKind::sema_inconsistency_elemType, "array").AddNote(errMsg);
    }
    return al.GetTy();
}

bool TypeChecker::TypeCheckerImpl::IsBuiltinTypeAlias(const Decl& decl, TypeKind kind) const
{
    if (decl.astKind != ASTKind::TYPE_ALIAS_DECL) {
        return false;
    }
    auto& typeAliasDecl = static_cast<const TypeAliasDecl&>(decl);
    // If typealias' base type is array, expr is not a normal call base.
    ModalTy origTy = typeAliasDecl.type->GetTy();
    if (origTy) {
        if (kind == AST::TypeKind::TYPE_ANY) {
            // rawarray, cfunc, cstring use normal ChkCallExpr
            return (origTy->kind == TypeKind::TYPE_POINTER || origTy->kind == TypeKind::TYPE_VARRAY);
        } else {
            return origTy->kind == kind;
        }
    }
    return false;
}

namespace {
bool ShouldDesugarRawArrayCall(const CallExpr& ce)
{
    if (!ce.resolvedFunction || !ce.resolvedFunction->TestAttr(Attribute::CONSTRUCTOR)) {
        return false;
    }
    auto outer = ce.resolvedFunction->outerDecl;
    if (!outer || !outer->IsBuiltIn()) {
        return false;
    }
    if (StaticCast<BuiltInDecl>(outer)->type != BuiltInType::ARRAY) {
        return false;
    }
    if (ce.args.size() == 1) {
        return true;
    }
    return ce.args.size() == 2 && ce.args[1]->name.Empty();
}
} // namespace

// This is only used to add initFunc field
// we don't need this desugar if we can fit in initFunc somewhere else and trigger instantiation as well.
void TypeChecker::TypeCheckerImpl::DesugarRawArrayCall(CallExpr& ce)
{
    if (!ShouldDesugarRawArrayCall(ce)) {
        return;
    }
    auto arrayExpr = MakeOwnedNode<ArrayExpr>();
    arrayExpr->type = MakeOwnedNode<Type>();
    arrayExpr->type->SetTy(ce.GetTy());
    arrayExpr->SetTy(ce.GetTy());
    arrayExpr->isValueArray = false;
    arrayExpr->args = std::move(ce.args);
    ce.args.clear();
    auto funName = arrayExpr->args.size() == 1 ? "arrayInitByCollection" : "arrayInitByFunction";
    arrayExpr->initFunc = importManager.GetCoreDecl<FuncDecl>(funName);
    if (!arrayExpr->initFunc) {
        diag.Diagnose(ce, DiagKind::sema_no_core_object, funName);
        ce.SetTy({TypeManager::GetInvalidTy()});
        return;
    }
    arrayExpr->scopeName = ce.scopeName;
    arrayExpr->sourceExpr = &ce;
    arrayExpr->begin = ce.begin;
    arrayExpr->end = ce.end;
    ce.desugarExpr = std::move(arrayExpr);
    AddCurFile(ce, ce.curFile);
}

bool TypeChecker::TypeCheckerImpl::ChkVArrayArg(ASTContext& ctx, ArrayExpr& ve)
{
    // check arg.
    if (ve.args.size() != 1) {
        diag.DiagnoseRefactor(
            DiagKindRefactor::sema_varray_args_number_mismatch, ve, MakeRange(ve.leftParenPos, ve.rightParenPos + 1));
        return false;
    }
    bool ret = false;
    if (ve.args[0]->name.Empty()) {
        // For Lambda.
        auto sizeType = TypeManager::GetPrimitiveTy(TypeKind::TYPE_INT64);
        auto expectedExprTy = typeManager.GetFunctionTy({{sizeType}}, ve.type->GetTy()->typeArgs[0]);
        ret = Check(ctx, {expectedExprTy}, ve.args[0].get());
    } else {
        if (ve.args[0]->name != "repeat") {
            auto builder = diag.Diagnose(*ve.args[0], DiagKind::sema_unknown_named_argument, ve.args[0]->name.Val());
            builder.AddNote("expect the name of the named parameter is 'item'");
            return false;
        }
        // For item: T
        auto expectedItemTy = ve.type->GetTy()->typeArgs[0];
        ret = Check(ctx, expectedItemTy, ve.args[0].get());
    }

    if (ve.GetTy().IsCorrect() && !ve.type->GetTy().IsCorrect()) {
        ve.type->SetTy(ve.GetTy()); // Update array type ty, overwrite Array<Invalid>.
    }
    return ret;
}

bool TypeChecker::TypeCheckerImpl::ChkVArrayExpr(ASTContext& ctx, ModalTy target, ArrayExpr& ve)
{
    CJC_NULLPTR_CHECK(ve.type);
    ModalTy targetTy = TypeCheckUtil::UnboxOptionType(target);
    ve.type->SetTy(Synthesize({ctx, SynPos::EXPR_ARG}, ve.type.get()));
    if (!ve.type->GetTy().IsCorrect() || !Is<VArrayTy>(ve.type->DataTy())) {
        ve.SetTy({TypeManager::GetInvalidTy()});
        return false;
    }
    if (!ChkVArrayArg(ctx, ve)) {
        ve.SetTy({TypeManager::GetInvalidTy()});
        return false;
    }
    // check T and size.
    if (!typeManager.IsSubtype(ve.type->GetTy(), targetTy)) {
        DiagMismatchedTypesWithFoundTy(diag, ve, targetTy->String(), ve.type->GetTy().String());
        ve.SetTy({TypeManager::GetInvalidTy()});
        return false;
    }
    ve.SetTy(ve.type->GetTy());
    return true;
}

ModalTy TypeChecker::TypeCheckerImpl::SynVArrayExpr(ASTContext& ctx, ArrayExpr& ve)
{
    CJC_NULLPTR_CHECK(ve.type);
    ve.type->SetTy(Synthesize({ctx, SynPos::EXPR_ARG}, ve.type.get()));
    if (!ve.type->GetTy().IsCorrect() || !Is<VArrayTy>(ve.type->DataTy())) {
        ve.SetTy({TypeManager::GetInvalidTy()});
        return ve.GetTy();
    }
    ve.SetTy(ve.type->GetTy());
    if (!ChkVArrayArg(ctx, ve)) {
        ve.SetTy({TypeManager::GetInvalidTy()});
        return ve.GetTy();
    }
    return ve.GetTy();
}

bool TypeChecker::TypeCheckerImpl::ChkPointerCall(ASTContext& ctx, ModalTy target, CallExpr& ce)
{
    if (IsCallOfBuiltInType(ce, AST::TypeKind::TYPE_POINTER)) {
        bool ret = false;
        ModalTy resultTy{TypeManager::GetInvalidTy()};
        DesugarPointerCall(ctx, ce);
        auto pointerExpr = StaticAs<ASTKind::POINTER_EXPR>(ce.desugarExpr.get());
        if (target->IsInvalid()) {
            resultTy = SynPointerExpr(ctx, *pointerExpr);
            ret = resultTy.IsCorrect();
        } else {
            ret = ChkPointerExpr(ctx, target, *pointerExpr);
            resultTy = pointerExpr->GetTy();
        }
        if (!ret) {
            if (pointerExpr->arg) {
                (void)ce.args.emplace_back(std::move(pointerExpr->arg));
            }
            ce.desugarExpr = nullptr;
        }
        ce.SetTy(resultTy);
        return ret;
    }
    return false;
}

bool TypeChecker::TypeCheckerImpl::ChkVArrayCall(ASTContext& ctx, ModalTy target, CallExpr& ce)
{
    if (IsCallOfBuiltInType(ce, AST::TypeKind::TYPE_VARRAY)) {
        bool ret = false;
        ModalTy resultTy{TypeManager::GetInvalidTy()};
        DesugarArrayCall(ctx, ce);
        auto arrayExpr = StaticAs<ASTKind::ARRAY_EXPR>(ce.desugarExpr.get());
        if (target->IsInvalid()) {
            resultTy = SynVArrayExpr(ctx, *arrayExpr);
            ret = resultTy.IsCorrect();
        } else {
            ret = ChkVArrayExpr(ctx, target, *arrayExpr);
            resultTy = arrayExpr->GetTy();
        }
        if (!ret) {
            RecoverCallFromArrayExpr(ce);
        }
        ce.SetTy(resultTy);
        return ret;
    }
    return false;
}

bool TypeChecker::TypeCheckerImpl::ChkCFuncCall([[maybe_unused]] ASTContext& ctx, AST::ModalTy target, AST::CallExpr& ce)
{
    if (!ce.baseFunc) {
        return false;
    }

    if (!ce.baseFunc->GetTarget()) {
        return false;
    }
    Ptr<Decl> realTarget = GetRealTarget(ce.baseFunc, ce.baseFunc->GetTarget());

    if (!realTarget->IsBuiltIn() || !RawStaticCast<BuiltInDecl*>(realTarget)->IsType(BuiltInType::CFUNC)) {
        return false;
    }

    if (target.IsCorrect() && !target->IsCFunc()) {
        ce.SetTy({TypeManager::GetInvalidTy()});
        return false;
    }

    ce.baseFunc->SetTarget(realTarget);

    bool ret = SynCFuncCall(ctx, ce);
    if (!ret) {
        ce.SetTy({TypeManager::GetInvalidTy()});
    }
    return ret;
}

bool TypeChecker::TypeCheckerImpl::SynCFuncCall(ASTContext& ctx, CallExpr& ce)
{
    auto callee = DynamicCast<RefExpr>(&*ce.baseFunc);
    if (!callee) {
        ce.SetTy({TypeManager::GetInvalidTy()});
        return false;
    }
    ce.SetTy(Synthesize({ctx, SynPos::EXPR_ARG}, ce.baseFunc.get()));
    if (!Ty::IsTyCorrect(ce.baseFunc->GetTy()) || !Ty::IsTyCorrect(ce.GetTy())) {
        ce.SetTy({TypeManager::GetInvalidTy()});
        return false;
    }
    if (ce.GetTy()->IsCFunc()) {
        if (ce.args.size() != 1) {
            diag.Diagnose(*ce.baseFunc, DiagKind::sema_cfunc_too_many_arguments);
            ce.SetTy({TypeManager::GetInvalidTy()});
            return false;
        }
        Synthesize({ctx, SynPos::EXPR_ARG}, ce.args[0]);
        if (!Ty::IsTyCorrect(ce.args[0]->GetTy())) {
            ce.SetTy({TypeManager::GetInvalidTy()});
            return false;
        }
        bool res{true};
        if (!ce.args[0]->name.Empty()) {
            diag.Diagnose(ce.args[0]->name.Begin(), ce.args[0]->name.End(), DiagKind::sema_unknown_named_argument,
                ce.args[0]->name.Val());
            // the program is invalid, yet the type check can still pass if all other checks hold, do not goto INVALID
            // here
            res = false;
        }
        // only CFunc<...>(CPointer(...)) is valid
        if (ce.args[0]->GetTy()->IsPointer()) {
            ce.SetTy(ce.baseFunc->GetTy());
            return res;
        }
    }
    // Otherwise, report error and return a invalid ty.
    diag.Diagnose(*ce.args[0], DiagKind::sema_cfunc_ctor_must_be_cpointer);
    ce.SetTy({TypeManager::GetInvalidTy()});
    return false;
}

bool TypeChecker::TypeCheckerImpl::ChkBuiltinCall(ASTContext& ctx, ModalTy target, CallExpr& ce)
{
    static bool (TypeCheckerImpl::*const CHK_FUNCS[])(ASTContext&, ModalTy, CallExpr&) = {
        &TypeCheckerImpl::ChkPointerCall,
        &TypeCheckerImpl::ChkVArrayCall,
        &TypeCheckerImpl::ChkCFuncCall,
    };
    for (auto& chkFunc : CHK_FUNCS) {
        if ((this->*chkFunc)(ctx, target, ce)) {
            return true;
        }
    }
    return false;
}

bool TypeChecker::TypeCheckerImpl::IsCallOfBuiltInType(const CallExpr& ce, const AST::TypeKind kind) const
{
    if (!ce.baseFunc) {
        return false;
    }
    auto baseTarget = ce.baseFunc->GetTarget();
    if (!baseTarget) {
        return false;
    }
    auto isTypeAlias = IsBuiltinTypeAlias(*baseTarget, kind);
    BuiltInType builtInType;
    switch (kind) {
        case AST::TypeKind::TYPE_POINTER:
            builtInType = BuiltInType::POINTER;
            break;
        case AST::TypeKind::TYPE_VARRAY:
            builtInType = BuiltInType::VARRAY;
            break;
        default:
            return false;
    }
    auto isSpecifyBuiltinType = baseTarget->IsBuiltIn() && RawStaticCast<BuiltInDecl*>(baseTarget)->IsType(builtInType);
    return isTypeAlias || isSpecifyBuiltinType;
}

bool TypeChecker::TypeCheckerImpl::ChkPointerExpr(ASTContext& ctx, ModalTy target, PointerExpr& cpe)
{
    CJC_ASSERT(cpe.type && cpe.type->GetTy());
    bool ret = true;
    // 'var a: CPointer<T> = CPointer()': Generic types should need to be derived.
    ModalTy targetTy = TypeCheckUtil::UnboxOptionType(target);
    if (!targetTy->typeArgs.empty() && targetTy->typeArgs[0].IsCorrect() &&
        (!cpe.type->GetTy()->typeArgs[0].IsCorrect() || cpe.type->GetTy()->HasGeneric())) {
        cpe.type->SetTy(targetTy);
    }
    if (targetTy.IsCorrect()) {
        ret = Check(ctx, targetTy, cpe.type.get());
    } else {
        // 'var a = CPointer<T>()': Type derivation does not depend on target information.
        cpe.type->SetTy(Synthesize({ctx, SynPos::NONE}, cpe.type.get()));
    }
    cpe.SetTy(cpe.type->GetTy());
    // 'var a = CPointer()': Generic type cannot be derived.
    if (!cpe.GetTy().IsCorrect()) {
        diag.Diagnose(cpe, DiagKind::sema_pointer_unknow_generic_type);
        return false;
    } else if (!ret) {
        DiagMismatchedTypesWithFoundTy(diag, *cpe.sourceExpr, targetTy, cpe.GetTy());
    }

    // One arg.
    if (cpe.arg) {
        auto argTy = Synthesize({ctx, SynPos::EXPR_ARG}, cpe.arg.get());
        if (!argTy.IsCorrect() || !(argTy->IsPointer() || argTy->IsCFunc())) {
            if (!TypeCheckUtil::CanSkipDiag(*cpe.arg)) {
                diag.Diagnose(cpe, DiagKind::sema_pointer_single_element_type_error);
            }
            return false;
        }
        if (!cpe.arg->name.Empty()) {
            diag.Diagnose(
                cpe.arg->name.Begin(), cpe.arg->name.End(), DiagKind::sema_unknown_named_argument, cpe.arg->name.Val());
            return false;
        }
    }

    if (targetTy.IsCorrect() && !typeManager.IsSubtype(cpe.GetTy(), targetTy)) {
        DiagSemaMismatchedTypes(diag, cpe, target.String(), cpe.GetTy().String());
        return false;
    }
    return ret;
}

ModalTy TypeChecker::TypeCheckerImpl::SynPointerExpr(ASTContext& ctx, PointerExpr& cptrExpr)
{
    if (!ChkPointerExpr(ctx, {TypeManager::GetInvalidTy()}, cptrExpr)) {
        return {TypeManager::GetInvalidTy()};
    }
    return cptrExpr.GetTy();
}
} // namespace Cangjie
