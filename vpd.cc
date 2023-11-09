/*
 * Copyright 2012 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <optional>
#include <string>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fmap.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <uuid/uuid.h>

#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>

extern "C" {
#include "lib/lib_vpd.h"
#include "lib/vpd.h"
#include "lib/vpd_tables.h"
};

#include "vpd/encoder.h"
#include "vpd/flashrom.h"

namespace {

/* The comment shown in the begin of --sh output */
#define SH_COMMENT                                                     \
  "#\n"                                                                \
  "# Prepend 'vpd -O' before this text to always reset VPD content.\n" \
  "# Append more -s at end to set additional key/values.\n"            \
  "# Or an empty line followed by other commands.\n"                   \
  "#\n"

/* The EPS base address used to fill the EPS table entry.
 * If the VPD partition can be found in fmap, this points to the starting
 * offset of VPD partition. If not found, this is used to be the base address
 * to increase the VPD 2.0 offset field.
 */
#define UNKNOWN_EPS_BASE ((uint32_t)-1)
uint32_t eps_base = UNKNOWN_EPS_BASE;

/* If found_vpd, replace the VPD partition when saveFile().
 * If not found, always create new file when saveFile(). */
bool found_vpd = false;

/* The VPD partition offset and size in buffer. The whole partition includes:
 *
 *   SMBIOS EPS
 *   SMBIOS tables[]
 *   VPD 2.0 data
 *
 */
uint32_t vpd_offset = 0, vpd_size; /* The whole partition */

int isbase64(uint8_t c) {
  return isalnum(c) || (c == '+') || (c == '/') || (c == '=');
}

std::optional<std::vector<uint8_t>> read_string_from_file(
    const char* file_name) {
  uint32_t i, j;

  auto file_buffer = base::ReadFileToBytes(base::FilePath(file_name));
  if (!file_buffer) {
    PLOG(ERROR) << "Failed to read file: " << file_name;
    return {};
  }

  /*
   * Are the contents a proper base64 blob? Verify it and drop EOL characters
   * along the way. This will help when displaying the contents.
   */
  for (i = 0, j = 0; i < file_buffer->size(); i++) {
    uint8_t c = (*file_buffer)[i];

    if ((c == 0xa) || (c == 0xd))
      continue; /* Skip EOL characters */

    if (!isbase64(c)) {
      fprintf(stderr, "[ERROR] file %s is not in base64 format (%c at %d)\n",
              file_name, c, i);
      return {};
    }
    (*file_buffer)[j++] = c;
  }
  (*file_buffer)[j] = '\0';

  return file_buffer;
}

/*
 * Check if given key name is compliant to recommended format.
 */
vpd_err_t checkKeyName(const uint8_t* name) {
  unsigned char c;
  while ((c = *name++)) {
    if (!(isalnum(c) || c == '_' || c == '.')) {
      fprintf(stderr, "[ERROR] VPD key name does not allow char [%c].\n", c);
      return VPD_ERR_PARAM;
    }
  }
  return VPD_OK;
}

/*
 * Check if key and value are compliant to recommended format.
 * For the checker of the key, see the function |checkKeyName|.
 * If key is "serial_number" or "mlb_serial_number", the value should only
 * contain characters a-z, A-Z, 0-9 or dash (-).
 * TODO(b/288327526): subsume into libvpd?
 */
vpd_err_t checkKeyValuePair(const uint8_t* key, const uint8_t* value) {
  int is_serial_number = 0;
  size_t value_len = 0;
  unsigned char c;

  vpd_err_t retval = checkKeyName(key);
  if (retval != VPD_OK)
    return retval;

  if (strncmp("serial_number", (const char*)key, sizeof("serial_number")) ==
          0 ||
      strncmp("mlb_serial_number", (const char*)key,
              sizeof("mlb_serial_number")) == 0)
    is_serial_number = 1;

  if (is_serial_number) {
    // For serial numbers, we only allow a-z, A-Z, 0-9 and dash (-).
    for (value_len = 0; (c = value[value_len]); value_len++) {
      if (!(isalnum(c) || c == '-')) {
        fprintf(stderr, "[ERROR] serial number does not allow char [%c].\n", c);
        return VPD_ERR_PARAM;
      }
    }

    if (value_len == 0) {
      fprintf(stderr, "[ERROR] serial number cannot be empty.\n");
      return VPD_ERR_PARAM;
    }

    if (value[0] == '-') {
      fprintf(stderr, "[ERROR] serial number cannot start with [-].\n");
      return VPD_ERR_PARAM;
    }

    if (value[value_len - 1] == '-') {
      fprintf(stderr, "[ERROR] serial number cannot end with [-].\n");
      return VPD_ERR_PARAM;
    }
  }

  return VPD_OK;
}

/*
 * Given a key=value string, this function parses it and adds to arugument
 * pair container. The 'value' can be stored in a base64 format file, in this
 * case the value field is the file name.
 */
vpd_err_t parseString(const uint8_t* string,
                      bool read_from_file,
                      std::map<std::string, std::string>* set_argument) {
  uint8_t* value;
  std::optional<std::vector<uint8_t>> file_contents;
  vpd_err_t retval = VPD_OK;

  uint8_t* key =
      reinterpret_cast<uint8_t*>(strdup(reinterpret_cast<const char*>(string)));
  if (!key || key[0] == '\0' || key[0] == '=') {
    if (key)
      free(key);
    return VPD_ERR_SYNTAX;
  }

  /*
   * Goes through the key string, and stops at the first '='.
   * If '=' is not found, the whole string is the key and
   * the value points to the end of string ('\0').
   */
  for (value = key; *value && *value != '='; value++) {
  }
  if (*value == '=') {
    *(value++) = '\0';

    if (read_from_file) {
      /* 'value' in fact is a file name */
      file_contents = read_string_from_file((const char*)value);
      if (!file_contents) {
        free(key);
        return VPD_ERR_SYNTAX;
      }
      value = file_contents->data();
    }
  }

  retval = checkKeyValuePair(key, value);
  if (retval == VPD_OK) {
    std::string k(reinterpret_cast<const char*>(key));
    std::string v(reinterpret_cast<const char*>(value));
    (*set_argument)[k] = v;
  }

  free(key);

  return retval;
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
vpd_err_t findVpdPartition(const std::vector<uint8_t>& read_buf,
                           const std::string& region_name,
                           uint32_t* vpd_offset,
                           uint32_t* vpd_size) {
  assert(vpd_offset);
  assert(vpd_size);

  /* scan the file and find out the VPD partition. */
  const off_t sig_offset = fmap_find(read_buf.data(), read_buf.size());
  if (sig_offset < 0) {
    return VPD_ERR_NOT_FOUND;
  }

  const struct fmap* fmap;
  if (sig_offset + sizeof(*fmap) > read_buf.size()) {
    LOG(ERROR) << "Bad FMAP at: " << sig_offset;
    return VPD_FAIL;
  }
  /* FMAP signature is found, try to search the partition name in table. */
  fmap = (const struct fmap*)(read_buf.data() + sig_offset);

  const struct fmap_area* area = fmap_find_area(fmap, region_name.c_str());
  if (!area) {
    LOG(ERROR) << "The VPD partition [" << region_name << "] is not found.";
    return VPD_ERR_NOT_FOUND;
  }
  *vpd_offset = area->offset;
  *vpd_size = area->size;
  /* Mark found here then saveFile() knows where to write back (vpd_offset,
   * vpd_size). */
  found_vpd = true;
  return VPD_OK;
}

vpd_err_t loadFile(const std::vector<uint8_t>& vpd_blob,
                   const std::string& region_name,
                   vpd::VpdRegion region,
                   vpd::Flashrom& flashrom,
                   int overwrite_it,
                   std::map<std::string, std::string>* kv_dict) {
  vpd_err_t retval = VPD_OK;

  if (0 == findVpdPartition(vpd_blob, region_name, &vpd_offset, &vpd_size)) {
    eps_base = vpd_offset;
  } else {
    /* We cannot parse out the VPD partition address from given file.
     * Then, try to read the whole BIOS chip. */
    uint32_t offset, size;
    if (!flashrom.GetPartitionDimensions(region, &offset, &size)) {
      if (overwrite_it) {
        retval = VPD_OK;
      } else {
        retval = VPD_FAIL;
        fprintf(stderr, "[ERROR] GetPartitionDimensions failed.");
      }
      return retval;
    }
    eps_base = offset;
    vpd_size = size;
  }

  /* Update the following variables:
   *   eps_base: integer, the VPD EPS address in ROM.
   *   vpd_offset: integer, the VPD partition offset in file (formerly
   *     read_buf[]).
   *   vpd_buf: uint8_t*, points to the VPD partition.
   */
  const uint8_t* vpd_buf = vpd_blob.data() + vpd_offset;

  if (eps_base == UNKNOWN_EPS_BASE) {
    fprintf(stderr,
            "[ERROR] Cannot determine eps_base. Cannot go on.\n"
            "        Ensure you have a valid FMAP.\n");
    return VPD_ERR_INVALID;
  }

  /* In overwrite mode, we don't care the content inside. Stop parsing. */
  if (overwrite_it) {
    return VPD_OK;
  }

  auto dict =
      vpd::Encoder::Decode(std::vector<uint8_t>(vpd_buf, vpd_buf + vpd_size));
  if (!dict) {
    return VPD_ERR_DECODE;
  }

  *kv_dict = std::move(*dict);

  return VPD_OK;
}

vpd_err_t saveFile(const std::vector<uint8_t>& blob, const char* filename) {
  FILE* fp;
  vpd_err_t retval = VPD_OK;
  uint32_t file_seek;

  if (found_vpd) {
    /* We found VPD partition in -f file, which means file is existed.
     * Instead of truncating the whole file, open to write partial. */
    if (!(fp = fopen(filename, "r+"))) {
      fprintf(stderr, "File [%s] cannot be opened for write.\n", filename);
      return VPD_ERR_SYSTEM;
    }
  } else {
    /* VPD is not found, which means the file is pure VPD data.
     * Always creates the new file and overwrites the original content. */
    if (!(fp = fopen(filename, "w+"))) {
      fprintf(stderr, "File [%s] cannot be opened for write.\n", filename);
      return VPD_ERR_SYSTEM;
    }
  }

  file_seek = vpd_offset;

  /* write entire blob */
  fseek(fp, file_seek, SEEK_SET);
  if (fwrite(blob.data(), blob.size(), 1, fp) != 1) {
    fprintf(stderr, "fwrite() error (%s)\n", strerror(errno));
    retval = VPD_ERR_SYSTEM;
    goto teardown;
  }

teardown:
  fclose(fp);

  return retval;
}

// We're just escaping single quotes within a single-quote string. i.e.,
// 'foo'bar' becomes 'foo'"'"'bar'.
std::string ShellSingleQuoteEscape(std::string s) {
  size_t pos = 0;
  std::string search("'");
  std::string replace("'\"'\"'");

  while ((pos = s.find(search, pos)) != std::string::npos) {
    s.replace(pos, search.size(), replace);
    pos += replace.size();
  }

  return s;
}

void usage(const char* progname) {
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
  printf("      -S <key=file>    To add/change a string value, reading its\n");
  printf("                       base64 contents from a file.\n");
  printf("      -s <key=value>   To add/change a string value.\n");
  printf("      -i <partition>   Specify VPD partition name in fmap.\n");
  printf("      -l               List content in the file.\n");
  printf("      --sh             Dump content for shell script.\n");
  printf("      --raw            Parse from a raw blob (without headers).\n");
  printf("      -0/--null-terminated\n");
  printf("                       Dump content in null terminate format.\n");
  printf("      -O               Overwrite and re-format VPD partition.\n");
  printf("      -g <key>         Print value string only.\n");
  printf("      -d <key>         Delete a key.\n");
  printf("\n");
  printf("   Notes:\n");
  printf("      You can specify multiple -s and -d. However, vpd always\n");
  printf("         applies -s first, then -d.\n");
  printf("      -g and -l must be mutually exclusive.\n");
  printf("\n");
}

}  // namespace

int main(int argc, char* argv[]) {
  // TODO(b/288327526): plumb libvpd's cache by default, with a new option to
  // force flash access.
  int opt;
  int option_index = 0;
  vpd_err_t retval = VPD_OK;
  int export_type = VPD_EXPORT_KEY_VALUE;
  const char* optstring = "hf:s:S:i:lOg:d:0";
  static struct option long_options[] = {
      {"help", 0, 0, 'h'},
      {"file", 0, 0, 'f'},
      {"string", 0, 0, 's'},
      {"base64file", 0, 0, 'S'},
      {"partition", 0, 0, 'i'},
      {"list", 0, 0, 'l'},
      {"overwrite", 0, 0, 'O'},
      {"filter", 0, 0, 'g'},
      {"sh", 0, &export_type, VPD_EXPORT_AS_PARAMETER},
      {"raw", 0, 0, 'R'},
      {"null-terminated", 0, 0, '0'},
      {"delete", 0, 0, 'd'},
      {0, 0, 0, 0}};
  vpd::VpdRegion region = vpd::VpdRo;
  std::string region_name = "RO_VPD";
  std::optional<std::string> filename;
  std::optional<std::string> save_file;
  std::optional<std::string> key_to_export;
  /* Stores parsed pairs from command "set" arguments. */
  std::map<std::string, std::string> set_argument;
  /* Stores parsed keys (and empty value) from command "delete"
   * arguments. */
  std::map<std::string, std::string> del_argument;
  /* Stores decoded pairs from VPD. */
  std::map<std::string, std::string> kv_dict;
  vpd::Flashrom flashrom;
  bool write_back_to_flash = false;
  bool list_it = false;
  bool overwrite_it = false;
  int modified = 0;
  bool read_from_file = false;
  bool raw_input = false;

  while ((opt = getopt_long(argc, argv, optstring, long_options,
                            &option_index)) != EOF) {
    switch (opt) {
      case 'h':
        usage(argv[0]);
        return retval;

      case 'f':
        filename = std::string(optarg);
        break;

      case 'S':
        read_from_file = true;
        /* Fall through into the next case */
      case 's':
        retval = parseString(reinterpret_cast<uint8_t*>(optarg), read_from_file,
                             &set_argument);
        if (VPD_OK != retval) {
          fprintf(stderr, "The string [%s] cannot be parsed.\n\n", optarg);
          return retval;
        }
        read_from_file = 0;
        break;

      case 'i':
        region_name = std::string(optarg);

        if (region_name == "RO_VPD") {
          region = vpd::VpdRo;
        } else if (region_name == "RW_VPD") {
          region = vpd::VpdRw;
        } else {
          LOG(ERROR) << "Invalid VPD partition name: " << region_name;
          return VPD_ERR_SYNTAX;
        }
        break;

      case 'l':
        list_it = true;
        break;

      case 'O':
        overwrite_it = true;
        /* This option forces to write empty data back even no new pair is
         * given. */
        modified = 1;
        break;

      case 'g':
        key_to_export = std::string(optarg);
        break;

      case 'd':
        /* Add key into container for delete. Since value is nonsense,
         * keep it empty. */
        del_argument[std::string(optarg)] = "";
        break;

      case '0':
        export_type = VPD_EXPORT_NULL_TERMINATE;
        break;

      case 'R':
        raw_input = true;
        break;

      case 0:
        break;

      default:
        fprintf(stderr, "Invalid option '%c' (%s), use --help for usage.\n",
                opt, optarg);
        return VPD_ERR_SYNTAX;
    }
  }

  if (optind < argc) {
    fprintf(stderr, "[ERROR] unexpected argument: %s\n\n", argv[optind]);
    usage(argv[0]);
    return VPD_ERR_SYNTAX;
  }

  if (list_it && key_to_export) {
    fprintf(stderr, "[ERROR] -l and -g must be mutually exclusive.\n");
    return VPD_ERR_SYNTAX;
  }

  if (VPD_EXPORT_KEY_VALUE != export_type && !list_it) {
    fprintf(stderr,
            "[ERROR] --sh/--null-terminated can be set only if -l is set.\n");
    return VPD_ERR_SYNTAX;
  }

  if (raw_input && !filename) {
    fprintf(stderr, "[ERROR] Needs -f FILE for raw input.\n");
    return VPD_ERR_SYNTAX;
  }

  if (raw_input && (!set_argument.empty() || !del_argument.empty())) {
    fprintf(stderr, "[ERROR] Changing in raw mode is not supported.\n");
    return VPD_ERR_SYNTAX;
  }

  std::vector<uint8_t> vpd_blob;
  /* if no filename is specified, call flashrom to read from flash. */
  if (!filename) {
    auto blob = flashrom.Read(region);
    if (!blob) {
      fprintf(stderr, "[ERROR] flashrom read error!\n");
      return VPD_ERR_ROM_READ;
    }
    vpd_blob = std::move(*blob);

    write_back_to_flash = true;
  } else {
    auto blob = base::ReadFileToBytes(base::FilePath(*filename));
    if (!blob) {
      return VPD_FAIL;
    }
    vpd_blob = std::move(*blob);
    save_file = filename;
  }

  if (raw_input) {
    auto res = vpd::Encoder::DecodeRaw(vpd_blob);
    if (!res) {
      return VPD_ERR_DECODE;
    }
    kv_dict = std::move(*res);
  } else {
    retval = loadFile(vpd_blob, region_name, region, flashrom, overwrite_it,
                      &kv_dict);
    if (VPD_OK != retval) {
      fprintf(stderr, "loadFile() error.\n");
      return retval;
    }
  }

  /* Do -s */
  if (!set_argument.empty()) {
    for (auto& pair : set_argument) {
      kv_dict[pair.first] = pair.second;
    }
    modified++;
  }

  /* Do -d */
  for (auto& pair : del_argument) {
    if (!kv_dict.erase(pair.first)) {
      fprintf(stderr,
              "[ERROR] At least one of the keys to delete"
              " does not exist. Command ignored.\n");
      return VPD_ERR_PARAM;
    }
  }
  if (!del_argument.empty()) {
    modified++;
  }

  /* Do -g */
  if (key_to_export) {
    const auto& pair = kv_dict.find(*key_to_export);
    if (pair == kv_dict.end()) {
      fprintf(stderr, "key lookup: Vpd data '%s' was not found.\n",
              key_to_export->c_str());
      return VPD_FAIL;
    } else {
      fwrite(pair->second.data(), strlen(pair->second.data()), 1, stdout);
    }
  }

  /* Do -l */
  if (list_it) {
    std::string list;

    for (const auto& pair : kv_dict) {
      switch (export_type) {
        case VPD_EXPORT_AS_PARAMETER:
          list += "    -s '" + ShellSingleQuoteEscape(pair.first) + "'='" +
                  ShellSingleQuoteEscape(pair.second) + "'\\\n";
          break;
        case VPD_EXPORT_KEY_VALUE:
          list += "\"" + pair.first + "\"=\"" + pair.second + "\"\n";
          break;
        case VPD_EXPORT_NULL_TERMINATE:
          list += pair.first + "=" + pair.second + std::string({'\0'});
          break;
        default:
          // Shouldn't be reached.
          assert(0);
      }
    }

    /* Export necessary program parameters */
    if (VPD_EXPORT_AS_PARAMETER == export_type) {
      printf("%s%s -i %s \\\n", SH_COMMENT, argv[0], region_name.c_str());

      if (filename)
        printf("    -f %s \\\n", filename->c_str());
    }

    fwrite(list.data(), list.size(), 1, stdout);
  }

  if (modified) {
    auto blob = vpd::Encoder::Encode(kv_dict);
    if (!blob) {
      fprintf(stderr, "Encode() error\n");
      return VPD_FAIL;
    }
    if (save_file) {
      retval = saveFile(*blob, save_file->c_str());
      if (VPD_OK != retval) {
        fprintf(stderr, "saveFile('%s') error: %d\n", filename->c_str(),
                retval);
        return retval;
      }
    }

    if (write_back_to_flash) {
      if (!flashrom.Write(region, vpd_blob)) {
        fprintf(stderr, "flashrom write error\n");
        return VPD_ERR_ROM_WRITE;
      }
    }
  }

  return VPD_OK;
}
