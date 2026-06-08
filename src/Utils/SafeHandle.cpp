// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/Utils/SafeHandle.h"

#include "cangjie/Macro/InvokeUtil.h"

using namespace Cangjie;

SafeHandle::SafeHandle(void* handle) noexcept : handle(handle) {}

SafeHandle::SafeHandle(SafeHandle&& other) noexcept : handle(other.handle)
{
    other.handle = nullptr;
}

SafeHandle& SafeHandle::operator=(SafeHandle&& other) noexcept
{
    if (this != &other) {
        if (handle != nullptr) {
            InvokeRuntime::CloseSymbolTable(handle);
        }
        handle = other.handle;
        other.handle = nullptr;
    }
    return *this;
}

SafeHandle::~SafeHandle()
{
    if (handle != nullptr) {
        InvokeRuntime::CloseSymbolTable(handle);
    }
}

void* SafeHandle::get() const noexcept
{
    return handle;
}