// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vpd/cache.h"

#include <optional>
#include <string>
#include <vector>

#include <stdio.h>

#include <base/check.h>
#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>

#include "vpd/cache_file.h"
#include "vpd/encoder.h"
#include "vpd/flashrom.h"
#include "vpd/vpd.h"

namespace vpd {

namespace {

std::vector<const KeyVal> MapToVector(
    const std::map<std::string, std::string>& map) {
  std::vector<const KeyVal> res;
  res.reserve(map.size());
  for (const auto& kv : map) {
    res.push_back(KeyVal{
        .key = kv.first,
        .value = kv.second,
    });
  }
  return res;
}

}  // namespace

Cache::Cache(const base::FilePath& fake_flash_path,
             unsigned int size,
             const base::FilePath& cache_dir)
    : flashrom_("dummy",
                base::StringPrintf("emulate=VARIABLE_SIZE,size=%u,image=%s",
                                   size,
                                   fake_flash_path.value().c_str())),
      cache_file_({
          {VpdRo, CacheFile(VpdRo, cache_dir)},
          {VpdRw, CacheFile(VpdRw, cache_dir)},
      }) {}

Cache::~Cache() {}

void Cache::Populate() {
  if (populated_) {
    return;
  }

  std::vector<const KeyVal> ro, rw;

  if (cache_file_.at(VpdRo).Exists()) {
    CHECK(cache_file_.at(VpdRo).Read(&ro));
  } else {
    ro = sysfs_.GetValues(VpdRo);
  }
  if (cache_file_.at(VpdRw).Exists()) {
    CHECK(cache_file_.at(VpdRw).Read(&rw));
  } else {
    rw = sysfs_.GetValues(VpdRw);
  }

  for (const auto& kv : ro) {
    cache_[VpdRo][kv.key] = kv.value;
  }
  for (const auto& kv : rw) {
    cache_[VpdRw][kv.key] = kv.value;
  }

  populated_ = true;
}

bool Cache::WriteBack(VpdRegion region) {
  if (!cache_file_.at(region).Write(MapToVector(cache_[region]))) {
    LOG(ERROR) << "Failed to write cache";
    return false;
  }

  const auto& blob = Encoder::Encode(cache_[region]);
  if (!blob) {
    LOG(ERROR) << "failed to encode VPD blob";
    return false;
  }

  if (!flashrom_.Write(region, *blob)) {
    LOG(ERROR) << "flashrom failure";
    return false;
  }

  return true;
}

std::optional<std::string> Cache::GetValue(VpdRegion region,
                                           const std::string& key) const {
  if (populated_) {
    const auto& map = cache_.at(region);
    const auto& it = map.find(key);
    if (it != map.end()) {
      return it->second;
    }
    return {};
  }

  return sysfs_.GetValue(region, key);
}

std::map<std::string, std::string> Cache::GetValues(VpdRegion region) {
  // We're going to need most of the keys, so might as well
  // populate everything.
  Populate();

  CHECK(populated_);

  return cache_.at(region);
}

bool Cache::WriteValues(
    VpdRegion region,
    const std::map<std::string, std::optional<std::string>>& pairs) {
  Populate();

  if (pairs.empty()) {
    LOG(ERROR) << "nothing to write";
    return false;
  }

  for (const auto& pair : pairs) {
    if (pair.second) {
      cache_[region][pair.first] = *pair.second;
    } else {
      cache_[region].erase(pair.first);
    }
  }

  return WriteBack(region);
}

}  // namespace vpd
