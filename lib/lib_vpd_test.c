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
 * Testing code for lib_vpd.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lib/lib_vpd.h"

enum {
  TEST_OK = 0,
  TEST_FAIL = 1,
};


int testEncodeLen() {
  unsigned char output[10];
  int generated;

  /* fail cases */
  assert(VPD_FAIL == encodeLen(-1, output, 0, &generated));
  assert(VPD_FAIL == encodeLen(0x7f, output, 0, &generated));

  /* success case - 1 byte output, all zeros */
  assert(VPD_OK == encodeLen(0x00, output, 1, &generated));
  assert(1 == generated);
  assert(0x00 == output[0]);

  /* success case - 1 byte output */
  assert(VPD_OK == encodeLen(0x7f, output, 1, &generated));
  assert(1 == generated);
  assert(0x7f == output[0]);

  /* 2 bytes of output */
  assert(VPD_FAIL == encodeLen(0x80, output, 1, &generated));
  /* success */
  assert(VPD_OK == encodeLen(0x80, output, 2, &generated));
  assert(2 == generated);
  assert(0x81 == output[0]);
  assert(0x00 == output[1]);

  /* 3 bytes of output */
  assert(VPD_FAIL == encodeLen(0x100040, output, 0, &generated));
  assert(VPD_FAIL == encodeLen(0x100040, output, 1, &generated));
  assert(VPD_FAIL == encodeLen(0x100040, output, 2, &generated));
  /* success */
  assert(VPD_OK == encodeLen(0x100040, output, 3, &generated));
  assert(3 == generated);
  assert(0xc0 == output[0]);
  assert(0x80 == output[1]);
  assert(0x40 == output[2]);

  printf("[PASS] %s()\n", __FUNCTION__);
  return TEST_OK;
}

int testEncodeVpdString() {
  unsigned char expected[] = {
    VPD_TYPE_STRING,
    0x03, 'K', 'E', 'Y',
    0x05, 'V', 'A', 'L', 'U', 'E',
  };
  unsigned char buf[256];
  int generated = 0;

  assert(VPD_OK ==
         encodeVpdString("KEY", "VALUE",
                         VPD_AS_LONG_AS, sizeof(buf), buf, &generated));
  assert(sizeof(expected) == generated);
  assert(!memcmp(expected, buf, generated));

  printf("[PASS] %s()\n", __FUNCTION__);
  return TEST_OK;
}

int testEncodeVpdStringPadding() {
  unsigned char expected[] = {
    VPD_TYPE_STRING,
    0x03, 'K', 'E', 'Y',
    0x08, 'V', 'A', 'L', 'U', 'E', '\0', '\0', '\0',
  };
  unsigned char buf[256];
  int generated = 0;

  assert(VPD_OK == encodeVpdString("KEY", "VALUE",
                                   8, sizeof(buf), buf, &generated));
  assert(sizeof(expected) == generated);
  assert(!memcmp(expected, buf, generated));

  printf("[PASS] %s()\n", __FUNCTION__);
  return TEST_OK;
}

int testEncodeMultiStrings() {
  unsigned char expected[] = {
    VPD_TYPE_STRING,
    0x03, 'M', 'A', 'C',
    0x08, '0', '1', '2', '3', '4', '5', '6', '7',
    VPD_TYPE_STRING,
    0x07, 'P', 'r', 'o', 'd', '/', 'I', 'd',
    0x0c, 'M', 'a', 'r', 'i', 'o', '0', '9', '2', '8', '4', '\0', '\0',
  };
  unsigned char buf[256];
  int generated = 0;

  assert(VPD_OK == encodeVpdString("MAC", "01234567",
                                   0x08, sizeof(buf), buf, &generated));
  assert(VPD_OK == encodeVpdString("Prod/Id", "Mario09284",
                                   0x0c, sizeof(buf), buf, &generated));
  assert(sizeof(expected) == generated);
  assert(!memcmp(expected, buf, generated));

  printf("[PASS] %s()\n", __FUNCTION__);
  return TEST_OK;
}

int testContainer() {
  unsigned char expected[] = {
    VPD_TYPE_STRING,
    0x03, 'K', 'E', 'Y',
    0x08, 'V', 'A', 'L', 'U', 'E', '\0', '\0', '\0',
  };
  unsigned char buf[256];
  int generated = 0;
  struct PairContainer container;

  initContainer(&container);
  setString(&container, "KEY", "VALUE", 8);
  encodeContainer(&container, sizeof(buf), buf, &generated);

  assert(sizeof(expected) == generated);
  assert(!memcmp(expected, buf, generated));

  printf("[PASS] %s()\n", __FUNCTION__);
  return TEST_OK;
}

int main() {
  assert(TEST_OK == testEncodeLen());
  assert(TEST_OK == testEncodeVpdString());
  assert(TEST_OK == testEncodeVpdStringPadding());
  assert(TEST_OK == testEncodeMultiStrings());
  assert(TEST_OK == testContainer());

  printf("SUCCESS!\n");
  return 0;
}
