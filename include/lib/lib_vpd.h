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

#ifndef __LIB_VPD__
#define __LIB_VPD__

enum {
  VPD_OK = 0,
  VPD_FAIL,
};

enum {
  VPD_TYPE_TERMINATOR = 0,
  VPD_TYPE_STRING,
};

enum {
  VPD_AS_LONG_AS = -1,
};


struct StringPair {
  unsigned char *key;
  unsigned char *value;
  int pad_len;
  struct StringPair *next;
};

struct PairContainer {
  struct StringPair *first;
};


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
    int *generated_len);

/*  Encodes the terminator.
 */
int encodeVpdTerminator(
    const int max_buffer_len,
    unsigned char *output_buf,
    int *generated_len);

/* Encodes the given type/key/value pair into buffer.
 *
 * The pad_value_len is used to control the output value length.
 * When pad_value_len > 0, the value is outputted into fixed length (pad \0
 *                         if the value is shorter). Truncated if too long.
 *      pad_value_len == VPD_NO_LIMIT, output the value as long as possible.
 * This is useful when we want to fix the structure layout at beginning.
 *
 * The encoded string will be stored in output_buf. If it is longer than
 * max_buffer_len, this function returns fail. If the buffer is long enough,
 * the generated_len will be updated.
 *
 * The initial value of *generated_len can be non-zero, so that this value
 * can be used between multiple calls to encodeVpd2Pair().
 */
int encodeVpdString(
    const unsigned char *key,
    const unsigned char *value,
    const int pad_value_len,
    const int max_buffer_len,
    unsigned char *output_buf,
    int *generated_len);


/***********************************************************************
 * Container helpers
 ***********************************************************************/
void initContainer(struct PairContainer *container);

struct StringPair *findString(const struct PairContainer *container,
                              const unsigned char *key);

/* If key is already existed in container, its value will be replaced.
 * If not existed, creates new entry in container.
 */
void setString(struct PairContainer *container,
               const unsigned char *key,
               const unsigned char *value,
               const int pad_len);

/* merge all entries in src into dst. If key is duplicate, overwrite it.
 */
void mergeContainer(struct PairContainer *dst,
                    const struct PairContainer *src);

/* Given a container, encode its all entries into the buffer.
 */
int encodeContainer(const struct PairContainer *container,
                    const int max_buf_len,
                    unsigned char *buf,
                    int *generated);

void destroyContainer(struct PairContainer *container);


#endif  /* __LIB_VPD__ */
