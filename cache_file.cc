// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vpd/cache_file.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"

#include "vpd/types.h"
#include "vpd/util/file.h"
#include "vpd/util/string.h"

namespace vpd {

namespace {

constexpr char kRunVpd[] = "/run/vpd";

std::string CacheFilePath(VpdRegion region, const std::string& dir) {
  if (region == VpdRo) {
    return vpd::util::JoinPath(dir, "ro.txt");
  }
  if (region == VpdRw) {
    return vpd::util::JoinPath(dir, "rw.txt");
  }
  __builtin_unreachable();
}

}  // namespace

CacheFile::CacheFile(VpdRegion region) : CacheFile(region, kRunVpd) {}

CacheFile::CacheFile(VpdRegion region, const std::string& cacheDir)
    : path_(CacheFilePath(region, cacheDir)) {}

bool CacheFile::Exists() const {
  return vpd::util::PathExists(path_);
}

bool CacheFile::Read(std::vector<const KeyVal>* kv) const {
  auto data = vpd::util::ReadFileToString(path_);
  if (!data) {
    LOG(ERROR) << "Failed to read cache file: " << path_;
    return false;
  }

  std::vector<std::pair<std::string, std::string>> pairs;
  if (!vpd::util::GetKeyValuePairsFromLines(data.value(), pairs)) {
    LOG(ERROR) << "Failed to parse cache file: " << path_;
    return false;
  }

  for (auto& pair : pairs) {
    const std::string& val = pair.second;

    if (val.size() < 2 || val[0] != '"' || val[val.size() - 1] != '"') {
      LOG(ERROR) << "Unquoted value entry: " << val;
      return false;
    }
    kv->push_back((KeyVal){
        .key = pair.first,
        .value = std::string(val.begin() + 1, val.end() - 1),
    });
  }

  return true;
}

bool CacheFile::Write(const std::vector<const KeyVal>& kvs) const {
  std::string data;

  for (const auto& kv : kvs) {
    data += kv.key + "=\"" + kv.value + "\"\n";
  }

  if (!vpd::util::WriteFile(path_, data)) {
    PLOG(ERROR) << "failed to write to " << path_;
    return false;
  }

  return true;
}

}  // namespace vpd
