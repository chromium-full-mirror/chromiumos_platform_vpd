/*
 * Copyright (C) 2010 Google Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 *
 */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "lib/lib_vpd.h"


/* Encodes the len into multiple bytes with the following format.
 *
 *    7   6 ............ 0
 *  +----+------------------+
 *  |More|      Length      |  ...
 *  +----+------------------+
 *
 * Returns fail if the buffer is not long enough.
 */
int encodeLen(
    const int len,
    unsigned char *output_buf,
    const int max_buffer_len,
    int *generated_len) {
  unsigned int shifting;
  unsigned int reversed_7bits = 0;
  int out_index = 0;

  assert(generated_len);

  if (len < 0) return VPD_FAIL;
  shifting = len;
  /* reverse the len for every 7-bit. The little endian. */
  for (*generated_len = 0; shifting; (*generated_len)++) {
    reversed_7bits = (reversed_7bits << 7) | (shifting & 0x7f);
    shifting >>= 7;
  }
  if (*generated_len > max_buffer_len) return VPD_FAIL;
  if (!*generated_len) *generated_len = 1;  /* output at least 1 byte */

  /* Output in reverse order, now big endian. */
  while (out_index < *generated_len) {
    /* always set MORE flag */
    output_buf[out_index++] = 0x80 | (reversed_7bits & 0x7f);
    reversed_7bits >>= 7;
  }
  output_buf[out_index - 1] &= 0x7f;  /* clear the MORE flag in last byte */

  return VPD_OK;
}

/* FIXME: not tested yet */
int decodeLen(
    const unsigned char *in,
    int *decoded_len,
    int *consumed) {
  int more;

  assert(decoded_len);

  *decoded_len = 0;
  do {
    more = *in & 0x80;
    *decoded_len <<= 7;
    *decoded_len |= *in & 0x7f;
    in++;
  } while (more);

  return VPD_OK;
}


/*  Encodes the terminator.
 */
int encodeVpdTerminator(
    const int max_buffer_len,
    unsigned char *output_buf,
    int *generated_len) {
  assert(generated_len);

  if (*generated_len + 1 > max_buffer_len) return VPD_FAIL;

  output_buf += *generated_len;  /* move cursor to end of string */
  *(output_buf++) = VPD_TYPE_TERMINATOR;
  (*generated_len)++;

  return VPD_OK;
}

/* Encodes a string with padding support. */
int encodeVpdString(
    const unsigned char *key,
    const unsigned char *value,
    const int pad_value_len,
    const int max_buffer_len,
    unsigned char *output_buf,
    int *generated_len) {
  int key_len, value_len;
  int out_index = 0;
  int ret_len;
  int pad_len = 0;

  assert(generated_len);

  key_len = strlen(key);
  value_len = strlen(value);
  output_buf += *generated_len;  /* move cursor to end of string */

  /* encode type */
  if (*generated_len + 1 > max_buffer_len) return VPD_FAIL;
  *(output_buf++) = VPD_TYPE_STRING;
  (*generated_len)++;

  /* encode key len */
  if (VPD_OK != encodeLen(key_len, output_buf,
                          max_buffer_len - *generated_len, &ret_len))
    return VPD_FAIL;
  output_buf += ret_len;
  *generated_len += ret_len;
  /* encode key string */
  if (*generated_len + key_len > max_buffer_len) return VPD_FAIL;
  memcpy(output_buf, key, key_len);
  output_buf += key_len;
  *generated_len += key_len;

  /* count padding length */
  if (pad_value_len != VPD_AS_LONG_AS) {
    if (value_len < pad_value_len) {
      pad_len = pad_value_len - value_len;
    } else {
      value_len = pad_value_len;
    }
  }

  /* encode value len */
  if (VPD_OK != encodeLen(value_len + pad_len, output_buf,
                          max_buffer_len - *generated_len, &ret_len))
    return VPD_FAIL;
  output_buf += ret_len;
  *generated_len += ret_len;
  /* encode value string */
  if (*generated_len + value_len > max_buffer_len) return VPD_FAIL;
  memcpy(output_buf, value, value_len);
  output_buf += value_len;
  *generated_len += value_len;
  /* encode padding (if applicable) */
  if (*generated_len + pad_len > max_buffer_len) return VPD_FAIL;
  memset(output_buf, 0, pad_len);
  output_buf += pad_len;
  *generated_len += pad_len;

  return VPD_OK;
}


/***********************************************************************
 * Container helpers
 ***********************************************************************/
void initContainer(struct PairContainer *container) {
  container->first = NULL;
}

struct StringPair *findString(const struct PairContainer *container,
                              const unsigned char *key) {
  struct StringPair *current;

  for (current = container->first; current; current = current->next) {
    if (!strcmp(key, current->key)) {
      return current;
    }
  }
  return NULL;
}

/* Just a helper function for setString() */
static void fillStringPair(struct StringPair *pair,
                           const unsigned char *key,
                           const unsigned char *value,
                           const int pad_len) {
  pair->key = malloc(strlen(key) + 1);
  assert(pair->key);
  strcpy(pair->key, key);
  pair->value = malloc(strlen(value) + 1);
  strcpy(pair->value, value);
  pair->pad_len = pad_len;
}

/* If key is already existed in container, its value will be replaced.
 * If not existed, creates new entry in container.
 */
void setString(struct PairContainer *container,
               const unsigned char *key,
               const unsigned char *value,
               const int pad_len) {
  struct StringPair *found;

  found = findString(container, key);
  if (found) {
    free(found->key);
    free(found->value);
    fillStringPair(found, key, value, pad_len);
  } else {
    struct StringPair *new_pair = malloc(sizeof(struct StringPair));
    assert(new_pair);

    fillStringPair(new_pair, key, value, pad_len);
    new_pair->next = container->first;
    container->first = new_pair;
  }
}

/* Iterate src container and setString() in dst.
 * so that if key is duplicate, the one in dst is overwritten.
 */
void mergeContainer(struct PairContainer *dst,
                    const struct PairContainer *src) {
  struct StringPair *current;

  for (current = src->first; current; current = current->next) {
    setString(dst, current->key, current->value, current->pad_len);
  }
}

int encodeContainer(const struct PairContainer *container,
                    const int max_buf_len,
                    unsigned char *buf,
                    int *generated) {
  struct StringPair *current;

  for (current = container->first; current; current = current->next) {
    if (VPD_OK != encodeVpdString(current->key,
                                  current->value,
                                  current->pad_len,
                                  max_buf_len,
                                  buf,
                                  generated)) {
      return VPD_FAIL;
    }
  }
  return VPD_OK;
}

void destroyContainer(struct PairContainer *container) {
  struct StringPair *current;

  for (current = container->first; current;) {
    struct StringPair *next;

    if (current->key) free(current->key);
    if (current->value) free(current->value);
    next = current->next;
    free(current);
    current = next;
  }
}
