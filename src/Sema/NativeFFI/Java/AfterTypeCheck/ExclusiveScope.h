// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "NativeFFI/Java/AfterTypeCheck/AfterTypeCheckContext.h"
#include "cangjie/AST/Node.h"
#include "InteropLibBridge.h"

namespace Cangjie::Native::FFI::Java {

    using namespace Cangjie::AST;
    using InteropLibBridge = Cangjie::Interop::Java::InteropLibBridge;
    
    /**
    * Wraps the given body with an exclusiveScope lambda call.
    *
   
    * before:
    *     {
    *         // body
    *     }
    *
    * after:
    *     exclusiveScope {
    *         // body
    *     }
    */
    OwnedPtr<Expr> WrapWithExclusiveScope(AfterTypeCheckContext& ctx, InteropLibBridge& lib,
        OwnedPtr<Block> body, Ptr<Ty> retTy);

}
