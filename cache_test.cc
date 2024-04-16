// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "gtest/gtest.h"

#include "vpd/cache.h"
#include "vpd/fake_fmap.h"
#include "vpd/types.h"

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

// A second cache object, reading/writing in sequence, as if multiple clients
// are operating on VPD.
TEST_F(CacheTest, MultiClient) {
  FillFlashFile();

  EXPECT_TRUE(cache_.WriteValues(VpdRw, {
                                            {"foo", "bar"},
                                            {"serial", "number"},
                                        }));
  EXPECT_EQ(cache_.GetValue(VpdRw, "foo"), "bar");
  EXPECT_EQ(cache_.GetValue(VpdRw, "serial"), "number");

  Cache cache2(fake_flash_path_, 16 * 1024 * 1024, base_dir_);
  EXPECT_EQ(cache2.GetValue(VpdRw, "foo"), "bar");
  EXPECT_EQ(cache2.GetValue(VpdRw, "serial"), "number");

  EXPECT_TRUE(cache2.WriteValues(VpdRw, {
                                            {"serial", "updated serial number"},
                                            {"another", "entry"},
                                        }));

  EXPECT_EQ(cache_.GetValue(VpdRw, "foo"), "bar");
  EXPECT_EQ(cache_.GetValue(VpdRw, "serial"), "updated serial number");
  EXPECT_EQ(cache_.GetValue(VpdRw, "another"), "entry");
  EXPECT_EQ(cache2.GetValue(VpdRw, "foo"), "bar");
  EXPECT_EQ(cache2.GetValue(VpdRw, "serial"), "updated serial number");
  EXPECT_EQ(cache2.GetValue(VpdRw, "another"), "entry");
}

TEST_F(CacheTest, FlashFailure) {
  // Fake a flash failure by way of an empty (i.e., no VPD regions) FMAP.
  FillFlashFile(true /* empty_fmap */);

  EXPECT_FALSE(cache_.WriteValues(VpdRw, {{"foo", "bar"}}));
  // Ensure we didn't update the cache when the flash failed.
  EXPECT_TRUE(cache_.GetValues(VpdRw).empty());

  // Second instance (e.g., rereading from cache?) is also empty.
  Cache cache2(fake_flash_path_, 16 * 1024 * 1024, base_dir_);
  EXPECT_TRUE(cache_.GetValues(VpdRw).empty());
}

}  // namespace vpd
