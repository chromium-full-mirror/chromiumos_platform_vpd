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
#include <getopt.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uuid/uuid.h>
#include "lib/flashrom.h"
#include "lib/fmap.h"
#include "lib/lib_vpd.h"
#include "lib/lib_smbios.h"
#include "lib/vpd.h"
#include "lib/vpd_tables.h"

/* The buffer length. Right now the VPD partition size on flash is 128KB. */
#define BUF_LEN (128 * 1024)

/* temp filename for partial read area. */
static uint8_t tmp_part_file[] = "/tmp/vpd.flashrom.XXXXXX";
/* temp filename for the full flash content (actually only partial content
 * is available). But for flashrom -w to match flash size, we need to keep this
 * file from flashromRead() to writeFlashrom(). */
static uint8_t tmp_full_file[] = "/tmp/vpd.flashrom.XXXXXX";

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
unsigned char buf[BUF_LEN];
int buf_len = 0;
int max_buf_len = sizeof(buf);

/* The EPS base address used to fill the EPS table entry.
 * If the VPD partition can be found in fmap, this points to the starting
 * offset of VPD partition. If not found, this is used to be the base address
 * to increase SPD and VPD 2.0 offset fields.
 *
 * User can overwrite this by -E argument.
 */
#define UNKNOWN_EPS_BASE ((uint32_t)-1)
uint32_t eps_base = UNKNOWN_EPS_BASE;
int eps_base_force_specified = 0;  /* a bool value to indicate if -E argument
                                    * is given. */

/* the fmap name of VPD. */
uint8_t fmap_vpd_area_name[FMAP_STRLEN] = "RO_VPD";

/* If found_vpd, replace the VPD partition when saveFile().
 * If not found, always create new file when saveFlie(). */
int found_vpd = 0;

/* The VPD partition offset and size in buf[]. The whole partition includes:
 *
 *   SMBIOS EPS
 *   SMBIOS tables[]
 *   SPD
 *   VPD 2.0 data
 *
 */
uint32_t vpd_offset = 0, vpd_size;  /* The whole partition */
/* Below offset are related to vpd_offset and assume positive.
 * Those are used in saveFile() to write back data. */
uint32_t eps_offset = 0;  /* EPS's starting address. Tables[] is following. */
uint32_t spd_offset = GOOGLE_SPD_OFFSET;  /* SPD address .*/
off_t vpd_2_0_offset = GOOGLE_VPD_2_0_OFFSET; /* VPD 2.0 data address. */

/* This points to the SPD data if it is availiable when loadFile().
 * The memory is allocated in loadFile(), will be used in saveFile(),
 * and freed at end of main(). */
uint8_t *spd_data = NULL;
int32_t spd_len = 256;  /* max value for DDR3 */


int myMkTemp(uint8_t *tmp_file) {
  int fd;
  fd = mkstemp(tmp_file);
  if (fd < 0) {
    fprintf(stderr, "mkstemp(%s) failed\n", tmp_file);
  }
  return fd;
}

/*  Given the offset of blob block (related to the first byte of EPS) and
 *  the size of blob, the is function generates an SMBIOS ESP.
 */
int buildEpsAndTables(
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
  assert(eps_base != UNKNOWN_EPS_BASE);

  buf += *generated;

  /* Generate type 241 - SPD data */
  table_len = vpd_append_type241(0, &table, table_len,
                                 GOOGLE_SPD_UUID,
                                 eps_base + GOOGLE_SPD_OFFSET,
                                 spd_len,  /* Max length for DDR3 */
                                 GOOGLE_SPD_VENDOR,
                                 GOOGLE_SPD_DESCRIPTION,
                                 GOOGLE_SPD_VARIANT);
  if (table_len < 0) {
    retval = VPD_FAIL;
    goto error_1;
  }
  num_structures++;

  /* Generate type 241 - VPD 2.0 */
  table_len = vpd_append_type241(1, &table, table_len,
                                 GOOGLE_VPD_2_0_UUID,
                                 eps_base + GOOGLE_VPD_2_0_OFFSET,
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
  table_len = vpd_append_type127(2, &table, table_len);
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


/* Given an address, compare if it is SMBIOS signature ("_SM_"). */
int isEps(const void* ptr) {
  return !memcmp(VPD_ENTRY_MAGIC, ptr, sizeof(VPD_ENTRY_MAGIC) - 1);
  /* TODO(yjlou): need more EPS sanity checks here. */
}


/* There are two possible file content appearng here:
 *   1. a full and complete BIOS file
 *   2. a full but only VPD partition area is valid. (no fmap)
 *   3. a full BIOS, but VPD partition is blank.
 *
 * The first case is easy. Just lookup the fmap and find out the VPD partition.
 * The second is harder. We try to search the SMBIOS signature (since others
 * are blank). For the third, we just return and leave caller to read full
 * content, including fmap info.
 *
 * If found, vpd_offset and vpd_size are updated.
 */
int findVpdPartition(const uint8_t* read_buf, const uint32_t file_size,
                     uint32_t* vpd_offset, uint32_t* vpd_size) {
  off_t sig_offset;
  struct fmap *fmap;
  int i;

  assert(read_buf);
  assert(vpd_offset);
  assert(vpd_size);

  /* scan the file and find out the VPD partition. */
  sig_offset = fmapFind(read_buf, file_size);
  if (-1 != sig_offset) {
    /* FMAP signature is found, try to search the partition name in table. */
    fmap = (struct fmap *)&read_buf[sig_offset];
    for(i = 0; i < fmap->nareas; i++) {
      fmapNormalizeAreaName(fmap->areas[i].name);
    }

    if (FMAP_OK == fmapGetArea(fmap_vpd_area_name, fmap,
                               vpd_offset, vpd_size)) {
      found_vpd = 1;  /* Mark found here then saveFile() knows where to
                       * write back (vpd_offset, vpd_size). */
      return 0;
    } else {
      fprintf(stderr, "[ERROR] The VPD partition [%s] is not found.\n",
              fmap_vpd_area_name);
      return 1;
    }
  }

  /* The signature must be aligned to 16-byte. */
  for (i = 0; i < file_size; i += 16) {
    if (isEps(&read_buf[i])) {
      found_vpd = 1;  /* Mark found here then saveFile() knows where to
                       * write back (vpd_offset, vpd_size). */
      *vpd_offset = i;
      /* FIXME: We don't know the VPD partition size in this case.
       *        However, return 4K should be safe enough now.
       *        In the long term, this code block will be obscured. */
      *vpd_size = 4096;
      return 0;
    }
  }
  return 1;
}


/* Load file content into memory.
 * Returns: NULL if file opens error or read error.
 *          Others, pointer to the memory. The filesize is also returned.
 *
 * Note: it's caller's responsbility to free the memory.
 */
uint8_t *readFileContent(const char* filename, uint32_t *filesize) {
  FILE *fp;
  uint8_t *read_buf;

  assert(filename);
  assert(filesize);

  if (!(fp = fopen(filename, "r"))) {
    return NULL;
  }
  /* get file size */
  fseek(fp, 0, SEEK_END);
  *filesize = ftell(fp);

  /* read file content */
  fseek(fp, 0, SEEK_SET);
  read_buf = malloc(*filesize);
  assert(read_buf);
  if (*filesize != fread(read_buf, 1, *filesize, fp)) {
    fprintf(stderr, "[ERROR] Reading file [%s] failed.\n", filename);
    return NULL;
  }
  fclose(fp);

  return read_buf;
}


int getVpdPartitionFromFullBios(uint32_t* offset, uint32_t* size) {
  uint8_t filename[] = "/tmp/vpd.flashrom.XXXXXX";
  uint8_t *buf;
  uint32_t buf_size;
  int retval;

  if (myMkTemp(filename) < 0) {
    return 1;
  }

  if (FLASHROM_OK != flashromFullRead(filename)) {
    fprintf(stderr, "[WARN] Cannot read full BIOS.\n");
    return 1;
  }
  assert(buf = readFileContent(filename, &buf_size));
  if (findVpdPartition(buf, buf_size, offset, size)) {
    fprintf(stderr, "[WARN] Cannot get eps_base from full BIOS.\n");
    retval = 1;
  } else {
    retval = 0;
  }
  free(buf);
  return retval;
}


int loadFile(const char *filename, struct PairContainer *container,
             int overwrite_it) {
  uint32_t file_size;
  uint8_t *read_buf;
  uint8_t *vpd_buf;
  struct vpd_entry *eps;
  uint32_t related_eps_base;
  struct vpd_header *header;
  struct vpd_table_binary_blob_pointer *data;
  uint8_t spd_uuid[16], vpd_2_0_uuid[16];
  int expected_handle = 0;
  int table_len;
  int index;
  int retval = 0;

  if (!(read_buf = readFileContent(filename, &file_size))) {
    fprintf(stderr, "[WARN] Cannot LoadFile('%s'), that's fine.\n", filename);
    return 0;
  }

  if (0 == findVpdPartition(read_buf, file_size, &vpd_offset, &vpd_size)) {
    if (!eps_base_force_specified) {
      eps_base = vpd_offset;
    }
  } else {
    /* We cannot parse out the VPD partition address from given file.
     * Then, try to read the whole BIOS chip. */
    uint32_t offset, size;
    if (!eps_base_force_specified) {
      if (0 == getVpdPartitionFromFullBios(&offset, &size)) {
        eps_base = offset;
        vpd_size = size;
      } else {
        if (overwrite_it) {
          retval = 0;
        } else {
          fprintf(stderr, "[ERROR] getVpdPartitionFromFullBios() failed.");
          retval = 1;
        }
        goto teardown;
      }
    }
  }

  /* Update the following variables:
   *   eps_base: integer, the VPD EPS address in ROM.
   *   vpd_offset: integer, the VPD partition offset in file (read_buf[]).
   *   vpd_buf: uint8_t*, points to the VPD partition.
   *   eps: vpd_entry*, points to the EPS structure.
   *   eps_offset: integer, the offset of EPS related to vpd_buf[].
   */
  vpd_buf = &read_buf[vpd_offset];
  /* eps and eps_offset will be set slightly later. */

  if (eps_base == UNKNOWN_EPS_BASE) {
    fprintf(stderr, "[ERROR] Cannot determine eps_base. Cannot go on.\n"
                    "        You may use -E to specify the value.\n");
    retval = 1;
    goto teardown;
  }

  /* In overwrite mode, we don't care the content inside. Stop parsing. */
  if (overwrite_it) {
    retval = 0;
    goto teardown;
  }

  if (vpd_size < sizeof(struct vpd_entry)) {
    fprintf(stderr, "[ERROR] vpd_size:%d is too small to be compared.\n",
            vpd_size);
    retval = 1;
    goto teardown;
  }
  /* try to search the EPS if it is not aligned to the begin of partition. */
  for (index = 0; index < vpd_size; index += 16) {
    if (isEps(&vpd_buf[index])) {
      eps = (struct vpd_entry *)&vpd_buf[index];
      eps_offset = index;
      break;
    }
  }
  /* jump if the VPD partition is not recognized. */
  if (index >= vpd_size) {
    /* But OKAY if the VPD partition starts with FF, which might be un-used. */
    if (!memcmp("\xff\xff\xff\xff", vpd_buf, sizeof(VPD_ENTRY_MAGIC) - 1)) {
      fprintf(stderr, "[WARN] VPD partition not formatted. It's fine.\n");
      retval = 0;
      goto teardown;
    } else {
      fprintf(stderr, "SMBIOS signature is not matched.\n");
      fprintf(stderr, "You may use -O to overwrite the data.\n");
      retval = 1;
      goto teardown;
    }
  }

  /* adjust the eps_base for data->offset field below. */
  if (!eps_base_force_specified) {
    related_eps_base = eps->table_address - sizeof(*eps);
  } else {
    related_eps_base = eps_base;
  }

  /* EPS is done above. Parse structure tables below. */
  /* Get the first type 241 blob, at the tail of EPS. */
  header = (struct vpd_header*)(((uint8_t*)eps) + eps->entry_length);
  data = (struct vpd_table_binary_blob_pointer *)
         ((uint8_t *)header + sizeof(*header));

  /* TODO(yjlou): Re-factor the parsing code to support more SMBIOS entries.
   *              The current code only supports 2 combinations:
   *                1. Type 241 (SPD) + Type 241 (VPD 2.0) + Type 127
   *                2. Type 241 (VPD 2.0) + Type 127
   */

  /* Now header points to the first SMBIOS entry, and data points to the
   * first BBP entry. The first entry could be SPD data. We don't care. */
  if (header->handle != expected_handle) {
    fprintf(stderr, "[ERROR] The first handle value must be 0, but is %d.\n"
                    "        Use -O option to re-format.\n",
                    header->handle);
    retval = 1;
    goto teardown;
  }
  if (header->type != VPD_TYPE_BINARY_BLOB_POINTER) {
    fprintf(stderr, "Expect first entry is type Binary Blob Pointer (241),"
                    " but actually is %d\n", header->type);
    fprintf(stderr, "You may use -O to overwrite the data.\n");
    retval = 1;
    goto teardown;
  }
  uuid_parse(GOOGLE_SPD_UUID, spd_uuid);
  if (!memcmp(data->uuid, spd_uuid, sizeof(data->uuid))) {
    ++expected_handle;
    spd_offset = data->offset - related_eps_base;
    spd_len = data->size;
    if (vpd_offset + spd_offset + spd_len >= file_size) {
      fprintf(stderr, "[ERROR] SPD offset in BBP is not correct.\n"
                      "        vpd=0x%x spd=0x%x len=0x%x file_size=0x%x\n"
                      "        If this file is VPD partition only, try to\n"
                      "        use -E to adjust offset values.\n",
                      (uint32_t)vpd_offset, (uint32_t)spd_offset,
                      spd_len, file_size);
      retval = 1;
      goto teardown;
    }
    if (!(spd_data = malloc(spd_len))) {
      fprintf(stderr, "spd_data: malloc(%d bytes) failed.\n", spd_len);
      retval = 1;
      goto teardown;
    }
    memcpy(spd_data, &read_buf[vpd_offset + spd_offset], spd_len);

    /* move to next table */
    if ((table_len = vpd_type241_size(header)) < 0) {
      fprintf(stderr, "[ERROR] Cannot get type 241 structure table length.\n");
      retval = 1;
      goto teardown;
    }
    header = (struct vpd_header*)((uint8_t*)header + table_len);
    data = (struct vpd_table_binary_blob_pointer *)
           ((uint8_t *)header + sizeof(*header));
  }

  /* The 2nd could be VPD 2.0 data or End Of Table. */
  if (header->handle != expected_handle) {
    fprintf(stderr, "[ERROR] The second handle value must be 1, but is %d.\n"
                    "        Use -O option to re-format.\n",
                    header->handle);
    retval = 1;
    goto teardown;
  }
  uuid_parse(GOOGLE_VPD_2_0_UUID, vpd_2_0_uuid);
  if (header->type == VPD_TYPE_BINARY_BLOB_POINTER &&
      !memcmp(data->uuid, vpd_2_0_uuid, sizeof(data->uuid))) {
    ++expected_handle;

    /* iterate all pairs */
    for (index = data->offset - related_eps_base;  /* skip the EPS */
         vpd_buf[index] != VPD_TYPE_TERMINATOR &&
         vpd_buf[index] != VPD_TYPE_IMPLICIT_TERMINATOR;) {
      if (VPD_OK != decodeVpdString(file_size, vpd_buf, container, &index)) {
        fprintf(stderr, "decodeVpdString() error.\n");
        retval = 1;
        goto teardown;
      }
    }

    /* move to next table */
    if ((table_len = vpd_type241_size(header)) < 0) {
      fprintf(stderr, "[ERROR] Cannot get type 241 structure table length.\n");
      retval = 1;
      goto teardown;
    }
    header = (struct vpd_header*)((uint8_t*)header + table_len);
    data = (struct vpd_table_binary_blob_pointer *)
           ((uint8_t *)header + sizeof(*header));
  } else {
    fprintf(stderr, "[WARN] no VPD 2.0 BBP is found, ignored.\n");
    retval = 0;
    goto teardown;
  }

  if (header->type != VPD_TYPE_END) {
    fprintf(stderr, "[WARN] we expect the last one is type 127. It's fine.\n");
  }

teardown:
  free(read_buf);

  return retval;
}


int saveFile(const struct PairContainer *container, const char *filename,
             int write_back_to_flash) {
  FILE *fp;
  unsigned char eps[1024];
  int eps_len = 0;
  int retval = 0;
  uint32_t file_seek;

  memset(eps, 0xff, sizeof(eps));

  /* encode into buffer */
  if (VPD_OK != encodeContainer(&file, max_buf_len, buf, &buf_len)) {
    fprintf(stderr, "encodeContainer() error.\n");
    retval = 1;
    goto teardown;
  }
  if (VPD_OK != encodeVpdTerminator(max_buf_len, buf, &buf_len)) {
    fprintf(stderr, "Out of space for terminator.\n");
    retval = 1;
    goto teardown;
  }

  if (VPD_OK != buildEpsAndTables(buf_len, sizeof(eps), eps, &eps_len)) {
    fprintf(stderr, "Cannot build EPS.\n");
    retval = 1;
    goto teardown;
  }
  assert(eps_len <= GOOGLE_SPD_OFFSET);

  /* Write data in the following order:
   *   1. EPS
   *   2. SPD
   *   3. VPD 2.0
  */
  if (found_vpd) {
    /* We found VPD partition in -f file, which means file is existed.
     * Instead of truncating the whole file, open to write partial. */
    if (!(fp = fopen(filename, "r+"))) {
      fprintf(stderr, "File [%s] cannot be opened for write.\n", filename);
      retval = 1;
      goto teardown;
    }
  } else {
    /* VPD is not found, which means the file is pure VPD data.
     * Always creates the new file and overwrites the original content. */
    if (!(fp = fopen(filename, "w+"))) {
      fprintf(stderr, "File [%s] cannot be opened for write.\n", filename);
      retval = 1;
      goto teardown;
    }
  }

  if (write_back_to_flash) {
    file_seek = 0;
  } else {
    file_seek = vpd_offset;
  }

  /* write EPS */
  fseek(fp, file_seek + eps_offset, SEEK_SET);
  if (fwrite(eps, eps_len, 1, fp) != 1) {
    fprintf(stderr, "fwrite(EPS) error (%s)\n", strerror(errno));
    retval = 1;
    goto teardown;
  }

  /* write SPD */
  if (spd_data) {
    fseek(fp, file_seek + spd_offset, SEEK_SET);
    if (fwrite(spd_data, spd_len, 1, fp) != 1) {
      fprintf(stderr, "fwrite(SPD) error (%s)\n", strerror(errno));
      retval = 1;
      goto teardown;
    }
  }

  /* write VPD 2.0 */
  fseek(fp, file_seek + vpd_2_0_offset, SEEK_SET);
  if (fwrite(buf, buf_len, 1, fp) != 1) {
    fprintf(stderr, "fwrite(VPD 2.0) error (%s)\n", strerror(errno));
    retval = 1;
    goto teardown;
  }
  fclose(fp);

teardown:
  return retval;
}

int generateTempFilenames(void) {
  if (myMkTemp(tmp_part_file) < 0) return -1;
  if (myMkTemp(tmp_full_file) < 0) return -1;
  return 0;
}

static void usage(const char *progname) {
  printf("Chrome OS VPD 2.0 utility --\n");
#ifdef VPD_VERSION
  printf("%s\n", VPD_VERSION);
#endif
  printf("\n");
  printf("Usage: %s [OPTION] ...\n", progname);
  printf("   OPTIONs include:\n");
  printf("      -h               This help page and version.\n");
  printf("      -f <filename>    The output file name.\n");
  printf("      -E <address>     EPS base address (default:0x240000).\n");
  printf("      -s <key=value>   To add/change a string value.\n");
  printf("      -p <pad length>  Pad if length is shorter.\n");
  printf("      -i <partition>   Specify VPD partition name in fmap.\n");
  printf("      -l               List content in the file.\n");
  printf("      -O               Overwrite and re-format VPD partition.\n");
  printf("      -g <key>         Print value string only.\n");
  printf("\n");
}

int main(int argc, char *argv[]) {
  int opt;
  int option_index = 0;
  int retval = 0;
  const char *optstring = "hf:E:s:p:i:lOg:";
  static struct option long_options[] = {
    {"help", 0, 0, 'h'},
    {"file", 0, 0, 'f'},
    {"epsbase", 0, 0, 'E'},
    {"string", 0, 0, 's'},
    {"pad", 0, 0, 'p'},
    {"partition", 0, 0, 'i'},
    {"list", 0, 0, 'l'},
    {"overwrite", 0, 0, 'O'},
    {"filter", 0, 0, 'g'},
    {0, 0, 0, 0}
  };
  char *filename = NULL;
  char *load_file = NULL;
  char *save_file = NULL;
  char *filter_str = NULL;
  int write_back_to_flash = 0;
  int list_it = 0;
  int overwrite_it = 0;
  int modified = 0;
  int fd;

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
        eps_base_force_specified = 1;

        /* FIXME: this is not a stable way to detect error because
         *        implementation may (or may not) assign errno. */
        if (!eps_base && errno == EINVAL) {
          fprintf(stderr, "Not a number for EPS base address: %s\n", optarg);
          retval = 1;
          goto teardown;
        }
        break;

      case 's':
        if (VPD_OK != parseString(optarg)) {
          fprintf(stderr, "The string [%s] cannot be parsed.\n\n", optarg);
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
          fprintf(stderr, "Not a number for pad length: %s\n", optarg);
          retval = 1;
          goto teardown;
        }
        break;

      case 'i':
        snprintf(fmap_vpd_area_name, sizeof(fmap_vpd_area_name), "%s", optarg);
        break;

      case 'l':
        list_it = 1;
        break;

      case 'O':
        overwrite_it = 1;
        modified = 1;  /* This option forces to write empty data back even
                        * no new pair is given. */
        break;

      case 'g':
        filter_str = strdup(optarg);
        break;

      default:
        fprintf(stderr, "Invalid option (%s), use --help for usage.\n", optarg);
        retval = 1;
        goto teardown;
        break;
    }
  }

  if (optind < argc) {
    fprintf(stderr, "[ERROR] unexpected argument: %s\n\n", argv[optind]);
    usage(argv[0]);
    retval = 1;
    goto teardown;
  }

  if (generateTempFilenames() < 0) {
    fprintf(stderr, "[ERROR] generateTempFilenames() returns failed.\n");
    retval = 1;
    goto teardown;
  }

  if (list_it && filter_str) {
    fprintf(stderr, "[ERROR] -l and -g must be mutually exclusive.\n");
    retval = 1;
    goto teardown;
  }

  /* to avoid malicious attack, we replace suspicious chars. */
  fmapNormalizeAreaName(fmap_vpd_area_name);

  /* if no filename is specified, call flashrom to read from flash. */
  if (!filename) {
    if (FLASHROM_OK != flashromPartialRead(tmp_part_file, tmp_full_file,
                                           fmap_vpd_area_name)) {
      fprintf(stderr, "flashromPartialRead() error!\n");
      retval = 1;
      goto teardown;
    }

    write_back_to_flash = 1;
    load_file = tmp_full_file;
    save_file = tmp_part_file;
  } else {
    load_file = filename;
    save_file = filename;
  }

  if (retval = loadFile(load_file, &file, overwrite_it)) {
    fprintf(stderr, "loadFile('%s') error.\n", load_file);
    goto teardown;
  }

  mergeContainer(&file, &argument);

  if (list_it || filter_str) {
    /* Reserve larger size because the exporting generates longer string than
     * the encoded data. */
    uint8_t list_buf[BUF_LEN * 2];
    int list_len = 0;

    if (filter_str) setContainerFilter(&file, filter_str);
    if (VPD_OK != exportContainer(
          (filter_str) ? VPD_EXPORT_VALUE : VPD_EXPORT_KEY_VALUE,
          &file, sizeof(list_buf), list_buf, &list_len)) {
      fprintf(stderr, "exportContainer(): Cannot generate string.\n");
      retval = 1;
      goto teardown;
    }
    printf("%s", list_buf);
  }

  if (modified) {
    if (retval = saveFile(&file, save_file, write_back_to_flash)) {
      fprintf(stderr, "saveFile('%s') error.\n", filename);
      goto teardown;
    }

    if (write_back_to_flash) {
      if (FLASHROM_OK != flashromPartialWrite(save_file, tmp_full_file,
                                              fmap_vpd_area_name)) {
        fprintf(stderr, "flashromPartialWrite() error.\n");
        retval = 1;
        goto teardown;
      }
    }
  }

teardown:
  if (spd_data) free(spd_data);
  if (filename) free(filename);
  if (filter_str) free(filter_str);
  destroyContainer(&file);
  destroyContainer(&argument);

  return retval;
}
