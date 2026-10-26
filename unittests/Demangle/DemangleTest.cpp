// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "gtest/gtest.h"

#define private public

#include "Demangler.h"
#include "StdString.h"

using namespace Cangjie;

TEST(DemangleTest, PackageNameColonDelimiter)
{
    Demangler<StdString> demangler_1("4abc:");
    auto result_1 = demangler_1.DemanglePackageName();
    EXPECT_STREQ("abc:", result_1.pkgName.Str());

    Demangler<StdString> demangler_2("5abc:x");
    auto result_2 = demangler_2.DemanglePackageName();
    EXPECT_STREQ("abc::x", result_2.pkgName.Str());

    Demangler<StdString> demangler_3("7abc:xyz");
    auto result_3 = demangler_3.DemanglePackageName();
    EXPECT_STREQ("abc::xyz", result_3.pkgName.Str());
}

TEST(DemangleTest, PackageNameTrailingModuleSplit)
{
    // The module split character '$' at the end of a package name must be
    // converted to '/', including when it is the last character.
    Demangler<StdString> demangler("2a$");
    auto result = demangler.DemanglePackageName();
    EXPECT_STREQ("a/", result.pkgName.Str());
}

TEST(DemangleTest, PrimitiveFloat16)
{
    // Float16 is encoded as "Dh". A lone 'D' is not a valid primitive type.
    Demangler<StdString> float16("Dh");
    auto result = float16.Demangle(true);
    EXPECT_TRUE(result.IsValid());
    EXPECT_STREQ("Float16", result.GetFullName("::").Str());

    Demangler<StdString> loneD("D");
    auto invalid = loneD.Demangle(true);
    EXPECT_FALSE(invalid.IsValid());
}

TEST(DemangleTest, TupleAndVArrayManglingNumber)
{
    // Regress the base62 mangling-number boundary check: "0_" decodes to 1.
    Demangler<StdString> tuple("T0_iE");
    auto tupleResult = tuple.Demangle(true);
    EXPECT_TRUE(tupleResult.IsValid());
    EXPECT_STREQ("Tuple<Int32>", tupleResult.GetFullName("::").Str());

    Demangler<StdString> varray("V0_i");
    auto varrayResult = varray.Demangle(true);
    EXPECT_TRUE(varrayResult.IsValid());
    EXPECT_STREQ("VArray<Int32,$1>", varrayResult.GetFullName("::").Str());
}

TEST(DemangleTest, GenericConstraintsNameTrailingArgs)
{
    // ReplaceString pairs its loop condition with the current string length:
    // a pattern near the tail must still be replaced when earlier replacements
    // changed the length ("," -> " &" inside GetGenericConstraintsName).
    DemangleInfo<StdString> trailingComma{ "C", TypeKind::NAME };
    trailingComma.args = StdString("a,b,");
    EXPECT_STREQ(", C <: a &b &", trailingComma.GetGenericConstraintsName().Str());

    DemangleInfo<StdString> plainArgs{ "C", TypeKind::NAME };
    plainArgs.args = StdString("a,b");
    EXPECT_STREQ(", C <: a &b", plainArgs.GetGenericConstraintsName().Str());
}
