// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "Desugar.h"
#include "NativeFFI/Utils.h"
#include "cangjie/AST/AttributePack.h"
#include "cangjie/AST/Node.h"
#include "cangjie/AST/Utils.h"
#include "cangjie/AST/ASTCasting.h"
#include "cangjie/Utils/CastingTemplate.h"
#include "cangjie/Utils/SafePointer.h"

using namespace Cangjie::AST;

namespace Cangjie::Interop::ObjC {
namespace {
// TODO: Probably should be moved to shared utils between BeforeTypeCheck and AfterTypeCheck
constexpr auto NSSTRING_CLASS_IDENT = "NSString";
constexpr auto NSOBJECT_CLASS_IDENT = "NSObject";
constexpr auto TOSTRING_METHOD_IDENT = "toString";
constexpr auto INIT_IDENT = "init";

void PutDeclToClassBody(FuncDecl& decl, ClassDecl& target)
{
    decl.curFile = target.curFile;
    decl.begin = target.body->end;
    decl.end = target.body->end;
    decl.EnableAttr(Attribute::IN_CLASSLIKE);
}

OwnedPtr<Decl> CreateNSStringFromStringCtorDecl(ClassDecl& target)
{
    auto param = CreateFuncParam("str", CreateRefType(STD_LIB_STRING));

    std::vector<OwnedPtr<FuncParam>> ctorParams;
    ctorParams.emplace_back(std::move(param));
    auto paramList = CreateFuncParamList(std::move(ctorParams));

    std::vector<OwnedPtr<FuncParamList>> paramLists;
    paramLists.emplace_back(std::move(paramList));

    auto block = CreateBlock({}, nullptr);
    block->EnableAttr(Attribute::IS_CHECK_VISITED);

    auto ctorBody = CreateFuncBody(std::move(paramLists), nullptr, std::move(block));
    auto ctorDecl = CreateFuncDecl(INIT_IDENT, std::move(ctorBody), nullptr);
    ctorDecl->EnableAttr(Attribute::PUBLIC, Attribute::CONSTRUCTOR, Attribute::OBJ_C_MIRROR_GENERATED_MEMBER);

    PutDeclToClassBody(*ctorDecl, target);

    return ctorDecl;
}

OwnedPtr<Decl> CreateNSObjectToStringDecl(ClassDecl& target)
{
    std::vector<OwnedPtr<FuncParam>> funcParams;
    auto paramList = CreateFuncParamList(std::move(funcParams));

    std::vector<OwnedPtr<FuncParamList>> paramLists;
    paramLists.emplace_back(std::move(paramList));

    auto block = CreateBlock({}, nullptr);
    block->EnableAttr(Attribute::IS_CHECK_VISITED);

    auto funcBody = CreateFuncBody(std::move(paramLists), CreateRefType(STD_LIB_STRING), std::move(block));
    auto funcDecl = CreateFuncDecl(TOSTRING_METHOD_IDENT, std::move(funcBody), nullptr);
    funcDecl->EnableAttr(Attribute::PUBLIC, Attribute::OBJ_C_MIRROR_GENERATED_MEMBER);

    PutDeclToClassBody(*funcDecl, target);

    return funcDecl;
}

void InsertNSStringCtor(ClassDecl& classDecl)
{
    auto ctorDecl = CreateNSStringFromStringCtorDecl(classDecl);
    classDecl.body->decls.push_back(std::move(ctorDecl));
}

void InsertNSObjectToString(ClassDecl& classDecl)
{
    auto funcDecl = CreateNSObjectToStringDecl(classDecl);
    classDecl.body->decls.push_back(std::move(funcDecl));
}

void DesugarMirrorClass(ClassDecl& classDecl)
{
    InsertMirrorVarProp(classDecl, Attribute::OBJ_C_MIRROR);

    auto foreignName = classDecl.identifier.Val();
    auto customName = Native::FFI::GetSingleArgumentAnnotationValue(classDecl, AnnotationKind::OBJ_C_MIRROR);
    if (customName) {
        foreignName = customName;
    }

    if (foreignName == NSSTRING_CLASS_IDENT) {
        InsertNSStringCtor(classDecl);
    } else if (foreignName == NSOBJECT_CLASS_IDENT) {
        InsertNSObjectToString(classDecl);
    }
}
}

void PrepareTypeCheck(Package& pkg)
{
    for (auto& file : pkg.files) {
        for (auto& decl : file->decls) {
            if (decl->TestAttr(Attribute::OBJ_C_MIRROR)) {
                if (auto cd = DynamicCast<ClassDecl*>(decl.get())) {
                    DesugarMirrorClass(*cd);
                }
            }
        }
    }
}
} // Cangjie::Interop::ObjC