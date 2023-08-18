// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/logging.h>
#include <gtest/gtest.h>

#include "vpd/cache.h"
#include "vpd/fake_fmap.h"

namespace vpd {

class CacheTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base_dir_ = temp_dir_.GetPath();
    fake_flash_path_ = base_dir_.Append("flash.bin");
    cache_ = Cache(fake_flash_path_, 16 * 1024 * 1024, base_dir_);
  }

  void FillFlashFile(bool empty_fmap = false) {
    FakeFmap::FillFlashFile(fake_flash_path_, 16 * 1024 * 1024, empty_fmap);
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath base_dir_;
  base::FilePath fake_flash_path_;
  Cache cache_;
};

TEST_F(CacheTest, NoFmap) {
  FillFlashFile(true);

  auto ro = cache_.GetValues(VpdRo);
  auto rw = cache_.GetValues(VpdRw);
  EXPECT_TRUE(ro.empty());
  EXPECT_TRUE(rw.empty());
}

TEST_F(CacheTest, Empty) {
  FillFlashFile();

  auto ro = cache_.GetValues(VpdRo);
  auto rw = cache_.GetValues(VpdRw);
  EXPECT_TRUE(ro.empty());
  EXPECT_TRUE(rw.empty());
}

TEST_F(CacheTest, WriteRead) {
  FillFlashFile();

  EXPECT_TRUE(cache_.WriteValues(VpdRw, {{"foo", "bar"}}));
  EXPECT_TRUE(cache_.WriteValues(VpdRw, {{"abc", "def"}}));

  EXPECT_EQ(cache_.GetValue(VpdRw, "foo"), "bar");
  EXPECT_EQ(cache_.GetValue(VpdRw, "abc"), "def");
  EXPECT_EQ(cache_.GetValue(VpdRw, "non exist"), std::nullopt);

  EXPECT_EQ(cache_.GetValue(VpdRo, "foo"), std::nullopt);
  EXPECT_EQ(cache_.GetValue(VpdRo, "abc"), std::nullopt);
  EXPECT_EQ(cache_.GetValue(VpdRo, "non exist"), std::nullopt);

  EXPECT_EQ(cache_.GetValues(VpdRw).size(), 2);
  EXPECT_EQ(cache_.GetValues(VpdRo).size(), 0);
}

TEST_F(CacheTest, Delete) {
  FillFlashFile();

  EXPECT_TRUE(cache_.WriteValues(VpdRw, {{"foo", "bar"}}));
  EXPECT_EQ(cache_.GetValue(VpdRw, "foo"), "bar");
  // Delete "foo".
  EXPECT_TRUE(cache_.WriteValues(VpdRw, {{"foo", std::nullopt}}));
  // "foo" is missing now.
  EXPECT_EQ(cache_.GetValue(VpdRw, "foo"), std::nullopt);
  // Delete "foo" again. This is not an error.
  EXPECT_TRUE(cache_.WriteValues(VpdRw, {{"foo", std::nullopt}}));
}

}  // namespace vpd
