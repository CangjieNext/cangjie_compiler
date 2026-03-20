// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements inserting $getObjCClass function in Objective-C mirrors and impls
 */

#include "NativeFFI/ObjC/Utils/Common.h"
#include "Handlers.h"
#include "NativeFFI/Utils.h"
#include "cangjie/AST/AttributePack.h"
#include "cangjie/AST/Create.h"
#include "cangjie/AST/Node.h"
#include "cangjie/Utils/CheckUtils.h"

using namespace Cangjie::AST;
using namespace Cangjie::Interop::ObjC;
using namespace Cangjie::Native::FFI;

namespace {
constexpr auto NSSTRING_CLASS_IDENT = "NSString";
constexpr auto NSOBJECT_CLASS_IDENT = "NSObject";
constexpr auto TOSTRING_METHOD_IDENT = "toString";
constexpr auto INIT_IDENT = "init";

Ptr<FuncDecl> FindFirstMirrorGeneratedFunction(ClassDecl& decl, std::string identifier)
{
    auto found = std::find_if(decl.GetMemberDecls().begin(), decl.GetMemberDecls().end(),
        [identifier] (const OwnedPtr<Decl>& decl) {
            return decl->astKind == Cangjie::AST::ASTKind::FUNC_DECL &&
                decl->TestAttr(Attribute::OBJ_C_MIRROR_GENERATED_MEMBER) &&
                decl->identifier.Val() == identifier;
        });
    if (found != decl.GetMemberDecls().end()) {
        return As<ASTKind::FUNC_DECL>(found->get());
    }
    return nullptr;
}
} // namespace

void InsertStringConversions::HandleImpl(InteropContext& ctx)
{
    for (auto& mirror : ctx.mirrors) {
        if (mirror->TestAttr(Attribute::IS_BROKEN)) {
            continue;
        }

        if (mirror->astKind == ASTKind::CLASS_DECL) {
            auto classDecl = StaticCast<ClassDecl*>(mirror);

            auto foreignName = classDecl->identifier.Val();
            auto customName = Native::FFI::GetSingleArgumentAnnotationValue(*classDecl, AnnotationKind::OBJ_C_MIRROR);
            if (customName) {
                foreignName = customName;
            }

            if (foreignName == NSSTRING_CLASS_IDENT) {
                auto funcDecl = FindFirstMirrorGeneratedFunction(*classDecl, INIT_IDENT);
                CJC_NULLPTR_CHECK(funcDecl);
                auto funcParam = funcDecl->funcBody->paramLists[0]->params[0].get();
                auto funcParamRefExpr = CreateRefExpr(*funcParam);
                auto convertCall = ctx.factory.CreateConvertToNSStringCall(
                    std::move(funcParamRefExpr), *classDecl, classDecl->curFile);
                auto& block = funcDecl->funcBody->body;
                block->body.emplace_back(std::move(convertCall));
            }

            if (foreignName == NSOBJECT_CLASS_IDENT) {
                auto funcDecl = FindFirstMirrorGeneratedFunction(*classDecl, TOSTRING_METHOD_IDENT);
                CJC_NULLPTR_CHECK(funcDecl);
                auto nativeHandle = GetNativeVarHandle(*classDecl);
                auto nativeHandleRefExpr = CreateRefExpr(*nativeHandle);
                auto convertCall = ctx.factory.CreateDescriptionAsStringCall(std::move(nativeHandleRefExpr));
                auto& block = funcDecl->funcBody->body;
                block->body.emplace_back(std::move(convertCall));
            }
        }

    }
}
