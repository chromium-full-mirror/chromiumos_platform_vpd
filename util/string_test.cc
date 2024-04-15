// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdarg.h>

#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

#include "vpd/util/string.h"

namespace {

// Helper function to easily call StringPrintV
std::string StringPrintVTestHelper(const char* format, ...) {
  va_list args;
  va_start(args, format);
  std::string result = vpd::util::StringPrintV(format, args);
  va_end(args);
  return result;
}

}  // namespace

namespace vpd {

typedef std::vector<std::pair<std::string, std::string>> KeyValuePairs;

TEST(GetKeyValuePairsFromLinesTest, RegularContent) {
  std::pair<std::string, KeyValuePairs> test_cases[] = {
      {"k=v", {{"k", "v"}}},
      {"k=v\n", {{"k", "v"}}},
      {"k1=v1\nk2=v2", {{"k1", "v1"}, {"k2", "v2"}}},
      {"k1=v1\nk2=v2\n", {{"k1", "v1"}, {"k2", "v2"}}},
  };
  for (const auto& tc : test_cases) {
    std::string content = tc.first;
    KeyValuePairs want = tc.second;
    KeyValuePairs v;
    EXPECT_TRUE(vpd::util::GetKeyValuePairsFromLines(content, v));
    EXPECT_EQ(v, want);
  }
}

TEST(GetKeyValuePairsFromLinesTest, BinaryContent) {
  std::pair<std::string, KeyValuePairs> test_cases[] = {
      {std::string({'\0', '=', '\1'}), {{std::string({'\0'}), "\1"}}},
      {std::string({'\0', '=', '\1', '\n'}), {{std::string({'\0'}), "\1"}}},
      {std::string({'\0', '=', '\1', '\n', '\2', '=', '\3'}),
       {{std::string({'\0'}), "\1"}, {"\2", "\3"}}},
      {std::string({'\0', '=', '\1', '\n', '\2', '=', '\3', '\n'}),
       {{std::string({'\0'}), "\1"}, {"\2", "\3"}}},
  };
  for (const auto& tc : test_cases) {
    std::string content = tc.first;
    KeyValuePairs want = tc.second;
    KeyValuePairs v;
    EXPECT_TRUE(vpd::util::GetKeyValuePairsFromLines(content, v));
    EXPECT_EQ(v, want);
  }
}

TEST(GetKeyValuePairsFromLinesTest, EmptyContent) {
  KeyValuePairs v;
  EXPECT_TRUE(vpd::util::GetKeyValuePairsFromLines("", v));
  EXPECT_TRUE(v.empty());
}

TEST(GetKeyValuePairsFromLinesTest, EqualSignInValue) {
  KeyValuePairs v;
  EXPECT_TRUE(vpd::util::GetKeyValuePairsFromLines("a=b=c", v));
  KeyValuePairs want = {{"a", "b=c"}};
  EXPECT_EQ(v, want);
}

TEST(GetKeyValuePairsFromLinesTest, InvalidEmptyContent) {
  std::string test_cases[] = {
      " ", "  ", "\n", " \n", "\n ", "\n\n",
  };
  for (const auto& content : test_cases) {
    KeyValuePairs v;
    EXPECT_FALSE(vpd::util::GetKeyValuePairsFromLines(content, v));
  }
}

TEST(GetKeyValuePairsFromLinesTest, MissingItem) {
  std::string test_cases[] = {"k=", "=v", "=", "k1=v1\nk2=", "k1=\nk2=v2"};
  for (const auto& tc : test_cases) {
    KeyValuePairs v;
    EXPECT_FALSE(vpd::util::GetKeyValuePairsFromLines(tc, v));
  }
}

TEST(StringPrintVTest, RegularContent) {
  EXPECT_EQ(StringPrintVTestHelper("foo"), "foo");
  EXPECT_EQ(StringPrintVTestHelper("%s %s", "hello", "world"), "hello world");
  EXPECT_EQ(StringPrintVTestHelper("%d + %d = %d", 1, 2, 3), "1 + 2 = 3");
}

TEST(StringPrintVTest, LargeContent) {
  std::vector<char> buf1(32 * 1024 * 1024, ' ');
  buf1.back() = '\0';
  EXPECT_NE(StringPrintVTestHelper("%s", buf1.data()), "");

  std::vector<char> buf2(32 * 1024 * 1024 + 1, ' ');
  buf2.back() = '\0';
  EXPECT_EQ(StringPrintVTestHelper("%s", buf2.data()), "");
}

}  // namespace vpd
