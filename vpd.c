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
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "lib/lib_vpd.h"
#include "lib/lib_smbios.h"
#include "lib/vpd.h"
#include "lib/vpd_tables.h"

/* 2 containers:
 *   file:      stores decoded pairs from file.
 *   argument:  stores parsed pairs from command arguments.
 */
struct PairContainer file;
struct PairContainer argument;

/* The current padding length value.
 * Default: VPD_AS_LONG_AS
 */
int pad_value_len = VPD_AS_LONG_AS;

/* The output buffer */
unsigned char buf[65536];
int buf_len = 0;
int max_buf_len = sizeof(buf);

/* TODO: refine this after we integrate fmap */
uint32_t eps_base = GOOGLE_VPD_2_0_EPS_BASE;


/* for debug purpose */
void dumpBuf() {
  int i;
  for(i = 0; i < buf_len; ++i) {
    printf("%02x ", buf[i]);
    if ((i % 16) == 15) printf("\n");
  }
}


/*  Given the offset of blob block (related to the first byte of EPS) and
 *  the size of blob, the is function generates an SMBIOS ESP.
 */
int buildEpsAndTables(
    const int offset_blob,
    const int size_blob,
    const int max_buf_len,
    unsigned char *buf,
    int *generated) {
  struct vpd_entry *eps;
  unsigned char *table = NULL;          /* the structure table */
  int table_len = 0;
  int num_structures = 0;
  int retval = VPD_OK;

  assert(buf);
  assert(generated);

  buf += *generated;

  /* Generate type 241 */
  table_len = vpd_append_type241(0, &table, table_len,
                                 GOOGLE_VPD_2_0_UUID,
                                 eps_base + offset_blob,
                                 size_blob,
                                 GOOGLE_VPD_2_0_VENDOR,
                                 GOOGLE_VPD_2_0_DESCRIPTION,
                                 GOOGLE_VPD_2_0_VARIANT);
  if (table_len < 0) {
    retval = VPD_FAIL;
    goto error_1;
  }
  num_structures++;

  /* Generate type 127 */
  table_len = vpd_append_type127(1, &table, table_len);
  if (table_len < 0) {
    retval = VPD_FAIL;
    goto error_1;
  }
  num_structures++;

  /* Generate EPS */
  eps = vpd_create_eps(table_len, num_structures, eps_base);
  if ((*generated + eps->entry_length) > max_buf_len) {
    retval = VPD_FAIL;
    goto error_2;
  }

  /* Copy EPS back to buf */
  memcpy(buf, eps, eps->entry_length);
  buf += eps->entry_length;
  *generated += eps->entry_length;

  /* Copy tables back to buf */
  if ((*generated + table_len) > max_buf_len) {
    retval = VPD_FAIL;
    goto error_2;
  }
  memcpy(buf, table, table_len);
  buf += table_len;
  *generated += table_len;

error_2:
  free(eps);
error_1:
  free(table);

  return retval;
}


/*  Given a key=value string, this function parses it and adds to arugument
 *  pair container.
 */
int parseString(const uint8_t *string) {
  char *key;
  char *value;
  int retval = VPD_OK;

  key = strdup(string);
  key = strtok(key, "=");
  /* We accept empty value: "KEY=" */
  if (!(value = strtok(NULL, "=")))
    value = "";

  setString(&argument, key, value, pad_value_len);

error:
  free(key);
  return retval;
}


int loadFile(const char *filename, struct PairContainer *container) {
  FILE *fp;
  int file_size;
  char *read_buf;
  struct vpd_entry *eps;
  struct vpd_header *header;
  struct vpd_table_binary_blob_pointer *data;
  int index;
  int retval = 0;

  if (!(fp = fopen(filename, "r"))) {
    fprintf(stderr, "* File [%s] cannot be opened for read. Ignored.\n",
            filename);
    return 0;
  }
  /* get file size */
  fseek(fp, 0, SEEK_END);
  file_size = ftell(fp);

  /* read file content */
  fseek(fp, 0, SEEK_SET);
  read_buf = malloc(file_size);
  assert(read_buf);
  if (file_size != fread(read_buf, 1, file_size, fp)) {
    printf("Reading file [%s] failed.\n", filename);
    retval = 1;
    goto teardown;
  }
  eps = (struct vpd_entry *)read_buf;
  fclose(fp);

  /* Get type 241 blob */
  header = (struct vpd_header*)(((uint8_t*)eps) + eps->entry_length);
  data = (struct vpd_table_binary_blob_pointer *)
         ((uint8_t *)header + sizeof(*header));

  /* iterate all pairs */
  for (index = data->offset - eps_base;  /* skip the EPS */
       read_buf[index] != VPD_TYPE_TERMINATOR &&
       read_buf[index] != VPD_TYPE_IMPLICIT_TERMINATOR;) {
    if (VPD_OK != decodeVpdString(file_size, read_buf, container, &index)) {
      retval = 1;
      goto teardown;
    }
  }

teardown:
    free(read_buf);
  return retval;
}


int saveFile(const struct PairContainer *container, const char *filename) {
  FILE *fp;
  unsigned char eps[1024];
  int eps_len = 0;
  int retval = 0;

  /* encode into buffer */
  if (VPD_OK != encodeContainer(&file, max_buf_len, buf, &buf_len)) {
    printf("encodeContainer() error.\n");
    retval = 1;
    goto teardown;
  }
  if (VPD_OK != encodeVpdTerminator(max_buf_len, buf, &buf_len)) {
    printf("Out of space for terminator.\n");
    retval = 1;
    goto teardown;
  }

  if (VPD_OK != buildEpsAndTables(GOOGLE_VPD_2_0_OFFSET, buf_len,
                                  sizeof(eps), eps, &eps_len)) {
    printf("Cannot build EPS.\n");
    retval = 1;
    goto teardown;
  }
  assert(eps_len <= GOOGLE_VPD_2_0_OFFSET);
  eps_len = GOOGLE_VPD_2_0_OFFSET;

  /* write into file */
  if (!(fp = fopen(filename, "w"))) {
    printf("File [%s] cannot be opened for write.\n", filename);
    retval = 1;
    goto teardown;
  } else {
    fwrite(eps, eps_len, 1, fp);
    fwrite(buf, buf_len, 1, fp);
    fclose(fp);
  }

teardown:
  return retval;
}


static void usage(const char *progname) {
  printf("Usage: %s [OPTION] ...\n", progname);
  printf("   OPTIONs include:\n");
  printf("      -h               This help page.\n");
  printf("      -f <filename>    The output file name.\n");
  printf("      -E <address>     EPS base address (default:0x240000).\n");
  printf("      -s <key=value>   To add/change a string value.\n");
  printf("      -p <pad length>  Pad if length is shorter.\n");
  printf("      -l               List content in the file.\n");
  printf("\n");
}

int main(int argc, char *argv[]) {
  int opt;
  int option_index = 0;
  int retval = 0;
  const char *optstring = "hf:s:p:lE:";
  static struct option long_options[] = {
    {"help", 0, 0, 'h'},
    {"file", 0, 0, 'f'},
    {"epsbase", 0, 0, 'E'},
    {"string", 0, 0, 's'},
    {"pad", 0, 0, 'p'},
    {"list", 0, 0, 'l'},
  };
  char *filename = NULL;
  int list_it = 0;
  int modified = 0;

  initContainer(&file);
  initContainer(&argument);

  while ((opt = getopt_long(argc, argv, optstring,
                            long_options, &option_index)) != EOF) {
    switch (opt) {
      case 'h':
        usage(argv[0]);
        retval = 0;
        goto teardown;
        break;

      case 'f':
        filename = strdup(optarg);
        break;

      case 'E':
        errno = 0;
        eps_base = strtoul(optarg, (char **) NULL, 0);

        /* FIXME: this is not a stable way to detect error because
         *        implementation may (or may not) assign errno. */
        if (!eps_base && errno == EINVAL) {
          printf("Not a number for EPS base address: %s\n", optarg);
          retval = 1;
          goto teardown;
        }
        break;

      case 's':
        if (VPD_OK != parseString(optarg)) {
          printf("The string [%s] cannot be parsed.\n\n", optarg);
          retval = 1;
          goto teardown;
        }
        modified++;
        break;

      case 'p':
        errno = 0;
        pad_value_len = strtol(optarg, (char **) NULL, 0);

        /* FIXME: this is not a stable way to detect error because
         *        implementation may (or may not) assign errno. */
        if (!pad_value_len && errno == EINVAL) {
          printf("Not a number for pad length: %s\n", optarg);
          retval = 1;
          goto teardown;
        }
        break;

      case 'l':
        list_it = 1;
        break;

      default:
        printf("Invalid option, use --help for usage.\n");
        retval = 1;
        goto teardown;
        break;
    }
  }

  /* Sanity check */
  if (!filename) {
    printf("Need specify filename.\n\n");
    usage(argv[0]);
    retval = 1;
    goto teardown;
  }

  if (retval = loadFile(filename, &file)) {
    goto teardown;
  }

  mergeContainer(&file, &argument);

  if (modified && (retval = saveFile(&file, filename))) {
    goto teardown;
  }

  if (list_it) {
    buf_len = 0;
    if (VPD_OK != exportContainer(VPD_EXPORT_KEY_VALUE,
                                  &file, max_buf_len, buf, &buf_len)) {
      printf("exportContainer(): Cannot generate string.\n");
      retval = 1;
      goto teardown;
    }
    printf("%s", buf);
  }

teardown:
  if (filename) free(filename);
  destroyContainer(&file);
  destroyContainer(&argument);

  return retval;
}
