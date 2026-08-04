// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "JClassCache.h"
#include "NativeFFI/Java/AfterTypeCheck/AfterTypeCheckContext.h"
#include "NativeFFI/Java/AfterTypeCheck/Utils.h"
#include "NativeFFI/Java/JavaFQName.h"
#include "NativeFFI/Utils.h"
#include "cangjie/AST/Create.h"
#include "cangjie/AST/Node.h"
#include "cangjie/AST/Utils.h"
#include "cangjie/Utils/CheckUtils.h"
#include <algorithm>

namespace Cangjie::Native::FFI::Java {
using namespace Cangjie::AST;

// std::string JClassCache::GetAccessorName(const Ty& javaTy) const
// {
//     constexpr static auto mangledDollar = "$DOLLAR$";
//     std::string mangledVarName = "jclass$";

//     for (auto packagePart : className.packageParts) {
//         for (auto ch : packagePart) {
//             if (ch == '$') {
//                 mangledVarName += mangledDollar;
//             } else {
//                 mangledVarName += ch;
//             }
//         }
//         mangledVarName += '$';
//     }

//     for (auto outerType : className.outerTypes) {
//         for (auto ch : outerType) {
//             if (ch == '$') {
//                 mangledVarName += mangledDollar;
//             } else {
//                 mangledVarName += ch;
//             }
//         }
//         mangledVarName += "$$";
//     }

//     for (auto ch : className.type) {
//         if (ch == '$') {
//             mangledVarName += mangledDollar;
//         } else {
//             mangledVarName += ch;
//         }
//     }

//     return mangledVarName;
// }

std::string JClassCache::GetAccessorName(const Ty& javaTy) const
{
    constexpr static auto mangledDollar = "$DOLLAR$";
    std::string mangledVarName = "jclass$";

    // encoding for for the poor.
    // TODO: Consider package parts and outer types in mangling scheme.
    for (auto ch : utils.GetJavaJniClassName(javaTy)) {
        if (ch == '$') {
            mangledVarName += mangledDollar;
        } else {
            mangledVarName += ch;
        }
    }

    return mangledVarName;
}

void JClassCache::Clear() noexcept
{
    cache.clear();
}

OwnedPtr<AST::Expr> JClassCache::CreateJClassAccess(
    AfterTypeCheckContext& ctx,
    const Ty& javaTy,
    Ptr<Expr> envPtr)
{
    CJC_ASSERT(!IsJArray(javaTy));
    CJC_ASSERT(IsMirror(javaTy) || IsImpl(javaTy));

    auto& curFile = *envPtr->curFile;
    auto& classVar = GetOrPut(ctx, javaTy, curFile);

    std::vector<OwnedPtr<Node>> ifNull;
    std::vector<OwnedPtr<Node>> ifNotNull;

    ifNull.push_back(
        CreateAssignExpr(WithinFile(CreateRefExpr(classVar), &curFile),
            jni.CreateNewGlobalRefCall(envPtr, ilib.CreateGetClassCall(javaTy, envPtr, curFile)),
            TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT)));
    ifNull.push_back(WithinFile(CreateRefExpr(classVar), &curFile));

    ifNotNull.push_back(WithinFile(CreateRefExpr(classVar), &curFile));

    return CreateIfExpr(
        CreateIsPtrNullCheckCall(importManager, typeManager, WithinFile(CreateRefExpr(classVar), &curFile)),
        CreateBlock(std::move(ifNull), &ilib.GetJniJClassTy()),
        CreateBlock(std::move(ifNotNull), &ilib.GetJniJClassTy()),
        &ilib.GetJniJClassTy());
}

VarDecl& JClassCache::GetOrPut(AfterTypeCheckContext& ctx, const Ty& javaTy, File& curFile)
{
    auto className = utils.GetJavaJniClassName(javaTy);
    // Get variable that stores JClass
    if (auto it = cache.find(className); it != cache.end()) {
        return *it->second;
    }
    // Create variable since it does not exist yet
    auto& jclassTy = ilib.GetJniJClassTy();
    // auto curFile = envPtr->curFile;

    auto null = MakeOwned<PointerExpr>();
    null->SetTy(&jclassTy);
    auto varDecl = CreateVarDecl(GetAccessorName(javaTy), std::move(null), nullptr);
    varDecl->EnableAttr(Attribute::INTERNAL, Attribute::GLOBAL, Attribute::INITIALIZED, Attribute::NO_REFLECT_INFO);
    varDecl->isVar = true;
    varDecl->curFile = &curFile;

    cache[className] = varDecl.get();
    auto& ret = *varDecl;
    ctx.AddGeneratedDecl(std::move(varDecl));
    return ret;
}

}
