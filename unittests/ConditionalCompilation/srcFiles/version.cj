// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// Exercises cjc_version comparison operators.

@When[cjc_version > "0.0.0"]
func versionGt() {}

@When[cjc_version >= "0.0.0"]
func versionGe() {}

@When[cjc_version < "99.0.0"]
func versionLt() {}

@When[cjc_version <= "99.0.0"]
func versionLe() {}

@When[cjc_version == "0.0.1"]
func versionEqOk() {}

@When[cjc_version != "99.0.0"]
func versionNe() {}

main() {}
