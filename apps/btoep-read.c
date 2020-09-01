#include <assert.h>
#include <btoep/dataset.h>
#include <stdio.h>

#ifdef _MSC_VER
# include <io.h>
# include <fcntl.h>
#endif

#include "util/common.h"
#include "util/res.h"

typedef struct {
  dataset_path_opts paths;
  optional_uint64 offset;
  optional_uint64 length;
  optional_uint64 limit;
} cmd_opts;

int main(int argc, char** argv) {
  opt_def options[6] = {
    UINT64_OPTION("--offset", offset),
    UINT64_OPTION("--length", length),
    UINT64_OPTION("--limit", limit)
  };

  opt_add_nested(options + 3, dataset_path_opt_defs, 3, offsetof(cmd_opts, paths));

  cmd_opts opts = {
    .offset = {
      .value = 0
    }
  };
  parse_cmd_opts(options, 6, &opts, (size_t) argc - 1, argv + 1,
                 read_usage_string, "btoep-read");

  if (!opts.paths.data_path) {
    fprintf(stderr, "Error: The --dataset option is required.\n");
    return offer_more_info("btoep-read");
  }

  btoep_dataset dataset;
  if (!btoep_open(&dataset, opts.paths.data_path, opts.paths.index_path,
                  opts.paths.lock_path, B_OPEN_EXISTING_READ_ONLY)) {
    print_error(&dataset);
    return B_EXIT_CODE_APP_ERROR;
  }

  bool success = true;
  btoep_range range;
  if (opts.length.set_by_user) {
    range = btoep_mkrange(opts.offset.value, opts.length.value);
  } else {
    bool exists;
    uint64_t end_offset;
    success = btoep_index_find_offset(&dataset, opts.offset.value, BTOEP_FIND_NO_DATA, &exists, &end_offset);
    if (success) {
      assert(exists && end_offset >= opts.offset.value);
      range = btoep_mkrange(opts.offset.value, end_offset - opts.offset.value);
    }
  }

  if (opts.limit.set_by_user && range.length > opts.limit.value)
    range.length = opts.limit.value;

#ifdef _MSC_VER
  // Prevent Windows from replacing '\n' with '\r\n' when calling fwrite.
  _setmode(fileno(stdout), _O_BINARY);
#endif

  if (success) {
    char buffer[64 * 1024];
    while (range.length > 0) {
      size_t size = sizeof(buffer);
      if (!btoep_data_read_range(&dataset, range, buffer, &size)) {
        success = false;
        break;
      }
      range = btoep_range_remove_left(range, size);
      size_t written = fwrite(buffer, 1, size, stdout);
      if (written != size) {
        perror("fwrite");
        break;
      }
    }
  }

  // The order is important here. Even if the previous call failed, the dataset
  // should still be closed.
  success = btoep_close(&dataset) && success;

  if (!success) {
    print_error(&dataset);
    return B_EXIT_CODE_APP_ERROR;
  }

  return B_EXIT_CODE_SUCCESS;
}
