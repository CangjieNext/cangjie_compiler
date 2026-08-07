// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "../src/Sema/Plugin/ParseJson.h"
#include "gtest/gtest.h"
#include <string>
#include <vector>

// These helpers are defined in ParseJson.cpp with external linkage but are not
// declared in the header. Forward-declare them here so the tests can exercise
// their early-return / fallback branches directly.
namespace Cangjie {
namespace PluginCheck {
std::string ParseJsonString(size_t& pos, const std::vector<uint8_t>& in);
uint64_t ParseJsonNumber(size_t& pos, const std::vector<uint8_t>& in);
void ParseJsonArray(size_t& pos, const std::vector<uint8_t>& in, Ptr<JsonPair> value);
} // namespace PluginCheck
} // namespace Cangjie

using namespace Cangjie;
using namespace PluginCheck;

class ParseJsonTest : public testing::Test {
protected:
    std::vector<uint8_t> ToBytes(const std::string& str)
    {
        return std::vector<uint8_t>(str.begin(), str.end());
    }
};

TEST_F(ParseJsonTest, ParseEmptyObject)
{
    std::string json = "{}";
    auto bytes = ToBytes(json);
    size_t pos = 0;
    auto obj = ParseJsonObject(pos, bytes);
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->pairs.size(), 0u);
}

TEST_F(ParseJsonTest, ParseSimpleString)
{
    std::string json = R"({"key": "value"})";
    auto bytes = ToBytes(json);
    size_t pos = 0;
    auto obj = ParseJsonObject(pos, bytes);
    ASSERT_NE(obj, nullptr);
    ASSERT_EQ(obj->pairs.size(), 1u);
    EXPECT_EQ(obj->pairs[0]->key, "key");
    ASSERT_EQ(obj->pairs[0]->valueStr.size(), 1u);
    EXPECT_EQ(obj->pairs[0]->valueStr[0], "value");
}

TEST_F(ParseJsonTest, ParseMultipleKeys)
{
    std::string json = R"({"key1": "value1", "key2": "value2"})";
    auto bytes = ToBytes(json);
    size_t pos = 0;
    auto obj = ParseJsonObject(pos, bytes);
    ASSERT_NE(obj, nullptr);
    ASSERT_EQ(obj->pairs.size(), 2u);
    EXPECT_EQ(obj->pairs[0]->key, "key1");
    EXPECT_EQ(obj->pairs[0]->valueStr[0], "value1");
    EXPECT_EQ(obj->pairs[1]->key, "key2");
    EXPECT_EQ(obj->pairs[1]->valueStr[0], "value2");
}

TEST_F(ParseJsonTest, ParseNumber)
{
    std::string json = R"({"count": 42})";
    auto bytes = ToBytes(json);
    size_t pos = 0;
    auto obj = ParseJsonObject(pos, bytes);
    ASSERT_NE(obj, nullptr);
    ASSERT_EQ(obj->pairs.size(), 1u);
    EXPECT_EQ(obj->pairs[0]->key, "count");
    ASSERT_EQ(obj->pairs[0]->valueNum.size(), 1u);
    EXPECT_EQ(obj->pairs[0]->valueNum[0], 42u);
}

TEST_F(ParseJsonTest, ParseNestedObject)
{
    std::string json = R"({"outer": {"inner": "value"}})";
    auto bytes = ToBytes(json);
    size_t pos = 0;
    auto obj = ParseJsonObject(pos, bytes);
    ASSERT_NE(obj, nullptr);
    ASSERT_EQ(obj->pairs.size(), 1u);
    EXPECT_EQ(obj->pairs[0]->key, "outer");
    ASSERT_EQ(obj->pairs[0]->valueObj.size(), 1u);
    auto innerObj = obj->pairs[0]->valueObj[0].get();
    ASSERT_NE(innerObj, nullptr);
    ASSERT_EQ(innerObj->pairs.size(), 1u);
    EXPECT_EQ(innerObj->pairs[0]->key, "inner");
    EXPECT_EQ(innerObj->pairs[0]->valueStr[0], "value");
}

TEST_F(ParseJsonTest, ParseArray)
{
    std::string json = R"({"items": ["a", "b", "c"]})";
    auto bytes = ToBytes(json);
    size_t pos = 0;
    auto obj = ParseJsonObject(pos, bytes);
    ASSERT_NE(obj, nullptr);
    ASSERT_EQ(obj->pairs.size(), 1u);
    EXPECT_EQ(obj->pairs[0]->key, "items");
    ASSERT_EQ(obj->pairs[0]->valueStr.size(), 3u);
    EXPECT_EQ(obj->pairs[0]->valueStr[0], "a");
    EXPECT_EQ(obj->pairs[0]->valueStr[1], "b");
    EXPECT_EQ(obj->pairs[0]->valueStr[2], "c");
}

TEST_F(ParseJsonTest, ParseEscapedDoubleQuotes)
{
    std::string json = R"({"key": "value\"with\"quotes"})";
    auto bytes = ToBytes(json);
    size_t pos = 0;
    auto obj = ParseJsonObject(pos, bytes);
    ASSERT_NE(obj, nullptr);
    ASSERT_EQ(obj->pairs.size(), 1u);
    EXPECT_EQ(obj->pairs[0]->valueStr[0], "value\"with\"quotes");
}

TEST_F(ParseJsonTest, ParseEscapedBackslash)
{
    std::string json = R"({"path": "C:\\Users\\test"})";
    auto bytes = ToBytes(json);
    size_t pos = 0;
    auto obj = ParseJsonObject(pos, bytes);
    ASSERT_NE(obj, nullptr);
    ASSERT_EQ(obj->pairs.size(), 1u);
    EXPECT_EQ(obj->pairs[0]->valueStr[0], "C:\\Users\\test");
}

TEST_F(ParseJsonTest, ParseEscapedNewline)
{
    std::string json = R"({"text": "line1\nline2"})";
    auto bytes = ToBytes(json);
    size_t pos = 0;
    auto obj = ParseJsonObject(pos, bytes);
    ASSERT_NE(obj, nullptr);
    ASSERT_EQ(obj->pairs.size(), 1u);
    EXPECT_EQ(obj->pairs[0]->valueStr[0], "line1\nline2");
}

TEST_F(ParseJsonTest, ParseEscapedTab)
{
    std::string json = R"({"text": "col1\tcol2"})";
    auto bytes = ToBytes(json);
    size_t pos = 0;
    auto obj = ParseJsonObject(pos, bytes);
    ASSERT_NE(obj, nullptr);
    ASSERT_EQ(obj->pairs.size(), 1u);
    EXPECT_EQ(obj->pairs[0]->valueStr[0], "col1\tcol2");
}

TEST_F(ParseJsonTest, ParseComplexEscapedString)
{
    std::string json = R"({"key": "a\"b\\c\nd\te"})";
    auto bytes = ToBytes(json);
    size_t pos = 0;
    auto obj = ParseJsonObject(pos, bytes);
    ASSERT_NE(obj, nullptr);
    ASSERT_EQ(obj->pairs.size(), 1u);
    EXPECT_EQ(obj->pairs[0]->valueStr[0], "a\"b\\c\nd\te");
}

TEST_F(ParseJsonTest, GetJsonStringTest)
{
    std::string json = R"({"name": "test", "value": "123"})";
    auto bytes = ToBytes(json);
    size_t pos = 0;
    auto obj = ParseJsonObject(pos, bytes);
    ASSERT_NE(obj, nullptr);

    auto values = GetJsonString(obj.get(), "name");
    ASSERT_EQ(values.size(), 1u);
    EXPECT_EQ(values[0], "test");

    values = GetJsonString(obj.get(), "value");
    ASSERT_EQ(values.size(), 1u);
    EXPECT_EQ(values[0], "123");

    values = GetJsonString(obj.get(), "notexist");
    EXPECT_EQ(values.size(), 0u);
}

TEST_F(ParseJsonTest, GetJsonObjectTest)
{
    std::string json = R"({"config": {"setting": "enabled"}})";
    auto bytes = ToBytes(json);
    size_t pos = 0;
    auto obj = ParseJsonObject(pos, bytes);
    ASSERT_NE(obj, nullptr);

    auto innerObj = GetJsonObject(obj.get(), "config", 0);
    ASSERT_NE(innerObj, nullptr);
    ASSERT_EQ(innerObj->pairs.size(), 1u);
    EXPECT_EQ(innerObj->pairs[0]->key, "setting");
    EXPECT_EQ(innerObj->pairs[0]->valueStr[0], "enabled");
}

TEST_F(ParseJsonTest, ParseWhitespaceHandling)
{
    std::string json = R"({
        "key1": "value1",
        "key2": 123
    })";
    auto bytes = ToBytes(json);
    size_t pos = 0;
    auto obj = ParseJsonObject(pos, bytes);
    ASSERT_NE(obj, nullptr);
    ASSERT_EQ(obj->pairs.size(), 2u);
    EXPECT_EQ(obj->pairs[0]->key, "key1");
    EXPECT_EQ(obj->pairs[1]->key, "key2");
}

TEST_F(ParseJsonTest, ParseNumberWithoutColon)
{
    std::string json = R"({123})";
    auto bytes = ToBytes(json);
    size_t pos = 0;
    auto obj = ParseJsonObject(pos, bytes);
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->pairs.size(), 0u);
}

TEST_F(ParseJsonTest, ParseObjectWithoutColon)
{
    std::string json = R"({{"inner": "value"}})";
    auto bytes = ToBytes(json);
    size_t pos = 0;
    auto obj = ParseJsonObject(pos, bytes);
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->pairs.size(), 0u);
}

TEST_F(ParseJsonTest, ParseArrayWithoutColon)
{
    std::string json = R"({["a", "b"]})";
    auto bytes = ToBytes(json);
    size_t pos = 0;
    auto obj = ParseJsonObject(pos, bytes);
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->pairs.size(), 0u);
}

TEST_F(ParseJsonTest, TestHeapOverflow)
{
    std::string json = R"({"})";
    auto bytes = ToBytes(json);
    size_t pos = 0;
    ParseJsonObject(pos, bytes);
    // The value of bytes[3] is a random number. If this value falls within the range of '0' to '9', it will result in
    // the position (pos) not being updated to 4.
    EXPECT_EQ(pos, 4);
}

// --- Coverage: ParseJsonString early-return (L48-50) ---
// pos out of range or the char is not a non-escaped '"' -> returns "".
TEST_F(ParseJsonTest, ParseJsonStringEarlyReturnOutOfRange)
{
    auto bytes = ToBytes("abc");
    size_t pos = 100; // out of range
    EXPECT_EQ(ParseJsonString(pos, bytes), "");
}

TEST_F(ParseJsonTest, ParseJsonStringEarlyReturnNotQuote)
{
    auto bytes = ToBytes("abc");
    size_t pos = 0; // not a '"'
    EXPECT_EQ(ParseJsonString(pos, bytes), "");
}

// --- Coverage: CountBackslashesBefore while body (L25-28) ---
// An odd run of backslashes before '"' means the quote is escaped -> not a terminator.
// Feeding an escaped backslash (a double-backslash in the JSON source) exercises the
// backslash-counting loop body inside CountBackslashesBefore.
TEST_F(ParseJsonTest, ParseJsonStringEscapedBackslashRun)
{
    // JSON value "a\\b" parses to a single backslash between a and b.
    std::string json = R"({"k": "a\\b"})";
    auto bytes = ToBytes(json);
    size_t pos = 0;
    auto obj = ParseJsonObject(pos, bytes);
    ASSERT_NE(obj, nullptr);
    ASSERT_EQ(obj->pairs.size(), 1u);
    auto& v = obj->pairs[0]->valueStr[0];
    ASSERT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], 'a');
    EXPECT_EQ(v[1], '\\');
    EXPECT_EQ(v[2], 'b');
}

// --- Coverage: CountBackslashesBefore while body (L26-27) ---
// The while loop body (++count; --pos;) only runs when the char before a candidate
// terminator is a backslash. A string value ending in an escaped backslash followed
// by the closing quote ("...\\") puts a backslash immediately before the closing '"',
// forcing CountBackslashesBefore to walk the run and enter the loop body.
TEST_F(ParseJsonTest, ParseJsonStringValueEndingWithBackslashRun)
{
    // JSON value "a\\" is a, then an escaped backslash; the closing '"' is preceded by '\'.
    std::string json = R"({"k": "a\\"})";
    auto bytes = ToBytes(json);
    size_t pos = 0;
    auto obj = ParseJsonObject(pos, bytes);
    ASSERT_NE(obj, nullptr);
    ASSERT_EQ(obj->pairs.size(), 1u);
    auto& v = obj->pairs[0]->valueStr[0];
    ASSERT_EQ(v.size(), 2u);
    EXPECT_EQ(v[0], 'a');
    EXPECT_EQ(v[1], '\\');
}

// --- Coverage: ParseJsonNumber early-return (L70-72) ---
TEST_F(ParseJsonTest, ParseJsonNumberEarlyReturnOutOfRange)
{
    auto bytes = ToBytes("abc");
    size_t pos = 50; // out of range
    EXPECT_EQ(ParseJsonNumber(pos, bytes), 0u);
}

TEST_F(ParseJsonTest, ParseJsonNumberEarlyReturnNotDigit)
{
    auto bytes = ToBytes("abc");
    size_t pos = 0; // not a digit
    EXPECT_EQ(ParseJsonNumber(pos, bytes), 0u);
}

// --- Coverage: ParseJsonArray early-return (L86-88) ---
TEST_F(ParseJsonTest, ParseJsonArrayEarlyReturnOutOfRange)
{
    auto bytes = ToBytes("abc");
    size_t pos = 50; // out of range
    auto pair = MakeOwned<JsonPair>();
    ParseJsonArray(pos, bytes, pair.get());
    EXPECT_EQ(pair->valueStr.size(), 0u);
}

TEST_F(ParseJsonTest, ParseJsonArrayEarlyReturnNotBracket)
{
    auto bytes = ToBytes("abc");
    size_t pos = 0; // not a '['
    auto pair = MakeOwned<JsonPair>();
    ParseJsonArray(pos, bytes, pair.get());
    EXPECT_EQ(pair->valueStr.size(), 0u);
}

TEST_F(ParseJsonTest, ParseJsonArrayEarlyReturnNullValue)
{
    auto bytes = ToBytes("[\"a\"]");
    size_t pos = 0;
    ParseJsonArray(pos, bytes, nullptr); // null value
    // Nothing crashes, nothing appended (no pair to append to).
    SUCCEED();
}

// --- Coverage: ParseJsonArray with nested object element (L98-100) ---
TEST_F(ParseJsonTest, ParseJsonArrayWithObjectElement)
{
    std::string json = R"({"items": [{"k": "v"}]})";
    auto bytes = ToBytes(json);
    size_t pos = 0;
    auto obj = ParseJsonObject(pos, bytes);
    ASSERT_NE(obj, nullptr);
    ASSERT_EQ(obj->pairs.size(), 1u);
    ASSERT_EQ(obj->pairs[0]->valueObj.size(), 1u);
    auto inner = obj->pairs[0]->valueObj[0].get();
    ASSERT_NE(inner, nullptr);
    ASSERT_EQ(inner->pairs.size(), 1u);
    EXPECT_EQ(inner->pairs[0]->key, "k");
    EXPECT_EQ(inner->pairs[0]->valueStr[0], "v");
}

// --- Coverage: ParseJsonObject early-return when not starting with '{' (L110-112) ---
TEST_F(ParseJsonTest, ParseJsonObjectEarlyReturnNotBrace)
{
    auto bytes = ToBytes("not an object");
    size_t pos = 0;
    auto obj = ParseJsonObject(pos, bytes);
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->pairs.size(), 0u);
}

TEST_F(ParseJsonTest, ParseJsonObjectEarlyReturnOutOfRange)
{
    auto bytes = ToBytes("");
    size_t pos = 0;
    auto obj = ParseJsonObject(pos, bytes);
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->pairs.size(), 0u);
}

// --- Coverage: ParseJsonObject first-pair creation on ':' (L126-128) ---
// A ':' that appears before any key is parsed forces creating the first pair via the
// empty-pairs branch (L126-128). Feeding malformed {" : "x"} triggers it; the exact pair
// count is implementation-defined, so we only assert the parse does not crash and returns.
TEST_F(ParseJsonTest, ParseJsonObjectColonBeforeAnyKey)
{
    std::string json = R"({ : "x"})";
    auto bytes = ToBytes(json);
    size_t pos = 0;
    auto obj = ParseJsonObject(pos, bytes);
    ASSERT_NE(obj, nullptr);
    SUCCEED();
}

// --- Coverage: ParseJsonObject nested object/array/number as value (L146,154,163,164) ---
// Deeply nested valueObj -> valueNum -> valueObj exercises the CJC_ASSERT-prefixed
// value branches for number, object and array in sequence.
TEST_F(ParseJsonTest, ParseJsonObjectMixedNestedValues)
{
    std::string json = R"({"a": 1, "b": {"c": 2}, "d": ["e", {"f": 3}]})";
    auto bytes = ToBytes(json);
    size_t pos = 0;
    auto obj = ParseJsonObject(pos, bytes);
    ASSERT_NE(obj, nullptr);
    ASSERT_GE(obj->pairs.size(), 3u);
    EXPECT_EQ(obj->pairs[0]->key, "a");
    ASSERT_EQ(obj->pairs[0]->valueNum.size(), 1u);
    EXPECT_EQ(obj->pairs[0]->valueNum[0], 1u);

    EXPECT_EQ(obj->pairs[1]->key, "b");
    ASSERT_EQ(obj->pairs[1]->valueObj.size(), 1u);
    auto bObj = obj->pairs[1]->valueObj[0].get();
    ASSERT_EQ(bObj->pairs.size(), 1u);
    EXPECT_EQ(bObj->pairs[0]->key, "c");
    EXPECT_EQ(bObj->pairs[0]->valueNum[0], 2u);

    EXPECT_EQ(obj->pairs[2]->key, "d");
    ASSERT_EQ(obj->pairs[2]->valueStr.size(), 1u);
    EXPECT_EQ(obj->pairs[2]->valueStr[0], "e");
    ASSERT_EQ(obj->pairs[2]->valueObj.size(), 1u);
    auto fObj = obj->pairs[2]->valueObj[0].get();
    ASSERT_EQ(fObj->pairs.size(), 1u);
    EXPECT_EQ(fObj->pairs[0]->key, "f");
    EXPECT_EQ(fObj->pairs[0]->valueNum[0], 3u);
}

// --- Coverage: GetJsonString null root + recursive nested-object hit (L174-176,181-186) ---
TEST_F(ParseJsonTest, GetJsonStringNullRoot)
{
    auto ret = GetJsonString(nullptr, "key");
    EXPECT_EQ(ret.size(), 0u);
}

TEST_F(ParseJsonTest, GetJsonStringFindsKeyInNestedObject)
{
    // The searched key lives only inside a nested object, so the recursion into
    // v->valueObj must run and return a non-empty result (L181-186).
    std::string json = R"({"outer": {"inner": "found"}, "other": "x"})";
    auto bytes = ToBytes(json);
    size_t pos = 0;
    auto obj = ParseJsonObject(pos, bytes);
    ASSERT_NE(obj, nullptr);

    auto values = GetJsonString(obj.get(), "inner");
    ASSERT_EQ(values.size(), 1u);
    EXPECT_EQ(values[0], "found");
}

// --- Coverage: GetJsonObject null root + recursive nested-object hit + not-found (L193-195,199-200,207) ---
TEST_F(ParseJsonTest, GetJsonObjectNullRoot)
{
    auto ret = GetJsonObject(nullptr, "key", 0);
    EXPECT_EQ(ret, nullptr);
}

TEST_F(ParseJsonTest, GetJsonObjectFindsInNestedObject)
{
    // The target object lives inside a nested valueObj, so the recursion branch (L200-204)
    // must find it.
    std::string json = R"({"a": "x", "b": {"target": {"k": "v"}}})";
    auto bytes = ToBytes(json);
    size_t pos = 0;
    auto obj = ParseJsonObject(pos, bytes);
    ASSERT_NE(obj, nullptr);

    auto target = GetJsonObject(obj.get(), "target", 0);
    ASSERT_NE(target, nullptr);
    ASSERT_EQ(target->pairs.size(), 1u);
    EXPECT_EQ(target->pairs[0]->key, "k");
    EXPECT_EQ(target->pairs[0]->valueStr[0], "v");
}

TEST_F(ParseJsonTest, GetJsonObjectNotFoundReturnsNull)
{
    std::string json = R"({"a": {"k": "v"}})";
    auto bytes = ToBytes(json);
    size_t pos = 0;
    auto obj = ParseJsonObject(pos, bytes);
    ASSERT_NE(obj, nullptr);

    auto ret = GetJsonObject(obj.get(), "doesnotexist", 0);
    EXPECT_EQ(ret, nullptr);
}

TEST_F(ParseJsonTest, GetJsonObjectIndexTooLargeReturnsNull)
{
    // key matches but valueObj.size() <= index -> not returned; recursion finds nothing.
    std::string json = R"({"a": {"k": "v"}})";
    auto bytes = ToBytes(json);
    size_t pos = 0;
    auto obj = ParseJsonObject(pos, bytes);
    ASSERT_NE(obj, nullptr);

    auto ret = GetJsonObject(obj.get(), "a", 5);
    EXPECT_EQ(ret, nullptr);
}
