// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vpd/encoder.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <uuid/uuid.h>

#include <base/check_op.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>

#include "lib/vpd.h"
#include "lib/vpd_tables.h"
#include "vpd/types.h"

namespace vpd {

namespace {

// The VPD format can support very large (up to 0x7FFFFFFF) entries, but this
// places an unreasonable strain on memory size, and is frankly impossible to
// fit on the flash devices VPD tends to be stored on. Let's just bail nicely
// if we're getting too large.
constexpr unsigned int kMaxBlobSize = 1024 * 1024 * 1024;

enum {
  VPD_TYPE_TERMINATOR = 0,
  VPD_TYPE_STRING,
  VPD_TYPE_INFO = 0xfe,
  VPD_TYPE_IMPLICIT_TERMINATOR = 0xff,
};

std::vector<uint8_t> EncodeLen(size_t len) {
  std::vector<uint8_t> data;
  unsigned int shifting = len;
  unsigned int encoded_len;
  unsigned int reversed_7bits = 0;

  /* reverse the len for every 7-bit. The little endian. */
  for (encoded_len = 0; shifting; encoded_len++) {
    reversed_7bits = (reversed_7bits << 7) | (shifting & 0x7f);
    shifting >>= 7;
  }
  if (!encoded_len)
    encoded_len = 1; /* output at least 1 byte */

  data.resize(encoded_len);

  /* Output in reverse order, now big endian. */
  for (auto& it : data) {
    /* always set MORE flag */
    it = 0x80 | (reversed_7bits & 0x7f);
    reversed_7bits >>= 7;
  }
  data.back() &= 0x7f; /* clear the MORE flag in last byte */

  return data;
}

std::vector<uint8_t> EncodePair(const KeyVal& kv) {
  std::vector<uint8_t> data;

  // Encode key.
  data.push_back(VPD_TYPE_STRING);
  auto k = EncodeLen(kv.key.size());
  data.insert(data.end(), k.begin(), k.end());
  for (const auto& b : kv.key) {
    data.push_back(b);
  }

  // Encode value.
  auto v = EncodeLen(kv.value.size());
  data.insert(data.end(), v.begin(), v.end());
  for (const auto& b : kv.value) {
    data.push_back(b);
  }

  return data;
}

bool DecodeLength(const std::vector<uint8_t> blob,
                  uint32_t* decoded_length,
                  uint32_t* entry_length) {
  *entry_length = 0;

  uint8_t more;
  uint32_t i = 0;
  do {
    if (i >= blob.size())
      return false;

    more = blob[i] & 0x80;
    *entry_length <<= 7;
    *entry_length |= blob[i] & 0x7f;
    i++;
  } while (more);

  *decoded_length = i;
  return true;
}

std::optional<KeyVal> DecodePair(const std::vector<uint8_t>& blob,
                                 uint32_t* consumed) {
  KeyVal ret;
  *consumed = 0;

  uint32_t decoded_length, entry_length;
  if (!DecodeLength(std::vector<uint8_t>(blob.begin(), blob.end()),
                    &decoded_length, &entry_length)) {
    return {};
  }
  if (decoded_length > blob.size() - *consumed) {
    return {};
  }
  *consumed += decoded_length;
  if (entry_length > blob.size() - *consumed) {
    return {};
  }

  ret.key = std::string(reinterpret_cast<const char*>(blob.data()) + *consumed,
                        entry_length);
  *consumed += entry_length;

  if (!DecodeLength(std::vector<uint8_t>(blob.begin() + *consumed, blob.end()),
                    &decoded_length, &entry_length)) {
    return {};
  }
  if (decoded_length > blob.size() - *consumed) {
    return {};
  }
  *consumed += decoded_length;
  if (entry_length > blob.size() - *consumed) {
    return {};
  }

  ret.value = std::string(
      reinterpret_cast<const char*>(blob.data()) + *consumed, entry_length);
  *consumed += entry_length;

  return ret;
}

std::vector<uint8_t> GenerateVpdBlob(
    const std::map<std::string, std::string>& dict) {
  struct google_vpd_info* info;
  std::vector<uint8_t> data(sizeof(*info));

  for (const auto& pair : dict) {
    const auto& b = EncodePair(KeyVal{
        .key = pair.first,
        .value = pair.second,
    });
    data.insert(data.end(), b.begin(), b.end());
  }

  data.push_back(VPD_TYPE_TERMINATOR);

  info = reinterpret_cast<struct google_vpd_info*>(data.data());
  memcpy(info->header.magic, VPD_INFO_MAGIC, sizeof(info->header.magic));
  info->size = data.size() - sizeof(*info);

  return data;
}

// Compare with the SMBIOS signature ("_SM_").
//
// Note that we don't write or require SMBIOS headers any more, but we still
// allow detecting some old blobs via this header.
//
// NB: This is a pretty basic validity check. We've never tried to do more, and
// it seems to be good enough.
bool IsSmbios(const std::vector<uint8_t>& blob) {
  if (blob.size() < sizeof(VPD_ENTRY_MAGIC) - 1)
    return false;
  return !memcmp(VPD_ENTRY_MAGIC, blob.data(), sizeof(VPD_ENTRY_MAGIC) - 1);
}

// Compare with the VPD magic signature.
bool IsVpdInfo(const struct google_vpd_info& info) {
  return memcmp(info.header.magic, VPD_INFO_MAGIC,
                sizeof(VPD_INFO_MAGIC) - 1) == 0;
}

std::optional<std::map<std::string, std::string>> DecodeBlob(
    const std::vector<uint8_t>& blob) {
  std::map<std::string, std::string> ret;

  std::vector<uint8_t> tracking(blob.begin(), blob.end());
  while (true) {
    // Legacy utility didn't require a terminator.
    if (tracking.empty()) {
      return ret;
    }

    uint8_t type = tracking[0];
    switch (type) {
      case VPD_TYPE_TERMINATOR:
      case VPD_TYPE_IMPLICIT_TERMINATOR:
        return ret;
      case VPD_TYPE_INFO:
      case VPD_TYPE_STRING: {
        uint32_t consumed;
        auto kv = DecodePair(
            std::vector<uint8_t>(tracking.begin() + 1, tracking.end()),
            &consumed);
        if (!kv) {
          LOG(ERROR) << "failed to decode key/value pair";
          return {};
        }
        ret[kv->key] = kv->value;
        VLOG(1) << "Parsed: key = " << kv->key << ", value = " << kv->value;
        tracking = std::vector<uint8_t>(tracking.begin() + 1 + consumed,
                                        tracking.end());
        break;
      }
      default:
        LOG(ERROR) << "unexpected VPD entry type: " << int(type);
        return {};
    }
  }
}

}  // namespace

// static
std::optional<std::vector<uint8_t>> Encoder::Encode(
    const std::map<std::string, std::string>& dict) {
  auto blob = GenerateVpdBlob(dict);

  // NB: old implementations pretended this was configurable, but always used a
  // fixed constant.
  const uint32_t vpd_2_0_offset = GOOGLE_VPD_2_0_OFFSET;

  std::vector<uint8_t> data(vpd_2_0_offset, 0xff);

  data.insert(data.end(), blob.begin(), blob.end());

  return data;
}

// static
std::optional<std::map<std::string, std::string>> Encoder::Decode(
    const std::vector<uint8_t>& blob) {
  const struct google_vpd_info* info;

  if (blob.size() < GOOGLE_VPD_2_0_OFFSET + sizeof(*info)) {
    LOG(ERROR) << "vpd size is too small: " << blob.size();
    return {};
  }
  info = reinterpret_cast<const struct google_vpd_info*>(blob.data() +
                                                         GOOGLE_VPD_2_0_OFFSET);

  if (blob.size() > kMaxBlobSize) {
    LOG(ERROR) << "vpd size is too large: " << blob.size();
    return {};
  }

  // Old tooling used to look for SMBIOS headers ("_SM_"). We no longer provide
  // SMBIOS headers, but we still allow them.
  if (!IsVpdInfo(*info) && !IsSmbios(blob)) {
    // This is an old heuristic used by VPD tooling, to treat an unformatted
    // flash as an empty key/value store.
    std::array<uint8_t, 4> empty_flash{0xff, 0xff, 0xff, 0xff};
    if (std::equal(empty_flash.begin(), empty_flash.end(), blob.begin())) {
      LOG(WARNING) << "VPD partition not formatted. It's fine.";
      return std::map<std::string, std::string>();
    }

    // All other contents are treated as an error.
    LOG(WARNING)
        << "could not find VPD headers; partition may not be formatted";
    return {};
  }

  return DecodeBlob(std::vector<uint8_t>(
      blob.begin() + GOOGLE_VPD_2_0_OFFSET + sizeof(*info), blob.end()));
}

// static
std::optional<std::map<std::string, std::string>> Encoder::DecodeRaw(
    const std::vector<uint8_t>& blob) {
  if (blob.size() > kMaxBlobSize) {
    LOG(ERROR) << "vpd size is too large: " << blob.size();
    return {};
  }

  return DecodeBlob(blob);
}

}  // namespace vpd
