// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares JNI jclass cache module.
 * Currently, it is not thread-safe but probably it should not be an issue
 * besides an extra access, global ref allocation and previously written global ref leak in the case of races.
 */
#ifndef CANGJIE_SEMA_NATIVE_FFI_JAVA_CACHING_API_JCLASS_CACHER
#define CANGJIE_SEMA_NATIVE_FFI_JAVA_CACHING_API_JCLASS_CACHER

#include <unordered_map>
#include "NativeFFI/Java/AfterTypeCheck/AfterTypeCheckContext.h"
#include "NativeFFI/Java/AfterTypeCheck/InteropLibBridge.h"
#include "NativeFFI/Java/AfterTypeCheck/JniBridge.h"
// #include "NativeFFI/Java/JavaFQName.h"
#include "cangjie/AST/Node.h"
#include "cangjie/Modules/ImportManager.h"

namespace Cangjie::Native::FFI::Java {
using namespace Interop::Java;

class JClassCache {
public:
    explicit JClassCache(
        const ImportManager& importManager,
        TypeManager& typeManager,
        InteropLibBridge& ilib,
        Interop::Java::Utils& utils);

    OwnedPtr<AST::Expr> CreateJClassAccess(AfterTypeCheckContext& ctx, const AST::Ty& javaTy, Ptr<AST::Expr> envPtr);

    void Clear() noexcept;
private:
    const ImportManager& importManager;
    TypeManager& typeManager;
    InteropLibBridge& ilib;
    JniBridge& jni;
    Interop::Java::Utils& utils;

    /**
     * For jclasses, already presented in cache, just returns corresponding jclass variable.
     * When there is no jclass of `fqname` (in form of "package.ClassName") in the cache,
     * it creates mangled top-level variable for corresponding jclass and returns it.
     */
    AST::VarDecl& GetOrPut(AfterTypeCheckContext& ctx, const AST::Ty& javaTy, AST::File& curFile);

    std::string GetAccessorName(const AST::Ty& javaTy) const;

    // std::unordered_map<JavaFQName, Ptr<AST::VarDecl>> cache;
    std::unordered_map<std::string, Ptr<AST::VarDecl>> cache;
};

} // namespace Cangjie::Native::FFI::Java

#endif // CANGJIE_SEMA_NATIVE_FFI_JAVA_CACHING_API_JCLASS_CACHER
