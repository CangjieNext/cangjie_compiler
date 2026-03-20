// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_CHIR_PACKAGE_SPEC_H
#define CANGJIE_CHIR_PACKAGE_SPEC_H

#include <set>
#include <string>

namespace Cangjie::CHIR {

class PackageSpec {
public:
    PackageSpec(const std::string& name, const std::set<std::string>& features)
        : name(name), features(features)
    {}

    ~PackageSpec() = default;

    // Getters
    const std::set<std::string>& GetFeatures() const { return features; }

private:
    std::string name;
    std::set<std::string> features;
};

} // namespace Cangjie::CHIR

#endif // CANGJIE_CHIR_PACKAGE_SPEC_H
