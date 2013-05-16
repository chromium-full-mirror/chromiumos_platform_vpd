/*
 * Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lib/lib_vpd.h"


#ifndef MIN
#define MIN(a, b) ((a < b) ? a : b)
#endif


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
    const int32_t len,
    uint8_t *encode_buf,
    const int32_t max_len,
    int32_t *encoded_len) {
  unsigned int shifting;
  unsigned int reversed_7bits = 0;
  int out_index = 0;

  assert(encoded_len);

  if (len < 0) return VPD_FAIL;
  shifting = len;
  /* reverse the len for every 7-bit. The little endian. */
  for (*encoded_len = 0; shifting; (*encoded_len)++) {
    reversed_7bits = (reversed_7bits << 7) | (shifting & 0x7f);
    shifting >>= 7;
  }
  if (*encoded_len > max_len) return VPD_FAIL;
  if (!*encoded_len) *encoded_len = 1;  /* output at least 1 byte */

  /* Output in reverse order, now big endian. */
  while (out_index < *encoded_len) {
    /* always set MORE flag */
    encode_buf[out_index++] = 0x80 | (reversed_7bits & 0x7f);
    reversed_7bits >>= 7;
  }
  encode_buf[out_index - 1] &= 0x7f;  /* clear the MORE flag in last byte */

  return VPD_OK;
}


int decodeLen(
    const int32_t max_len,
    const uint8_t *in,
    int32_t *length,
    int32_t *decoded_len) {
  uint8_t more;
  int i = 0;

  assert(length);
  assert(decoded_len);

  *length = 0;
  do {
    if (i >= max_len) return VPD_FAIL;
    more = in[i] & 0x80;
    *length <<= 7;
    *length |= in[i] & 0x7f;
    ++i;
  } while (more);

  *decoded_len = i;

  return VPD_OK;
}


/*  Encodes the terminator.
 */
int encodeVpdTerminator(
    const int max_buffer_len,
    uint8_t *output_buf,
    int *generated_len) {
  assert(generated_len);

  if (*generated_len >= max_buffer_len) return VPD_FAIL;

  output_buf += *generated_len;  /* move cursor to end of string */
  *(output_buf++) = VPD_TYPE_TERMINATOR;
  (*generated_len)++;

  return VPD_OK;
}

/* Encodes a string with padding support. */
int encodeVpdString(
    const uint8_t *key,
    const uint8_t *value,
    const int pad_value_len,
    const int max_buffer_len,
    uint8_t *output_buf,
    int *generated_len) {
  int key_len, value_len;
  int ret_len;
  int pad_len = 0;

  assert(generated_len);

  key_len = strlen((char*)key);
  value_len = strlen((char*)value);
  output_buf += *generated_len;  /* move cursor to end of string */

  /* encode type */
  if (*generated_len >= max_buffer_len) return VPD_FAIL;
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


/* Sequentially decodes type, key, and value.
 */
int decodeVpdString(
    const int32_t max_len,
    const uint8_t *input_buf,
    struct PairContainer *container,
    int32_t *consumed) {
  int type;
  int32_t length;
  int32_t decoded_len;
  uint8_t *key, *value;

  /* type */
  if (*consumed >= max_len) return VPD_FAIL;
  type = input_buf[*consumed];
  switch (type) {
    case VPD_TYPE_STRING:
      (*consumed)++;

      /* key */
      if (VPD_OK != decodeLen(max_len - *consumed, &input_buf[*consumed],
                              &length, &decoded_len)) {
        return VPD_FAIL;
      }
      *consumed += decoded_len;

      key = malloc(length + 1);
      assert(key);
      strncpy((char*)key, (char*)&input_buf[*consumed], length);
      key[length] = '\0';
      *consumed += length;

      /* value */
      if (VPD_OK != decodeLen(max_len - *consumed, &input_buf[*consumed],
                              &length, &decoded_len)) {
        return VPD_FAIL;
      }
      *consumed += decoded_len;

      value = malloc(length + 1);
      assert(value);
      strncpy((char*)value, (char*)&input_buf[*consumed], length);
      value[length] = '\0';
      *consumed += length;

      /* Add it into container */
      setString(container, key, value, length);
      break;
    default:
      return VPD_FAIL;
      break;
  }
  return VPD_OK;
}


/***********************************************************************
 * Container helpers
 ***********************************************************************/
void initContainer(struct PairContainer *container) {
  container->first = NULL;
}


/*
 * Returns the pointer to the 'key' entry.
 * Returns NULL if the key is not found in 'container'.
 *
 * If 'prev_next' is not NULL, findString() stores the address of the "next"
 * member of the StringPair prior to the returned StringPair (the special case
 * is &container->first if first StringPair matches 'key'). This is for linked
 * list manipulation. If 'prev_next' is NULL, findString() would just ignore
 * it.
 */
struct StringPair *findString(struct PairContainer *container,
                              const uint8_t *key,
                              struct StringPair ***prev_next) {
  struct StringPair *current;

  if (prev_next) {
    *prev_next = &container->first;
  }

  for (current = container->first; current; current = current->next) {
    if (!strcmp((char*)key, (char*)current->key)) {
      return current;
    }
    if (prev_next) {
      *prev_next = &current->next;
    }
  }
  return NULL;
}

/* Just a helper function for setString() */
static void fillStringPair(struct StringPair *pair,
                           const uint8_t *key,
                           const uint8_t *value,
                           const int pad_len) {
  pair->key = malloc(strlen((char*)key) + 1);
  assert(pair->key);
  strcpy((char*)pair->key, (char*)key);
  pair->value = malloc(strlen((char*)value) + 1);
  strcpy((char*)pair->value, (char*)value);
  pair->pad_len = pad_len;
}

/* If key is already existed in container, its value will be replaced.
 * If not existed, creates new entry in container.
 */
void setString(struct PairContainer *container,
               const uint8_t *key,
               const uint8_t *value,
               const int pad_len) {
  struct StringPair *found;

  found = findString(container, key, NULL);
  if (found) {
    free(found->key);
    free(found->value);
    fillStringPair(found, key, value, pad_len);
  } else {
    struct StringPair *new_pair = malloc(sizeof(struct StringPair));
    assert(new_pair);
    memset(new_pair, 0, sizeof(struct StringPair));

    fillStringPair(new_pair, key, value, pad_len);

    /* append this pair to the end of list. to keep the order */
    if ((found = container->first)) {
      while (found->next) found = found->next;
      found->next = new_pair;
    } else {
      container->first = new_pair;
    }
    new_pair->next = NULL;
  }
}


/*
 * Remove a key.
 * Returns VPD_OK if deleted successfully. Otherwise, VPD_FAIL.
 */
int deleteKey(struct PairContainer *container,
              const uint8_t *key) {
  struct StringPair *found, **prev_next;

  found = findString(container, key, &prev_next);
  if (found) {
    free(found->key);
    free(found->value);

    /* remove the 'found' from the linked list. */
    assert(prev_next);
    *prev_next = found->next;
    free(found);

    return VPD_OK;
  } else {
    return VPD_FAIL;
  }
}


/*
 * Returns number of pairs in container.
 */
int lenOfContainer(const struct PairContainer *container) {
  int count;
  struct StringPair *current;

  for (count = 0, current = container->first;
       current;
       count++, current = current->next);

  return count;
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


int subtractContainer(struct PairContainer *dst,
                       const struct PairContainer *src) {
  struct StringPair *current;
  int count = 0;

  for (current = src->first; current; current = current->next) {
    if (VPD_OK == deleteKey(dst, current->key))
      count++;
  }

  return count;
}


int encodeContainer(const struct PairContainer *container,
                    const int max_buf_len,
                    uint8_t *buf,
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

int setContainerFilter(struct PairContainer *container,
                       const uint8_t *filter) {
  struct StringPair *str;

  for (str = container->first; str; str = str->next) {
    if (filter) {
      /*
       * TODO(yjlou):
       * Now, we treat the inputing filter string as plain string.
       * Will support regular expression syntax in future if needed.
       */
      if (strcmp((char*)str->key, (char*)filter)) {
        str->filter_out = 1;
      }
    } else {
      str->filter_out = 0;
    }
  }
  return VPD_OK;
}

/* Export the container content with human-readable text. */
int exportContainer(const int export_type,
                    const struct PairContainer *container,
                    const int max_buf_len,
                    uint8_t *buf,
                    int *generated) {
  struct StringPair *str;
  int index;

  assert(generated);
  index = *generated;

  for (str = container->first; str; str = str->next) {
    int copy_len;

    if (str->filter_out)
      continue;

    if (export_type == VPD_EXPORT_AS_PARAMETER) {
      char pad_str[32];
      int pad_len;

      snprintf(pad_str, sizeof(pad_str), "-p %d -s ", str->pad_len);
      pad_len = strlen(pad_str);
      if ((index + pad_len) > max_buf_len) return VPD_FAIL;
      strcpy((char*)&buf[index], (char*)pad_str);
      index += pad_len;
    }

    if (export_type != VPD_EXPORT_VALUE) {
      /* double quote */
      if ((index + 1 > max_buf_len)) return VPD_FAIL;
      buf[index++] = '"';

      /* output key */
      if ((index + strlen((char*)str->key)) > max_buf_len) return VPD_FAIL;
      strcpy((char*)&buf[index], (char*)str->key);
      index += strlen((char*)str->key);

      /* double quote */
      if ((index + 1 > max_buf_len)) return VPD_FAIL;
      buf[index++] = '"';

      /* equal sign */
      if ((index + 1 > max_buf_len)) return VPD_FAIL;
      buf[index++] = '=';

      /* double quote */
      if ((index + 1 > max_buf_len)) return VPD_FAIL;
      buf[index++] = '"';
    }

    /* output value */
    if (str->pad_len != VPD_AS_LONG_AS)
      copy_len = MIN(str->pad_len, strlen((char*)str->value));
    else
      copy_len = strlen((char*)str->value);
    if ((index + copy_len) > max_buf_len) return VPD_FAIL;
    memcpy(&buf[index], str->value, copy_len);
    index += copy_len;

    if (export_type != VPD_EXPORT_VALUE) {
      /* double quote */
      if ((index + 1 > max_buf_len)) return VPD_FAIL;
      buf[index++] = '"';

      if (export_type == VPD_EXPORT_AS_PARAMETER) {
        /* backslash */
        if ((index + 2 > max_buf_len)) return VPD_FAIL;
        buf[index++] = ' ';
        buf[index++] = '\\';
      }

      /* new line */
      if ((index + 1 > max_buf_len)) return VPD_FAIL;
      buf[index++] = '\n';
    }
  }

  /* null terminator */
  if ((index + 1 > max_buf_len)) return VPD_FAIL;
  buf[index++] = '\0';

  *generated = index;

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
