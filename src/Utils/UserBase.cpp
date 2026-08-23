// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "UserBase.h"

#include <fstream>
#include <iostream>

#include "cangjie/Utils/CheckUtils.h"
#include "cangjie/Utils/FileUtil.h"

using namespace Cangjie;

std::string UserBase::GetResult() const
{
    if (!enable) {
        return "";
    }
    return GetJson();
}

void UserBase::OutputResult() const noexcept
{
    if (!enable) {
        return;
    }
#ifndef CANGJIE_ENABLE_GCOV
    try {
#endif
        std::string result = GetResult();
        WriteJson(result, GetSuffix());
#ifndef CANGJIE_ENABLE_GCOV
    } catch (...) {
        std::cerr << "Get an exception while running function 'OutputResult' !!!\n" << std::endl;
    }
#endif
}

void UserBase::WriteJson(const std::string& context, const std::string& suffix) const
{
    // Replace every directory separator, not just the first one: a name that still contains one
    // would send the profile into a subdirectory that does not exist, and the write would then
    // fail silently below.
    std::string name = packageName;
    for (size_t pos = name.find_first_of(DIR_SEPARATOR); pos != std::string::npos;
         pos = name.find_first_of(DIR_SEPARATOR, pos + 1)) {
        name[pos] = '-';
    }
    std::string filename = name + suffix;
    std::ofstream out(FileUtil::JoinPath(outputDir, filename).c_str());
    if (!out) {
        return;
    }
    out.write(context.c_str(), static_cast<long>(context.size()));
    out.close();
}

void UserBase::Enable(bool en)
{
    enable = en;
}

bool UserBase::IsEnable() const
{
    return enable;
}

void UserBase::SetPackageName(const std::string& name)
{
    packageName = name;
}

void UserBase::SetOutputDir(const std::string& path)
{
    if (FileUtil::IsDir(path)) {
        outputDir = path;
    } else {
        outputDir = FileUtil::GetDirPath(path);
    }
}