// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/AST2CHIR/TranslateASTNode/Translator.h"
#include "cangjie/CHIR/AST2CHIR/Utils.h"
#include "cangjie/CHIR/Utils/Utils.h"

using namespace Cangjie::CHIR;
using namespace Cangjie;

Ptr<Value> Translator::TranslateLeftValueOfVarDecl(const AST::VarDecl& decl, bool isLocalVar)
{
    Ptr<Value> leftValue = nullptr;
    auto varPos = TranslateLocation(decl);
    auto leftType = TranslateType(*(decl.ty));
    if (isLocalVar) {
        auto allocate =
            CreateAndAppendExpression<Allocate>(varPos, builder.GetType<RefType>(leftType), leftType, currentBlock);
        leftValue = allocate->GetResult();
        // Debug is only useful for allocated value, but not for implicit added
        // it will generate bug in cjdb if we generate debug expr for implicit added
        if (!decl.TestAttr(AST::Attribute::IMPLICIT_ADD)) {
            StaticCast<LocalVar*>(leftValue)->SetSrcCodeIdentifier(decl.identifier);
            CreateAndAppendExpression<Debug>(GetDeclLoc(builder.GetChirContext(), decl), varPos, builder.GetUnitTy(),
                leftValue, decl.identifier, currentBlock);
        }
        leftValue->AppendAttributeInfo(BuildVarDeclAttr(decl));
    } else {
        leftValue = GetSymbolTable(decl);
        leftValue->AppendAttributeInfo(BuildVarDeclAttr(decl));
    }

    return leftValue;
}

void Translator::StoreRValueToLValue(const AST::VarDecl& decl, Value& rval, Ptr<Value>& lval)
{
    auto leftType = TranslateType(*(decl.ty));
    auto varPos = TranslateLocation(decl);
    if (lval != nullptr) {
        CreateAndAppendWrappedStore(rval, *lval, varPos);
    } else {
        auto warnPos = GetDeclLoc(builder.GetChirContext(), decl);
        lval = TypeCastOrBoxIfNeeded(rval, *leftType, varPos);
        // If disable `-g` option, let variable still need create `Debug` node for DCE print warning.
        // Note: there is a special case exist only disable '-g' option: the LocalVar's debug node will be overwritten,
        // but does not affect debugging information, because disable '-g' option, codegen will skip `Debug` node.
        // the example code:
        // let a = 1    --->%1 = Debug(1, a)  the constant `1`'s Debug node is %1
        // let b = a    --->%2 = Debug(1, b)  the constant `1`'s Debug node is replaced by %2.
        // Also note that Debug expr must be placed on T&, typically an Allocate, and const local variable is never
        // allocated.
        if (!decl.TestAttr(AST::Attribute::IMPLICIT_ADD) && !decl.isConst) {
            // doesn't need to create allocate node, but have `debug` node.
            // 1. immutable variable and has initializer: `let a: Int32 = 1`
            auto debugInfo = CreateAndAppendExpression<Debug>(
                warnPos, varPos, builder.GetUnitTy(), lval.get(), decl.identifier, currentBlock);
            debugInfo->GetResult()->EnableAttr(Attribute::READONLY);
        }
    }
}

Ptr<Value> Translator::Visit(const AST::VarDecl& decl)
{
    CJC_ASSERT(!decl.TestAttr(AST::Attribute::GLOBAL));

    // The local const var is lifted as global variable, thus not handled here
    if (localConstVars.HasElement(&decl)) {
        return nullptr;
    }

    // Since current block will be changed during translation,
    // creation order should be 'alloca' -> 'debug' -> 'init' -> 'store'.
    Ptr<AST::Expr> init = decl.initializer.get();
    Ptr<Value> leftValue = TranslateLeftValueOfVarDecl(decl, true);
    if (init != nullptr) {
        // Need to try to get rvalue, otherwise next step will cast reftype to non-reftype. eg: cast Int64& to Int64.
        auto oldCompileTimeValueMark = builder.GetCompileTimeValueMark();
        builder.SetCompileTimeValueMark(decl.isConst);
        Ptr<Value> initNode = TranslateExprArg(*init);
        builder.SetCompileTimeValueMark(oldCompileTimeValueMark);
        StoreRValueToLValue(decl, *initNode, leftValue);
    }

    SetSymbolTable(decl, *leftValue);
    return leftValue;
}
