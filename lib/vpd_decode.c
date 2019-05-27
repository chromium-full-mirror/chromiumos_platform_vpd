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
  uint32_t decoded_len;
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

      /* key */
      retval = decodeLen(max_len - *consumed, &input_buf[*consumed],
                         &key_len, &decoded_len);
      if (VPD_OK != retval)
        return retval;

      if (*consumed + decoded_len >= max_len)
        return VPD_ERR_OVERFLOW;

      *consumed += decoded_len;
      key = &input_buf[*consumed];
      *consumed += key_len;

      /* value */
      retval = decodeLen(max_len - *consumed, &input_buf[*consumed],
                         &value_len, &decoded_len);
      if (VPD_OK != retval)
        return retval;

      if (*consumed + decoded_len > max_len)
        return VPD_ERR_OVERFLOW;

      *consumed += decoded_len;
      value = &input_buf[*consumed];
      *consumed += value_len;

      if (type == VPD_TYPE_STRING)
        return callback(key, key_len, value, value_len, callback_arg);

      return VPD_OK;

    default:
      return VPD_ERR_INVALID;
      break;
  }
  return VPD_OK;
}
