// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares the class which used for validating correctness of ast sema ty.
 */

#ifndef CANGJIE_AST_TYPE_VALIDATOR_H
#define CANGJIE_AST_TYPE_VALIDATOR_H

namespace Cangjie {    
class DiagnosticEngine;
}

namespace Cangjie::AST {
void ValidateUsedNodes(DiagnosticEngine& diagEngine, struct Package& pkg);
} // namespace Cangjie::AST

#endif
