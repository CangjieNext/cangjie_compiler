// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares JNI jmethodId cache module.
 * Currently, it is not thread-safe but probably it should not be an issue
 * besides an extra access, global ref allocation and previously written global ref leak in the case of races.
 */
#ifndef CANGJIE_SEMA_NATIVE_FFI_JAVA_CACHING_API_JMETHOD_ID_CACHE
#define CANGJIE_SEMA_NATIVE_FFI_JAVA_CACHING_API_JMETHOD_ID_CACHE

#include <unordered_map>
#include "NativeFFI/Java/AfterTypeCheck/AfterTypeCheckContext.h"
#include "NativeFFI/Java/AfterTypeCheck/InteropLibBridge.h"
#include "NativeFFI/Java/JavaMemberSignature.h"
#include "cangjie/AST/Node.h"
#include "cangjie/Modules/ImportManager.h"

namespace Cangjie::Native::FFI::Java {
using namespace Interop::Java;

class JMethodIdCache {
public:
    explicit JMethodIdCache(
        const ImportManager& importManager,
        TypeManager& typeManager,
        InteropLibBridge& ilib);

    OwnedPtr<AST::Expr> CreateJMethodIdAccess(AfterTypeCheckContext& ctx,
       JavaMemberSignature method, Ptr<AST::Expr> jclass, Ptr<AST::Expr> envPtr);

    void Clear() noexcept;
private:
    const ImportManager& importManager;
    TypeManager& typeManager;
    InteropLibBridge& ilib;

    /**
     * For jmethodId-s, already presented in cache, just returns corresponding jmethodId variable.
     * When there is no jmethodId of in the cache,
     * it creates mangled top-level variable for corresponding jmethod and returns it.
     */
    AST::VarDecl& GetOrPut(AfterTypeCheckContext& ctx, JavaMemberSignature method, AST::File& curFile);

    std::string GetNewAccessorName(const JavaMemberSignature& method) const;

    // std::unordered_map<JavaFQName, Ptr<AST::VarDecl>> cache;
    // std::unordered_map<std::string, Ptr<AST::VarDecl>> cache;
    std::unordered_map<JavaMemberSignature, Ptr<AST::VarDecl>> cache;
};

} // namespace Cangjie::Native::FFI::Java

#endif // CANGJIE_SEMA_NATIVE_FFI_JAVA_CACHING_API_JMETHOD_ID_CACHE
