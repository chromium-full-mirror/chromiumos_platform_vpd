// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vpd/encoder.h"

#include <cstdint>
#include <vector>

#include <base/logging.h>

namespace vpd {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Turn off logging.
  logging::SetMinLogLevel(logging::LOGGING_FATAL);

  auto dict = Encoder::Decode(std::vector<uint8_t>(data, data + size));

  // If we decoded something, why not try re-encoding it?
  if (dict) {
    // Don't assume this will succeed, as we might be a bit stricter on
    // encoding inputs. Just fuzz that we don't do Real Bad Things.
    Encoder::Encode(*dict);
  }

  return 0;
}

}  // namespace vpd
