// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_CHIR_TRANSFORMATION_PATCH_CJMP_SUPER_CALLS_H
#define CANGJIE_CHIR_TRANSFORMATION_PATCH_CJMP_SUPER_CALLS_H

#include "cangjie/CHIR/IR/CHIRBuilder.h"
#include "cangjie/CHIR/IR/Package.h"

namespace Cangjie::CHIR {

class PatchCJMPSuperCalls {
public:
    PatchCJMPSuperCalls(Package& pkg, CHIRBuilder& builder);

    void Run();

private:
    void PatchConstructorSuperCall(Function& constructor, ClassDef& platformParent);
    Function* FindPlatformParentEmptyConstructor(const ClassDef& platformParent);
    /// Repoint an existing common-part super call (targeting Object.init) to the platform
    /// parent's empty constructor. Only repoints when the callee's parent is the root class.
    void RepointExistingSuperCall(Apply& superCall, Function& platformConstructor);
    /// Insert a new super call to the platform parent's empty constructor at the head of the
    /// constructor's entry block, for common-part constructors that had no super call.
    void InsertSuperCall(Function& constructor, Function& platformConstructor, Block& entryBlock);

    Package& pkg;
    CHIRBuilder& builder;
};

} // namespace Cangjie::CHIR

#endif