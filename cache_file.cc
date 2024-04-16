// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vpd/cache_file.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"

#include "vpd/types.h"
#include "vpd/util/file.h"
#include "vpd/util/string.h"

namespace vpd {

namespace {

constexpr char kRunVpd[] = "/run/vpd";

base::FilePath CacheFilePath(VpdRegion region, const base::FilePath& dir) {
  if (region == VpdRo) {
    return dir.Append("ro.txt");
  }
  if (region == VpdRw) {
    return dir.Append("rw.txt");
  }
  __builtin_unreachable();
}

}  // namespace

CacheFile::CacheFile(VpdRegion region)
    : CacheFile(region, base::FilePath(kRunVpd)) {}

CacheFile::CacheFile(VpdRegion region, const base::FilePath& cacheDir)
    : path_(CacheFilePath(region, cacheDir)) {}

bool CacheFile::Exists() const {
  return base::PathExists(path_);
}

bool CacheFile::Read(std::vector<const KeyVal>* kv) const {
  auto data = vpd::util::ReadFileToString(path_.value());
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

  if (!base::WriteFile(path_, data)) {
    PLOG(ERROR) << "failed to write to " << path_;
    return false;
  }

  return true;
}

}  // namespace vpd
