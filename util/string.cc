// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vpd/util/string.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_split.h"

namespace vpd {
namespace util {

bool GetKeyValuePairsFromLines(
    const std::string& content,
    std::vector<std::pair<std::string, std::string>>& result) {
  std::vector<std::string> lines = absl::StrSplit(content, '\n');

  // Remove trailing linefeed.
  if (!lines.empty() && lines.back().empty()) {
    lines.pop_back();
  }

  for (auto line : lines) {
    std::pair<std::string, std::string> kv =
        absl::StrSplit(line, absl::MaxSplits('=', 1));
    if (kv.first.empty() || kv.second.empty()) {
      return false;
    }
    result.push_back(kv);
  }
  return true;
}

}  // namespace util
}  // namespace vpd
