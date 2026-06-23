// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.


#ifndef CANGJIE_SEMA_OBJC_UTILS_H
#define CANGJIE_SEMA_OBJC_UTILS_H

namespace Cangjie::Interop::ObjC {
enum class ObjCFunctionType { STATIC, INSTANCE };
enum class GenerationTarget { HEADER, SOURCE, BOTH };
enum class FunctionListFormat { DECLARATION, STATIC_REF, CANGJIE_DECL };
enum class OptionalBlockOp { OPEN, CLOSE, NONE };
}
#endif // CANGJIE_SEMA_OBJC_GENERATOR_H