// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Outputs a filtered list of VPD key/value pairs.
//
// Some clients will care not just whether these key/value pairs exist, but
// also whether the underlying region storage was valid. We dump some free-form
// error text about their status in stderr, and we dump precise error codes on
// exit as well.
//
// Exit status expectation:
//   0 = both RO and RW VPD are valid
//   1 = only RO is invalid
//   2 = only RW is invalid
//   3 = both RO and RW are invalid
//
// All other exit statuses are unexpected.

#include <algorithm>
#include <array>
#include <flat_set>
#include <iostream>
#include <string_view>
#include <utility>

#include "absl/log/initialize.h"
#include "absl/strings/ascii.h"
#include "vpd/vpd.h"

namespace {

constexpr std::array kRoVpdKeyAllowlist{
    "Product_S/N",
    "attested_device_id",
    "customization_id",
    "display_profiles",
    "initial_locale",
    "initial_timezone",
    "keyboard_layout",
    "model_name",
    "oem_device_requisition",
    "oem_name",
    "panel_backlight_max_nits",
    "region",
    "rlz_brand_code",
    "serial_number",
    "sku_number",
};

constexpr std::array kRwVpdKeyAllowlist{
    "ActivateDate",    "block_devmode",        "check_enrollment",
    "gbind_attribute", "rlz_embargo_end_date", "should_send_rlz_ping",
    "ubind_attribute",
};

enum ExitStatus {
  Valid = 0,
  RoInvalid = 1,
  RwInvalid = 2,
  BothInvalid = RoInvalid | RwInvalid,
};

bool IsValidVpdValue(std::string_view value) {
  return std::ranges::all_of(value, [](unsigned char c) {
    return absl::ascii_isprint(c) && c != '"';
  });
}

}  // namespace

int main(int argc, char* argv[]) {
  absl::InitializeLog();
  vpd::Vpd vpd;

  auto ro = vpd.GetValues(vpd::VpdRo);
  std::flat_set<std::string_view> ro_allowlist(std::begin(kRoVpdKeyAllowlist),
                                               std::end(kRoVpdKeyAllowlist));

  auto rw = vpd.GetValues(vpd::VpdRw);
  std::flat_set<std::string_view> rw_allowlist(std::begin(kRwVpdKeyAllowlist),
                                               std::end(kRwVpdKeyAllowlist));

  for (const auto& [dict, allowlist] :
       std::array{std::pair{ro, ro_allowlist}, std::pair{rw, rw_allowlist}}) {
    for (const auto& [key, value] : dict) {
      if (allowlist.contains(key) && IsValidVpdValue(value)) {
        std::cout << "\"" << key << "\"=\"" << value << "\"\n";
      }
    }
  }

  int ret = Valid;

  if (!vpd.Valid(vpd::VpdRo)) {
    std::cerr << "RO VPD is invalid\n";
    ret |= RoInvalid;
  }

  if (!vpd.Valid(vpd::VpdRw)) {
    std::cerr << "RW VPD is invalid\n";
    ret |= RwInvalid;
  }

  return ret;
}
