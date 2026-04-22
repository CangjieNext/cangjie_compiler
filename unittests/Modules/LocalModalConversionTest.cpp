// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "gtest/gtest.h"

#include "cangjie/AST/Types.h"
#include "flatbuffers/ModuleFormat_generated.h"
#include "../src/Modules/ASTSerialization/ASTSerializeUtils.h"

using namespace Cangjie;
using namespace AST;

class LocalModalConversionTest : public testing::Test {
protected:
    void SetUp() override
    {
    }
};

TEST_F(LocalModalConversionTest, ToFormatLocalModal)
{
    EXPECT_EQ(PackageFormat::LocalModal::NOT, ToFormatLocalModal(LocalModal::NOT));
    EXPECT_EQ(PackageFormat::LocalModal::FULL, ToFormatLocalModal(LocalModal::FULL));
    EXPECT_EQ(PackageFormat::LocalModal::HALF, ToFormatLocalModal(LocalModal::HALF));
}

TEST_F(LocalModalConversionTest, FromFormatLocalModal)
{
    EXPECT_EQ(LocalModal::NOT, FromFormatLocalModal(PackageFormat::LocalModal::NOT));
    EXPECT_EQ(LocalModal::FULL, FromFormatLocalModal(PackageFormat::LocalModal::FULL));
    EXPECT_EQ(LocalModal::HALF, FromFormatLocalModal(PackageFormat::LocalModal::HALF));
}
