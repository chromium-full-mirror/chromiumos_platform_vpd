// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CACHE_FILE_H_
#define CACHE_FILE_H_

#include <vector>

#include "base/files/file_path.h"
#include "brillo/brillo_export.h"

#include "vpd/types.h"

namespace vpd {

class BRILLO_EXPORT CacheFile {
 public:
  explicit CacheFile(VpdRegion region);
  CacheFile(VpdRegion region, const base::FilePath& cacheDir);
  bool Exists() const;
  bool Read(std::vector<const KeyVal>* kv) const;
  bool Write(const std::vector<const KeyVal>& kv) const;

 private:
  base::FilePath path_;
};

}  // namespace vpd

#endif  // CACHE_FILE_H_
