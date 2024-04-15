// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_FILE_H_
#define UTIL_FILE_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "vpd/export.h"

namespace vpd {
namespace util {

EXPORT std::optional<std::vector<uint8_t>> ReadFileToBytes(
    const std::string& filename);

}  // namespace util
}  // namespace vpd

#endif  // UTIL_FILE_H_
