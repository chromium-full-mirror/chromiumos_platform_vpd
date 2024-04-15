// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vpd/util/file.h"

#include <stddef.h>
#include <stdint.h>

#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace vpd {
namespace util {

std::optional<std::vector<uint8_t>> ReadFileToBytes(
    const std::string& filename) {
  std::ifstream file_stream(filename, std::ios::in | std::ios::binary);

  if (!file_stream.is_open()) {
    return {};
  }

  file_stream.seekg(0, std::ios::end);
  size_t file_size = file_stream.tellg();
  file_stream.seekg(0, std::ios::beg);

  std::vector<uint8_t> blob(file_size);
  file_stream.read(reinterpret_cast<char*>(blob.data()), file_size);

  file_stream.close();

  return blob;
}

}  // namespace util
}  // namespace vpd
