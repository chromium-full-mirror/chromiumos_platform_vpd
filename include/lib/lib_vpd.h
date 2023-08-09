/*
 * Copyright 2013 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef INCLUDE_LIB_LIB_VPD_H_
#define INCLUDE_LIB_LIB_VPD_H_

enum vpd_err {
  /* These error codes are returned to the shell as exit codes. If they are
   * changed then callers such as Enterprise.VpdCheck from histograms.xml must
   * be updated along with tests/functions.sh. If codes are added then they
   * must not conflict with conventional exit codes:
   * http://www.tldp.org/LDP/abs/html/exitcodes.html
   */
  VPD_OK = 0,
  VPD_FAIL = 3,          /* generic vpd utility error */
  VPD_ERR_SYSTEM = 4,    /* system error (file error, out of memory, etc) */
  VPD_ERR_ROM_READ = 5,  /* error reading ROM (e.g. thru flashrom) */
  VPD_ERR_ROM_WRITE = 6, /* error writing ROM (e.g. thru flashrom) */
  VPD_ERR_SYNTAX = 7,    /* command syntax error */
  VPD_ERR_PARAM = 8,     /* invalid parameter specified by user */
  VPD_ERR_NOT_FOUND = 9, /* VPD not found */
  VPD_ERR_OVERFLOW = 10, /* boundary exceeded */
  VPD_ERR_INVALID = 11,  /* error in VPD - possible corruption or bug */
  VPD_ERR_DECODE = 12,   /* error when decoding VPD blob */
};

typedef enum vpd_err vpd_err_t;

/* export_type */
enum {
  VPD_EXPORT_KEY_VALUE = 1,
  VPD_EXPORT_VALUE,
  VPD_EXPORT_AS_PARAMETER,
  VPD_EXPORT_NULL_TERMINATE,
};

#endif /* INCLUDE_LIB_LIB_VPD_H_ */
