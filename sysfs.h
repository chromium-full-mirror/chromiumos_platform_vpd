// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSFS_H_
#define SYSFS_H_

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "brillo/brillo_export.h"

#include "vpd/types.h"

namespace vpd {

class BRILLO_EXPORT Sysfs {
 public:
  Sysfs();
  explicit Sysfs(const base::FilePath& sysfs_dir);

  std::optional<std::string> GetValue(VpdRegion region,
                                      const std::string& key) const;

  std::vector<const KeyVal> GetValues(VpdRegion region) const;

 private:
  base::FilePath sysfs_dir_;
};

}  // namespace vpd

#endif  // SYSFS_H_
