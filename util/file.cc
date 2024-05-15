// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vpd/util/file.h"

#include <dirent.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "absl/log/log.h"

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

std::optional<std::string> ReadFileToString(const std::string& filename) {
  std::ifstream file(filename, std::ios::in);

  if (!file.is_open()) {
    return {};
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  file.close();

  return buffer.str();
}

bool WriteFile(const std::string& filename, const std::string& data) {
  std::ofstream file(filename, std::ios::out);

  if (!file.is_open()) {
    return false;
  }

  file << data;
  file.close();

  return file.good();
}

std::vector<std::string> ListFiles(const std::string& dir) {
  DIR* d = opendir(dir.c_str());
  if (d == nullptr) {
    LOG(ERROR) << "Error opening directory: " << dir;
    return {};
  }

  std::vector<std::string> files;
  struct dirent* entry = nullptr;
  while ((entry = readdir(d)) != nullptr) {
    // Check if the entry is a regular file (and not a directory, symlink, etc.)
    if (entry->d_type == DT_REG) {
      files.push_back(entry->d_name);
    }
  }

  closedir(d);
  return files;
}

bool PathExists(const std::string& path) {
  return access(path.c_str(), F_OK) == 0;
}

}  // namespace util
}  // namespace vpd
