// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "JMethodIdCache.h"
#include "NativeFFI/Java/AfterTypeCheck/AfterTypeCheckContext.h"
#include "NativeFFI/Java/AfterTypeCheck/Utils.h"
#include "NativeFFI/Utils.h"
#include "cangjie/AST/Create.h"
#include "cangjie/AST/Node.h"
#include "cangjie/AST/Utils.h"
#include "cangjie/Utils/CheckUtils.h"
#include <algorithm>

namespace Cangjie::Native::FFI::Java {
using namespace Cangjie::AST;

std::string JMethodIdCache::GetAccessorName(const FuncDecl& method) const
{
    constexpr static auto mangledDollar = "$DOLLAR$";
    std::string mangledVarName = "jmethod$";

    // encoding for for the poor.
    // TODO: Consider package parts and outer types in mangling scheme.
    CJC_NULLPTR_CHECK(method.outerDecl);
    
    for (auto ch : utils.GetJavaJniClassName(*method.outerDecl->GetTy())) {
        if (ch == '$') {
            mangledVarName += mangledDollar;
        } else {
            mangledVarName += ch;
        }
    }
    mangledVarName += '$';
    mangledVarName += mangler.MangleType(*method.GetTy());
    for (auto ch : method.identifier.GetRawText()) {
        if (ch == '$') {
            mangledVarName += mangledDollar;
        } else {
            mangledVarName += ch;
        }
    }

    return mangledVarName;
}

void JMethodIdCache::Clear() noexcept
{
    cache.clear();
}

OwnedPtr<Expr> JMethodIdCache::CreateJMethodIdAccess(AfterTypeCheckContext& ctx,
    const AST::FuncDecl& method, Ptr<AST::Expr> jclass, Ptr<AST::Expr> envPtr)
{
    CJC_NULLPTR_CHECK(method.outerDecl);
    CJC_ASSERT(IsMirror(*method.outerDecl) || IsImpl(*method.outerDecl));

    auto& curFile = *envPtr->curFile;
    auto& jmethodVar = GetOrPut(ctx, method, curFile);

    std::vector<OwnedPtr<Node>> ifNull;
    std::vector<OwnedPtr<Node>> ifNotNull;

    ifNull.push_back(
        CreateAssignExpr(WithinFile(CreateRefExpr(jmethodVar), &curFile),
            jni.interface.CreateNewGlobalRefCall(envPtr, ilib.CreateGetMethodIdCall(??javaTy, envPtr, curFile)),
            TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT)));
    ifNull.push_back(WithinFile(CreateRefExpr(jmethodVar), &curFile));

    ifNotNull.push_back(WithinFile(CreateRefExpr(jmethodVar), &curFile));

    return CreateIfExpr(
        CreateIsPtrNullCheckCall(importManager, typeManager, WithinFile(CreateRefExpr(jmethodVar), &curFile)),
        CreateBlock(std::move(ifNull), &ilib.GetJniJClassTy()),
        CreateBlock(std::move(ifNotNull), &ilib.GetJniJClassTy()),
        &ilib.GetJniJClassTy());
}

VarDecl& JMethodIdCache::GetOrPut(AfterTypeCheckContext& ctx, const FuncDecl& method, File& curFile)
{
    auto cacheKey = utils.GetJavaJniClassName(*method.outerDecl);
    cacheKey += '$';
    cacheKey += mangler.MangleType(*method.GetTy());
    // Get variable that stores JClass
    if (auto it = cache.find(cacheKey); it != cache.end()) {
        return *it->second;
    }
    // Create variable since it does not exist yet
    auto& jmethodIdTy = ilib.GetJniJClassTy(); // TODO: replace with get methodID ty
    // auto curFile = envPtr->curFile;

    auto null = MakeOwned<PointerExpr>();
    null->SetTy(&jmethodIdTy);
    auto varDecl = CreateVarDecl(GetAccessorName(method), std::move(null), nullptr);
    varDecl->EnableAttr(Attribute::INTERNAL, Attribute::GLOBAL, Attribute::INITIALIZED, Attribute::NO_REFLECT_INFO);
    varDecl->isVar = true;
    varDecl->curFile = &curFile;

    cache[cacheKey] = varDecl.get();
    auto& ret = *varDecl;
    ctx.AddGeneratedDecl(std::move(varDecl));
    return ret;
}

}
