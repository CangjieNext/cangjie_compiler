// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares PrepareTypeCheck handlers for implementation of Cangjie <-> Objective-C interopability.
 */

#ifndef CANGJIE_SEMA_NATIVEFFI_OBJC_BEFORETYPECHECK_HANDLERS_H
#define CANGJIE_SEMA_NATIVEFFI_OBJC_BEFORETYPECHECK_HANDLERS_H

#include "PrepareTypeCheckContext.h"
#include "NativeFFI/ObjC/Utils/Handler.h"

namespace Cangjie::Interop::ObjC {
class WarmupContext : public Handler<WarmupContext, PrepareTypeCheckContext> {
public:
    void HandleImpl(PrepareTypeCheckContext&);
};

class ReplaceFieldsWithProps : public Handler<ReplaceFieldsWithProps, PrepareTypeCheckContext> {
public:
    void HandleImpl(PrepareTypeCheckContext&);
};

class InsertToString : public Handler<InsertToString, PrepareTypeCheckContext> {
public:
    void HandleImpl(PrepareTypeCheckContext&);
};

class InsertFromStringCtor : public Handler<InsertFromStringCtor, PrepareTypeCheckContext> {
public:
    void HandleImpl(PrepareTypeCheckContext&);
};

class InsertRegCompanionDecl : public Handler<InsertRegCompanionDecl, PrepareTypeCheckContext> {
public:
    void HandleImpl(PrepareTypeCheckContext&);
};

class InsertHandleWrapperDecl : public Handler<InsertHandleWrapperDecl, PrepareTypeCheckContext> {
public:
    void HandleImpl(PrepareTypeCheckContext&);
};
} // namespace Cangjie::Interop::ObjC

#endif // CANGJIE_SEMA_NATIVEFFI_OBJC_BEFORETYPECHECK_HANDLERS_H
