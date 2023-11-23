// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ENCODER_H_
#define ENCODER_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include <brillo/brillo_export.h>

#include "lib/vpd.h"

namespace vpd {

class BRILLO_EXPORT Encoder {
 public:
  static std::optional<std::vector<uint8_t>> Encode(
      const std::map<std::string, std::string>& dict);

  static std::optional<std::map<std::string, std::string>> Decode(
      const std::vector<uint8_t>& blob);
  static std::optional<std::map<std::string, std::string>> DecodeRaw(
      const std::vector<uint8_t>& blob);
};

}  // namespace vpd

#endif  // ENCODER_H_
