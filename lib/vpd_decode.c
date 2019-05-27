/*
 * Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 */
#include <assert.h>
#include "lib_vpd.h"

vpd_err_t decodeLen(
    const uint32_t max_len,
    const uint8_t *in,
    uint32_t *length,
    uint32_t *decoded_len) {
  uint8_t more;
  int i = 0;

  assert(length);
  assert(decoded_len);

  *length = 0;
  do {
    if (i >= max_len) return VPD_ERR_OVERFLOW;
    more = in[i] & 0x80;
    *length <<= 7;
    *length |= in[i] & 0x7f;
    ++i;
  } while (more);

  *decoded_len = i;

  return VPD_OK;
}

static vpd_err_t decodeVpdEntry(
    const uint32_t max_len,
    const uint8_t *input_buf,
    uint32_t *consumed,
    const uint8_t **entry,
    uint32_t *entry_len) {

  vpd_err_t res;
  uint32_t decoded_len;

  res = decodeLen(max_len - *consumed, &input_buf[*consumed],
                  entry_len, &decoded_len);
  if (res != VPD_OK)
    return res;
  if (max_len - *consumed < decoded_len)
    return VPD_ERR_OVERFLOW;

  *consumed += decoded_len;
  *entry = input_buf + *consumed;

  /* entry_len is untrusted data and must be checked again. */
  if (max_len - *consumed < *entry_len)
    return VPD_ERR_OVERFLOW;

  *consumed += *entry_len;
  return VPD_OK;
}

/* Sequentially decodes type, key, and value.
 */
vpd_err_t decodeVpdString(
    const uint32_t max_len,
    const uint8_t *input_buf,
    uint32_t *consumed,
    VpdDecodeCallback callback,
    void *callback_arg) {
  int type;
  uint32_t key_len, value_len;
  const uint8_t *key, *value;
  vpd_err_t retval;

  /* type */
  if (*consumed >= max_len)
    return VPD_ERR_OVERFLOW;

  type = input_buf[*consumed];
  switch (type) {
    case VPD_TYPE_INFO:
    case VPD_TYPE_STRING:
      (*consumed)++;

      retval = decodeVpdEntry(max_len, input_buf, consumed, &key, &key_len);
      if (retval != VPD_OK)
        return retval;

      retval = decodeVpdEntry(max_len, input_buf, consumed, &value, &value_len);
      if (retval != VPD_OK)
        return retval;

      if (type == VPD_TYPE_STRING)
        return callback(key, key_len, value, value_len, callback_arg);

      return VPD_OK;

    default:
      return VPD_ERR_INVALID;
      break;
  }
  return VPD_OK;
}
