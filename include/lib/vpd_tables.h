/*
 * Copyright 2014 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Ported from mosys project (http://code.google.com/p/mosys/).
 */

#ifndef INCLUDE_LIB_VPD_TABLES_H_
#define INCLUDE_LIB_VPD_TABLES_H_

#include <inttypes.h>

#define VPD_ENTRY_MAGIC "_SM_"
#define VPD_INFO_MAGIC                   \
  "\xfe"     /* type: VPD header */      \
  "\x09"     /* key length, 9 = 1 + 8 */ \
  "\x01"     /* info version, 1 */       \
  "gVpdInfo" /* signature, 8 bytes */    \
  "\x04"     /* value length */

/* Google specific VPD info */
struct google_vpd_info {
  union {
    struct {
      uint8_t type;
      uint8_t key_len;
      uint8_t info_ver;
      uint8_t signature[8];
      uint8_t value_len;
    } tlv;
    uint8_t magic[12];
  } header;
  uint32_t size;
} __attribute__((packed));

/* Entry */
struct vpd_entry {
  uint8_t anchor_string[4];
  uint8_t entry_cksum;
  uint8_t entry_length;
  uint8_t major_ver;
  uint8_t minor_ver;
  uint16_t max_size;
  uint8_t entry_rev;
  uint8_t format_area[5];
  uint8_t inter_anchor_string[5];
  uint8_t inter_anchor_cksum;
  uint16_t table_length;
  uint32_t table_address;
  uint16_t table_entry_count;
  uint8_t bcd_revision;
} __attribute__((packed));

/* Header */
struct vpd_header {
  uint8_t type;
  uint8_t length;
  uint16_t handle;
} __attribute__((packed));

#endif /* INCLUDE_LIB_VPD_TABLES_H_ */
