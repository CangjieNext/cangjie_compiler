// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.


#ifndef CANGJIE_SEMA_NATIVE_FFI_OBJC_INTEROP_UTILS
#define CANGJIE_SEMA_NATIVE_FFI_OBJC_INTEROP_UTILS

#include "NativeFFI/ObjC/AfterTypeCheck/Interop/Context.h"

namespace Cangjie::Interop::ObjC {
enum class ObjCFunctionType { STATIC, INSTANCE };
enum class GenerationTarget { HEADER, SOURCE, BOTH };
enum class FunctionListFormat { DECLARATION, STATIC_REF, CANGJIE_DECL };
enum class OptionalBlockOp { OPEN, CLOSE, NONE };

template <typename T>
std::string JoinVec(const std::vector<T>& vec, std::function<std::string(const T&)> trans,
    const std::string& sep = ", ", const std::string& pre = "", const std::string& suf = "", bool force = true)
{
    std::stringstream ss;
    auto it = vec.cbegin();
    if (it == vec.end()) {
        if (!force) {
            return "";
        }
        ss << pre << suf;
        return ss.str();
    }
    ss << pre;
    do {
        ss << trans(*it);
        ++it;
        if (it != vec.cend()) {
            ss << sep;
        }
    } while (it != vec.cend());
    ss << suf;
    return ss.str();
}

}
#endif // CANGJIE_SEMA_NATIVE_FFI_OBJC_INTEROP_UTILS