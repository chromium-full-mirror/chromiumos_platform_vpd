/*
 * Copyright 2019 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef INCLUDE_LIB_VPD_DECODE_H_
#define INCLUDE_LIB_VPD_DECODE_H_

enum {
  VPD_TYPE_TERMINATOR = 0,
  VPD_TYPE_STRING,
  VPD_TYPE_INFO = 0xfe,
  VPD_TYPE_IMPLICIT_TERMINATOR = 0xff,
};

#endif /* INCLUDE_LIB_VPD_DECODE_H_ */
