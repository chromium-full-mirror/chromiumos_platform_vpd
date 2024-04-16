// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vpd/util/file.h"

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "gtest/gtest.h"

namespace vpd {

class FileUtilTest : public ::testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::ScopedTempDir temp_dir_;
};

TEST_F(FileUtilTest, ReadFileToBytesTest_RegularContent) {
  auto file_path =
      temp_dir_.GetPath().Append("ReadFileToBytesTest_RegularContent");
  std::vector<uint8_t> test_data = {'\0', '\1', '\2', '\3'};
  ASSERT_TRUE(base::WriteFile(file_path, test_data));

  auto bytes = vpd::util::ReadFileToBytes(file_path.value());
  EXPECT_TRUE(bytes.has_value());
  EXPECT_EQ(bytes, test_data);
}

TEST_F(FileUtilTest, ReadFileToBytesTest_EmptyContent) {
  auto file_path =
      temp_dir_.GetPath().Append("ReadFileToBytesTest_EmptyContent");
  ASSERT_TRUE(base::WriteFile(file_path, ""));

  auto bytes = vpd::util::ReadFileToBytes(file_path.value());
  EXPECT_TRUE(bytes.has_value());
  EXPECT_TRUE(bytes->empty());
}

TEST_F(FileUtilTest, ReadFileToBytesTest_FileNotExist) {
  auto file_path =
      temp_dir_.GetPath().Append("ReadFileToBytesTest_FileNotExist");
  ASSERT_FALSE(base::PathExists(file_path));
  EXPECT_FALSE(vpd::util::ReadFileToBytes(file_path.value()));
}

TEST_F(FileUtilTest, ReadFileToStringTest_RegularContent) {
  auto file_path =
      temp_dir_.GetPath().Append("ReadFileToStringTest_RegularContent");
  ASSERT_TRUE(base::WriteFile(file_path, "hello world"));

  auto buf = vpd::util::ReadFileToString(file_path.value());
  EXPECT_TRUE(buf.has_value());
  EXPECT_EQ(buf, "hello world");
}

TEST_F(FileUtilTest, ReadFileToStringTest_EmptyContent) {
  auto file_path =
      temp_dir_.GetPath().Append("ReadFileToStringTest_EmptyContent");
  ASSERT_TRUE(base::WriteFile(file_path, ""));

  auto buf = vpd::util::ReadFileToString(file_path.value());
  EXPECT_TRUE(buf.has_value());
  EXPECT_EQ(buf, "");
}

TEST_F(FileUtilTest, ReadFileToStringTest_FileNotExist) {
  auto file_path =
      temp_dir_.GetPath().Append("ReadFileToStringTest_FileNotExist");
  ASSERT_FALSE(base::PathExists(file_path));
  EXPECT_FALSE(vpd::util::ReadFileToString(file_path.value()));
}

}  // namespace vpd
