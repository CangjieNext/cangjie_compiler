// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/// @file
/// This file declares add indirect extend in CHIR.

#ifndef CANGJIE_CHIR_TRANSFORMATION_ADD_INDIRECT_EXTEND_H
#define CANGJIE_CHIR_TRANSFORMATION_ADD_INDIRECT_EXTEND_H

#include "cangjie/CHIR/Type/Type.h"
#include <vector>

namespace Cangjie::CHIR {
class CHIRBuilder;
class ExtendDef;
class Package;

/// @brief Add indirect extend declarations for classes that inherit from classes with extends.
///
/// Given:
///   class B<B1,B2> <: A<B2,Q<B1>,B2>{}
///   extend<R1,R2,R3> A<SSS<R1>,R2,R3> <: I<R1,SSS<R2>>{}
///
/// This function generates:
///   extend<K4,K1> B<K4,SSS<K1>> <: A<SSS<K1>,Q<K4>,SSS<K1>> <: I<K1,SSS<Q<K4>>> {}
///
/// @param pkg The CHIR package containing the declarations.
/// @param builder The CHIR builder for creating new types and nodes.
/// @return The list of newly created ExtendDefs (for building vtable).
std::vector<ExtendDef*> AddIndirectExtend(Package& pkg, CHIRBuilder& builder);

} // namespace Cangjie::CHIR

#endif // CANGJIE_CHIR_TRANSFORMATION_ADD_INDIRECT_EXTEND_H
