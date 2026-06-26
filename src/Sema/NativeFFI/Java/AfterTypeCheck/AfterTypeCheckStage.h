// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares sema after-typecheck abstract stage.
 */
#ifndef CANGJIE_SEMA_NATIVE_FFI_JAVA_AFTER_TYPECHECK_STAGE
#define CANGJIE_SEMA_NATIVE_FFI_JAVA_AFTER_TYPECHECK_STAGE

#include "AfterTypeCheckContext.h"

namespace Cangjie::Native::FFI::Java {

class AfterTypeCheckStage {
public:
    void operator()(AfterTypeCheckContext& ctx);
protected:
    virtual void Process(AfterTypeCheckContext& ctx) = 0;
    virtual ~AfterTypeCheckStage() = default;
};

} // namespace Cangjie::Native::FFI::Java

#endif // CANGJIE_SEMA_NATIVE_FFI_JAVA_AFTER_TYPECHECK_STAGE
