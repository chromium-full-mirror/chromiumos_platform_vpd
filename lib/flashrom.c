/*
 * Copyright 2010, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *    * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lib/flashrom.h"

/* The best choice is PATH_MAX. However, it leads portibility issue.
 * So, define a long enough length here. */
#define CMD_BUF_SIZE (4096)

#define PARTIAL_WRITE_AREA_NAME "pw_name"

static uint8_t layout_filename[] = "/tmp/vpd.layout.XXXXXX";

static uint8_t flashrom_cmd[] = "flashrom";

/* The argument for flashrom.
 *   bus=spi: The VPD data are stored in BIOS flash, which is attached
 *            to the SPI bus.
 */
static uint8_t flashrom_arguments[] = " -p internal:bus=spi ";

int flashromRead(const char* tmp_file) {
  char cmd[CMD_BUF_SIZE];
  int ret = 0;

  snprintf(cmd, sizeof(cmd), "%s %s -r '%s' >/dev/null 2>&1",
           flashrom_cmd, flashrom_arguments, tmp_file);
  ret = system(cmd);
  if (ret == 0)
    return FLASHROM_OK;
  else
    return FLASHROM_FAIL;
}

int flashromPartialWrite(const char* tmp_file,
                         const off_t section_offset,
                         const size_t section_size) {
  int fd;
  char cmd[CMD_BUF_SIZE];
  FILE* fp;
  int ret = 0;

  fd = mkstemp(layout_filename);
  if (fd < 0) {
    fprintf(stderr, "mkstemp(%s) failed.\n", layout_filename);
    return FLASHROM_FAIL;
  } else {
    close(fd);
  }

  /* generate layout file */
  if (!(fp = fopen(layout_filename, "w"))) {
    fprintf(stderr, "cannot create layout file: [%s]\n", layout_filename);
    return FLASHROM_FAIL;
  }
  fprintf(fp, "0x%lx:0x%lx %s\n", section_offset,
                                  section_offset + section_size - 1,
                                  PARTIAL_WRITE_AREA_NAME);
  fclose(fp);

  /* write it back */
  snprintf(cmd, sizeof(cmd), "%s %s -l %s -i '%s' -w '%s' >/tmp/null 2>&1",
           flashrom_cmd, flashrom_arguments, layout_filename,
           PARTIAL_WRITE_AREA_NAME, tmp_file);
  ret = system(cmd);

  if (ret == 0)
    return FLASHROM_OK;
  else
    return FLASHROM_FAIL;
}
