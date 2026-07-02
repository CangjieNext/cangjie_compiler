// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_CHIR_TRANSFORMATION_PATCH_CJMP_IMPL_BODIES_H
#define CANGJIE_CHIR_TRANSFORMATION_PATCH_CJMP_IMPL_BODIES_H

#include "cangjie/CHIR/IR/CHIRBuilder.h"
#include "cangjie/CHIR/IR/Package.h"

namespace Cangjie::CHIR {

class PatchCJMPImplBodies {
public:
    PatchCJMPImplBodies(Package& pkg, CHIRBuilder& builder);

    void Run();
    void RunCtorFieldInitOnly();

private:
    void PatchFieldReferences(Function& func, ClassDef& implClassDef);
    Expression* TryPatchLoad(Load& load, ClassDef& implClassDef);
    Expression* TryPatchLoadByName(Load& load, ClassDef& implClassDef);
    void TryPatchStoreElementByName(StoreElementByName& storeByName, ClassDef& implClassDef);
    Expression* TryPatchStoreElementRef(StoreElementRef& store, ClassDef& implClassDef);
    void PatchConstructorFieldInit(Function& ctor, ClassDef& implClassDef);

    Package& pkg;
    CHIRBuilder& builder;
};

} // namespace Cangjie::CHIR

#endif