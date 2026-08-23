// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <gtest/gtest.h>

#include "cangjie/CHIR/IR/CHIRBuilder.h"

using namespace Cangjie::CHIR;

// VArrayType::GetSize() must preserve large (> UINT32_MAX) sizes. It used to
// return static_cast<unsigned>(size), truncating the length to 32 bits (e.g.
// 5000000000 -> 705032704), which corrupted the length later used by CodeGen
// (CGVArrayType::GenLLVMType, VArrayExprImpl and the init size literal in
// TranslateArrayExpr).
TEST(VArrayTypeTest, LargeSizeIsNotTruncated)
{
    std::unordered_map<unsigned int, std::string> fileNameMap;
    CHIRContext cctx(&fileNameMap);
    CHIRBuilder builder(cctx);
    auto int64Ty = builder.GetInt64Ty();

    const int64_t largeSize = 5000000000; // > UINT32_MAX, < 2^63
    auto varrTy = builder.GetType<VArrayType>(int64Ty, largeSize);

    // The size must survive GetSize() intact (int64_t), not wrap into 32-bit.
    EXPECT_EQ(varrTy->GetSize(), largeSize);
    EXPECT_EQ(varrTy->GetSize(), static_cast<int64_t>(5000000000LL));
    // The old unsigned truncation would have yielded 5000000000 % 2^32.
    EXPECT_NE(varrTy->GetSize(), 705032704);

    // Same for a nested VArray whose inner length is large.
    auto innerTy = builder.GetType<VArrayType>(int64Ty, largeSize);
    auto outerTy = builder.GetType<VArrayType>(innerTy, 1);
    EXPECT_EQ(outerTy->GetSize(), 1);
    EXPECT_EQ(static_cast<VArrayType*>(outerTy->GetElementType())->GetSize(), largeSize);
}