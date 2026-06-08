// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_UTILS_SAFEHANDLE_H
#define CANGJIE_UTILS_SAFEHANDLE_H

namespace Cangjie {

class SafeHandle {
public:
    SafeHandle(void* handle) noexcept;
    SafeHandle(SafeHandle&& other) noexcept;
    SafeHandle& operator=(SafeHandle&& other) noexcept;
    SafeHandle(const SafeHandle&) = delete;
    SafeHandle& operator=(const SafeHandle&) = delete;
    ~SafeHandle();

    void* get() const noexcept;

private:
    void* handle;
};

} // namespace Cangjie

#endif
