// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vpd/sysfs.h"

#include <optional>
#include <string>
#include <vector>

#include "absl/log/log.h"
#include "base/files/file_enumerator.h"

#include "vpd/types.h"
#include "vpd/util/file.h"

namespace vpd {

namespace {

constexpr char kSysfsVpd[] = "/sys/firmware/vpd";

base::FilePath VpdRegionToSysfsPath(const base::FilePath& sysfs_dir,
                                    VpdRegion region) {
  if (region == VpdRo) {
    return sysfs_dir.Append("ro");
  }
  if (region == VpdRw) {
    return sysfs_dir.Append("rw");
  }
  __builtin_unreachable();
}

std::optional<std::string> VpdSysfsRead(const base::FilePath& region_dir,
                                        const std::string& key) {
  base::FilePath path = region_dir.Append(key);

  auto contents = vpd::util::ReadFileToString(path.value());
  if (!contents) {
    return {};
  }

  return contents;
}

}  // namespace

Sysfs::Sysfs() : Sysfs(base::FilePath(kSysfsVpd)) {}

Sysfs::Sysfs(const base::FilePath& sysfs_dir) : sysfs_dir_(sysfs_dir) {}

std::optional<std::string> Sysfs::GetValue(VpdRegion region,
                                           const std::string& key) const {
  return VpdSysfsRead(VpdRegionToSysfsPath(sysfs_dir_, region), key);
}

std::vector<const KeyVal> Sysfs::GetValues(VpdRegion region) const {
  base::FileEnumerator e(VpdRegionToSysfsPath(sysfs_dir_, region),
                         false /* recursive */, base::FileEnumerator::FILES);
  std::vector<const KeyVal> res;

  for (base::FilePath key = e.Next(); !key.empty(); key = e.Next()) {
    auto contents = vpd::util::ReadFileToString(key.AsUTF8Unsafe());
    if (!contents) {
      PLOG(ERROR) << "Failed to read sysfs: " << key;
      continue;
    }
    res.push_back(KeyVal{
        .key = key.BaseName().AsUTF8Unsafe(),
        .value = contents.value(),
    });
  }

  return res;
}

}  // namespace vpd
