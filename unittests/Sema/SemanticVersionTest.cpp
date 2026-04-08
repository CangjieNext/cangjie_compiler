// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * Semantic version related unit tests for APILevel and IfAvailable annotations.
 */

#include <string>
#include <vector>
#include "gtest/gtest.h"
#include "cangjie/Sema/Plugin/PluginCustomAnnoChecker.h"
#include "cangjie/Basic/StringConvertor.h"

using namespace Cangjie;
using namespace Cangjie::PluginCheck;

// Helper function to parse semantic version (duplicated from implementation for testing)
namespace {
std::optional<unsigned long long> Stoull(const std::string& s) {
    try {
        return std::stoull(s);
    } catch (...) {
        return std::nullopt;
    }
}

SemanticVersion ParseSemanticVersion(const std::string& s)
{
    SemanticVersion version;

    // Split the string by dots
    std::vector<std::string> parts;
    size_t start = 0;
    size_t end = s.find('.');

    while (end != std::string::npos) {
        parts.push_back(s.substr(start, end - start));
        start = end + 1;
        end = s.find('.', start);
    }
    parts.push_back(s.substr(start));

    // Parse each component
    if (parts.size() >= 1 && !parts[0].empty()) {
        version.major = static_cast<uint32_t>(Stoull(parts[0]).value_or(0));
    }
    if (parts.size() >= 2 && !parts[1].empty()) {
        version.minor = static_cast<uint32_t>(Stoull(parts[1]).value_or(0));
    }
    if (parts.size() >= 3 && !parts[2].empty()) {
        version.patch = static_cast<uint32_t>(Stoull(parts[2]).value_or(0));
    }

    return version;
}
} // namespace

// Test SemanticVersion construction
TEST(SemanticVersionTest, ConstructorTest)
{
    SemanticVersion v1;
    EXPECT_EQ(v1.major, 0);
    EXPECT_EQ(v1.minor, 0);
    EXPECT_EQ(v1.patch, 0);
    EXPECT_TRUE(v1.IsZero());

    SemanticVersion v2(20);
    EXPECT_EQ(v2.major, 20);
    EXPECT_EQ(v2.minor, 0);
    EXPECT_EQ(v2.patch, 0);
    EXPECT_FALSE(v2.IsZero());

    SemanticVersion v3(20, 1);
    EXPECT_EQ(v3.major, 20);
    EXPECT_EQ(v3.minor, 1);
    EXPECT_EQ(v3.patch, 0);

    SemanticVersion v4(20, 1, 5);
    EXPECT_EQ(v4.major, 20);
    EXPECT_EQ(v4.minor, 1);
    EXPECT_EQ(v4.patch, 5);
}

// Test parsing from string
TEST(SemanticVersionTest, ParseSimpleVersion)
{
    auto v = ParseSemanticVersion("20");
    EXPECT_EQ(v.major, 20);
    EXPECT_EQ(v.minor, 0);
    EXPECT_EQ(v.patch, 0);
    EXPECT_EQ(v.ToString(), "20.0.0");
}

TEST(SemanticVersionTest, ParseTwoPartVersion)
{
    auto v = ParseSemanticVersion("20.1");
    EXPECT_EQ(v.major, 20);
    EXPECT_EQ(v.minor, 1);
    EXPECT_EQ(v.patch, 0);
    EXPECT_EQ(v.ToString(), "20.1.0");
}

TEST(SemanticVersionTest, ParseFullVersion)
{
    auto v1 = ParseSemanticVersion("20.0.0");
    EXPECT_EQ(v1.major, 20);
    EXPECT_EQ(v1.minor, 0);
    EXPECT_EQ(v1.patch, 0);
    EXPECT_EQ(v1.ToString(), "20.0.0");

    auto v2 = ParseSemanticVersion("20.1.5");
    EXPECT_EQ(v2.major, 20);
    EXPECT_EQ(v2.minor, 1);
    EXPECT_EQ(v2.patch, 5);
    EXPECT_EQ(v2.ToString(), "20.1.5");

    auto v3 = ParseSemanticVersion("21.10.999");
    EXPECT_EQ(v3.major, 21);
    EXPECT_EQ(v3.minor, 10);
    EXPECT_EQ(v3.patch, 999);
    EXPECT_EQ(v3.ToString(), "21.10.999");
}

// Test equality comparison
TEST(SemanticVersionTest, EqualityComparison)
{
    SemanticVersion v1(20, 0, 0);
    SemanticVersion v2(20, 0, 0);
    SemanticVersion v3(20, 1, 0);

    EXPECT_TRUE(v1 == v2);
    EXPECT_FALSE(v1 == v3);
    EXPECT_TRUE(v1 != v3);
    EXPECT_FALSE(v1 != v2);

    // Test that "20" parsed equals "20.0.0" parsed
    auto vSimple = ParseSemanticVersion("20");
    auto vFull = ParseSemanticVersion("20.0.0");
    EXPECT_TRUE(vSimple == vFull);
}

// Test less than comparison
TEST(SemanticVersionTest, LessThanComparison)
{
    SemanticVersion v1(19, 0, 0);
    SemanticVersion v2(20, 0, 0);
    SemanticVersion v3(20, 1, 0);
    SemanticVersion v4(20, 1, 5);
    SemanticVersion v5(21, 0, 0);

    // Major version comparison
    EXPECT_TRUE(v1 < v2);
    EXPECT_FALSE(v2 < v1);

    // Minor version comparison (same major)
    EXPECT_TRUE(v2 < v3);
    EXPECT_FALSE(v3 < v2);

    // Patch version comparison (same major and minor)
    EXPECT_TRUE(v3 < v4);
    EXPECT_FALSE(v4 < v3);

    // Mixed comparisons
    EXPECT_TRUE(v4 < v5);
    EXPECT_TRUE(v1 < v5);

    // Not less than itself
    EXPECT_FALSE(v2 < v2);
}

// Test greater than comparison
TEST(SemanticVersionTest, GreaterThanComparison)
{
    SemanticVersion v1(20, 0, 0);
    SemanticVersion v2(20, 1, 0);
    SemanticVersion v3(21, 0, 0);

    EXPECT_TRUE(v2 > v1);
    EXPECT_TRUE(v3 > v2);
    EXPECT_TRUE(v3 > v1);

    EXPECT_FALSE(v1 > v2);
    EXPECT_FALSE(v1 > v1);
}

// Test less than or equal comparison
TEST(SemanticVersionTest, LessThanOrEqualComparison)
{
    SemanticVersion v1(20, 0, 0);
    SemanticVersion v2(20, 0, 0);
    SemanticVersion v3(20, 1, 0);

    EXPECT_TRUE(v1 <= v2);  // Equal
    EXPECT_TRUE(v1 <= v3);  // Less than
    EXPECT_FALSE(v3 <= v1);
}

// Test greater than or equal comparison
TEST(SemanticVersionTest, GreaterThanOrEqualComparison)
{
    SemanticVersion v1(20, 0, 0);
    SemanticVersion v2(20, 0, 0);
    SemanticVersion v3(20, 1, 0);

    EXPECT_TRUE(v1 >= v2);  // Equal
    EXPECT_TRUE(v3 >= v1);  // Greater than
    EXPECT_FALSE(v1 >= v3);
}

// Test version ordering
TEST(SemanticVersionTest, VersionOrdering)
{
    std::vector<SemanticVersion> versions = {
        ParseSemanticVersion("19.0.0"),
        ParseSemanticVersion("20.0.0"),
        ParseSemanticVersion("20.1.0"),
        ParseSemanticVersion("20.1.5"),
        ParseSemanticVersion("21.0.0"),
    };

    // Verify ordering
    for (size_t i = 0; i < versions.size() - 1; ++i) {
        EXPECT_TRUE(versions[i] < versions[i + 1])
            << versions[i].ToString() << " should be < " << versions[i + 1].ToString();
        EXPECT_FALSE(versions[i] > versions[i + 1]);
        EXPECT_FALSE(versions[i] == versions[i + 1]);
    }
}

// Test runtime encoding scheme
TEST(SemanticVersionTest, RuntimeEncoding)
{
    auto encode = [](const SemanticVersion& v) {
        return static_cast<uint64_t>(v.major) * 1000000ULL +
               static_cast<uint64_t>(v.minor) * 1000ULL +
               static_cast<uint64_t>(v.patch);
    };

    EXPECT_EQ(encode(ParseSemanticVersion("19.0.0")), 19000000ULL);
    EXPECT_EQ(encode(ParseSemanticVersion("20.0.0")), 20000000ULL);
    EXPECT_EQ(encode(ParseSemanticVersion("20.1.0")), 20001000ULL);
    EXPECT_EQ(encode(ParseSemanticVersion("20.1.5")), 20001005ULL);
    EXPECT_EQ(encode(ParseSemanticVersion("21.0.0")), 21000000ULL);

    // Verify encoding maintains ordering
    uint64_t e1 = encode(ParseSemanticVersion("20.0.0"));
    uint64_t e2 = encode(ParseSemanticVersion("20.1.0"));
    uint64_t e3 = encode(ParseSemanticVersion("20.1.5"));
    uint64_t e4 = encode(ParseSemanticVersion("21.0.0"));

    EXPECT_LT(e1, e2);
    EXPECT_LT(e2, e3);
    EXPECT_LT(e3, e4);
}

// Test backward compatibility
TEST(SemanticVersionTest, BackwardCompatibility)
{
    // Integer literal treated as major version
    SemanticVersion vInt(20);
    auto vParsed = ParseSemanticVersion("20");
    auto vFull = ParseSemanticVersion("20.0.0");

    EXPECT_EQ(vInt, vParsed);
    EXPECT_EQ(vInt, vFull);
    EXPECT_EQ(vParsed, vFull);

    // All should have same encoding
    auto encode = [](const SemanticVersion& v) {
        return static_cast<uint64_t>(v.major) * 1000000ULL +
               static_cast<uint64_t>(v.minor) * 1000ULL +
               static_cast<uint64_t>(v.patch);
    };

    EXPECT_EQ(encode(vInt), encode(vParsed));
    EXPECT_EQ(encode(vInt), encode(vFull));
    EXPECT_EQ(encode(vInt), 20000000ULL);
}

// Test IsZero method
TEST(SemanticVersionTest, IsZeroMethod)
{
    SemanticVersion v1;
    EXPECT_TRUE(v1.IsZero());

    SemanticVersion v2(0, 0, 0);
    EXPECT_TRUE(v2.IsZero());

    SemanticVersion v3(1, 0, 0);
    EXPECT_FALSE(v3.IsZero());

    SemanticVersion v4(0, 1, 0);
    EXPECT_FALSE(v4.IsZero());

    SemanticVersion v5(0, 0, 1);
    EXPECT_FALSE(v5.IsZero());
}

// Test ToString method
TEST(SemanticVersionTest, ToStringMethod)
{
    EXPECT_EQ(SemanticVersion(20, 0, 0).ToString(), "20.0.0");
    EXPECT_EQ(SemanticVersion(20, 1, 0).ToString(), "20.1.0");
    EXPECT_EQ(SemanticVersion(20, 1, 5).ToString(), "20.1.5");
    EXPECT_EQ(SemanticVersion(21, 10, 999).ToString(), "21.10.999");
}

// Test edge cases
TEST(SemanticVersionTest, EdgeCases)
{
    // Empty string
    auto v1 = ParseSemanticVersion("");
    EXPECT_TRUE(v1.IsZero());

    // Single zero
    auto v2 = ParseSemanticVersion("0");
    EXPECT_EQ(v2.major, 0);
    EXPECT_EQ(v2.minor, 0);
    EXPECT_EQ(v2.patch, 0);

    // Large numbers
    auto v3 = ParseSemanticVersion("999.999.999");
    EXPECT_EQ(v3.major, 999);
    EXPECT_EQ(v3.minor, 999);
    EXPECT_EQ(v3.patch, 999);

    // Trailing dots (should be handled gracefully)
    auto v4 = ParseSemanticVersion("20.");
    EXPECT_EQ(v4.major, 20);
    EXPECT_EQ(v4.minor, 0);
    EXPECT_EQ(v4.patch, 0);
}

// Test PluginCustomAnnoInfo structure
TEST(SemanticVersionTest, PluginCustomAnnoInfoIntegration)
{
    PluginCustomAnnoInfo info1;
    EXPECT_TRUE(info1.since.IsZero());

    PluginCustomAnnoInfo info2;
    info2.since = SemanticVersion(20, 1, 5);
    EXPECT_FALSE(info2.since.IsZero());
    EXPECT_EQ(info2.since.ToString(), "20.1.5");

    // Test comparison
    PluginCustomAnnoInfo info3;
    info3.since = SemanticVersion(21, 0, 0);
    EXPECT_TRUE(info2.since < info3.since);
}

// Test real-world version scenarios
TEST(SemanticVersionTest, RealWorldScenarios)
{
    // Android API level style
    auto api19 = ParseSemanticVersion("19");
    auto api20 = ParseSemanticVersion("20");
    auto api21 = ParseSemanticVersion("21");

    EXPECT_TRUE(api19 < api20);
    EXPECT_TRUE(api20 < api21);

    // HarmonyOS style versioning
    auto harmony4_0_0 = ParseSemanticVersion("4.0.0");
    auto harmony4_1_0 = ParseSemanticVersion("4.1.0");
    auto harmony5_0_0 = ParseSemanticVersion("5.0.0");

    EXPECT_TRUE(harmony4_0_0 < harmony4_1_0);
    EXPECT_TRUE(harmony4_1_0 < harmony5_0_0);

    // Patch version updates
    auto v1_0_0 = ParseSemanticVersion("1.0.0");
    auto v1_0_1 = ParseSemanticVersion("1.0.1");
    auto v1_0_10 = ParseSemanticVersion("1.0.10");

    EXPECT_TRUE(v1_0_0 < v1_0_1);
    EXPECT_TRUE(v1_0_1 < v1_0_10);
}

// ============================================================
// Tests for --cfg APILevel scenario
//
// Simulates the CheckLevel logic triggered by --cfg APILevel_level=X.Y.Z:
//   - globalLevel  : parsed from --cfg APILevel_level (i.e., scopeLevel when no IfAvailable)
//   - targetSince  : from the @APILevel(since: "X.Y.Z") annotation on the referenced API
//   - Rule: if targetSince > scopeLevel → inaccessible (should report error)
//           if targetSince <= scopeLevel → accessible (no error)
// ============================================================

namespace {
// Helper: simulate CheckLevel return value.
// Returns true when the API is accessible (no error should be reported),
// false when the API level is too high (error should be reported).
bool IsAccessible(const SemanticVersion& targetSince, const SemanticVersion& scopeLevel)
{
    // Mirrors PluginCustomAnnoChecker::CheckLevel: error when targetSince > scopeLevel
    return !(targetSince > scopeLevel);
}
} // namespace

// --cfg APILevel_level=20 (integer style)
TEST(CfgAPILevelTest, IntegerStyleCfg_Accessible)
{
    auto globalLevel = ParseSemanticVersion("20");   // --cfg APILevel_level=20
    auto targetSince = ParseSemanticVersion("20");   // @APILevel(since: "20")
    EXPECT_TRUE(IsAccessible(targetSince, globalLevel));
}

TEST(CfgAPILevelTest, IntegerStyleCfg_Inaccessible)
{
    auto globalLevel = ParseSemanticVersion("19");   // --cfg APILevel_level=19
    auto targetSince = ParseSemanticVersion("20");   // @APILevel(since: "20")
    EXPECT_FALSE(IsAccessible(targetSince, globalLevel));
}

// --cfg APILevel_level=20.0.0 (full x.y.z style)
TEST(CfgAPILevelTest, FullVersionCfg_ExactMatch_Accessible)
{
    auto globalLevel = ParseSemanticVersion("20.0.0");
    auto targetSince = ParseSemanticVersion("20.0.0");
    EXPECT_TRUE(IsAccessible(targetSince, globalLevel));
}

TEST(CfgAPILevelTest, FullVersionCfg_HigherTarget_Inaccessible)
{
    auto globalLevel = ParseSemanticVersion("20.0.0");  // --cfg APILevel_level=20.0.0
    auto targetSince = ParseSemanticVersion("20.1.0");  // @APILevel(since: "20.1.0")
    EXPECT_FALSE(IsAccessible(targetSince, globalLevel));
}

TEST(CfgAPILevelTest, FullVersionCfg_LowerTarget_Accessible)
{
    auto globalLevel = ParseSemanticVersion("20.1.0");  // --cfg APILevel_level=20.1.0
    auto targetSince = ParseSemanticVersion("20.0.0");  // @APILevel(since: "20.0.0")
    EXPECT_TRUE(IsAccessible(targetSince, globalLevel));
}

// Patch-level boundary
TEST(CfgAPILevelTest, PatchBoundary_ExactMatch_Accessible)
{
    auto globalLevel = ParseSemanticVersion("20.1.5");
    auto targetSince = ParseSemanticVersion("20.1.5");
    EXPECT_TRUE(IsAccessible(targetSince, globalLevel));
}

TEST(CfgAPILevelTest, PatchBoundary_OneHigher_Inaccessible)
{
    auto globalLevel = ParseSemanticVersion("20.1.5");  // --cfg APILevel_level=20.1.5
    auto targetSince = ParseSemanticVersion("20.1.6");  // @APILevel(since: "20.1.6")
    EXPECT_FALSE(IsAccessible(targetSince, globalLevel));
}

TEST(CfgAPILevelTest, PatchBoundary_OneLower_Accessible)
{
    auto globalLevel = ParseSemanticVersion("20.1.5");  // --cfg APILevel_level=20.1.5
    auto targetSince = ParseSemanticVersion("20.1.4");  // @APILevel(since: "20.1.4")
    EXPECT_TRUE(IsAccessible(targetSince, globalLevel));
}

// Integer --cfg is equivalent to x.0.0
TEST(CfgAPILevelTest, IntegerEquivalentToMajorZeroZero)
{
    auto cfgInt = ParseSemanticVersion("20");      // --cfg APILevel_level=20
    auto cfgFull = ParseSemanticVersion("20.0.0"); // --cfg APILevel_level=20.0.0

    // Both forms must yield the same accessibility decisions
    auto targetBelow = ParseSemanticVersion("19.9.9");
    auto targetEqual = ParseSemanticVersion("20.0.0");
    auto targetAbove = ParseSemanticVersion("20.0.1");

    EXPECT_EQ(IsAccessible(targetBelow, cfgInt), IsAccessible(targetBelow, cfgFull));
    EXPECT_EQ(IsAccessible(targetEqual, cfgInt), IsAccessible(targetEqual, cfgFull));
    EXPECT_EQ(IsAccessible(targetAbove, cfgInt), IsAccessible(targetAbove, cfgFull));
}

// Two-part --cfg (e.g., --cfg APILevel_level=20.1)
TEST(CfgAPILevelTest, TwoPartCfg_Accessible)
{
    auto globalLevel = ParseSemanticVersion("20.1");    // --cfg APILevel_level=20.1
    auto targetSince = ParseSemanticVersion("20.1.0");  // @APILevel(since: "20.1.0")
    EXPECT_TRUE(IsAccessible(targetSince, globalLevel));
}

TEST(CfgAPILevelTest, TwoPartCfg_Inaccessible)
{
    auto globalLevel = ParseSemanticVersion("20.1");    // --cfg APILevel_level=20.1
    auto targetSince = ParseSemanticVersion("20.2.0");  // @APILevel(since: "20.2.0")
    EXPECT_FALSE(IsAccessible(targetSince, globalLevel));
}

// Cross-major boundary
TEST(CfgAPILevelTest, CrossMajor_OlderCfg_Inaccessible)
{
    auto globalLevel = ParseSemanticVersion("20.9.9");  // --cfg APILevel_level=20.9.9
    auto targetSince = ParseSemanticVersion("21.0.0");  // @APILevel(since: "21.0.0")
    EXPECT_FALSE(IsAccessible(targetSince, globalLevel));
}

TEST(CfgAPILevelTest, CrossMajor_NewerCfg_Accessible)
{
    auto globalLevel = ParseSemanticVersion("21.0.0");  // --cfg APILevel_level=21.0.0
    auto targetSince = ParseSemanticVersion("20.9.9");  // @APILevel(since: "20.9.9")
    EXPECT_TRUE(IsAccessible(targetSince, globalLevel));
}

// No --cfg provided: globalLevel is zero, optionWithLevel==false (CheckLevel returns true immediately).
// Modeled here as: zero globalLevel should never block access via the IsZero guard.
TEST(CfgAPILevelTest, NoCfgProvided_GlobalLevelIsZero)
{
    SemanticVersion globalLevel;  // default-constructed = 0.0.0 (IsZero)
    EXPECT_TRUE(globalLevel.IsZero());
    // When optionWithLevel is false, CheckLevel returns true regardless; ensure zero is detectable.
    auto targetSince = ParseSemanticVersion("21.0.0");
    // The real CheckLevel would skip the check; here we verify the zero sentinel.
    EXPECT_FALSE(targetSince.IsZero());
}

// Batch accessibility: multiple APIs at different levels with a fixed --cfg
TEST(CfgAPILevelTest, BatchAccessibility_FixedCfg)
{
    auto globalLevel = ParseSemanticVersion("20.1.5");  // --cfg APILevel_level=20.1.5

    // accessible APIs (since <= 20.1.5)
    EXPECT_TRUE(IsAccessible(ParseSemanticVersion("19.0.0"), globalLevel));
    EXPECT_TRUE(IsAccessible(ParseSemanticVersion("20.0.0"), globalLevel));
    EXPECT_TRUE(IsAccessible(ParseSemanticVersion("20.1.0"), globalLevel));
    EXPECT_TRUE(IsAccessible(ParseSemanticVersion("20.1.5"), globalLevel));

    // inaccessible APIs (since > 20.1.5)
    EXPECT_FALSE(IsAccessible(ParseSemanticVersion("20.1.6"), globalLevel));
    EXPECT_FALSE(IsAccessible(ParseSemanticVersion("20.2.0"), globalLevel));
    EXPECT_FALSE(IsAccessible(ParseSemanticVersion("21.0.0"), globalLevel));
}
