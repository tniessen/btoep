#include <assert.h>
#include <btoep/dataset.h>
#include <stdio.h>

#ifdef _MSC_VER
#include <io.h>
#include <fcntl.h>
#endif

#include "util/common.h"
#include "util/opt.h"

typedef struct {
  dataset_path_opts paths;
  maybe_uint64 offset;
  maybe_uint64 length;
} cmd_opts;

int main(int argc, char** argv) {
  opt_def options[5] = {
    {
      .name = "--offset",
      .has_value = true,
      .accept = opt_accept_uint64_once,
      .out_offset = offsetof(cmd_opts, offset)
    },
    {
      .name = "--length",
      .has_value = true,
      .accept = opt_accept_uint64_once,
      .out_offset = offsetof(cmd_opts, length)
    }
  };

  opt_add_nested(options + 2, dataset_path_opt_defs, 3, offsetof(cmd_opts, paths));

  cmd_opts opts = {
    .offset = {
      .value = 0
    }
  };
  if (!opt_parse(options, 5, &opts, (size_t) argc - 1, argv + 1)) {
    fprintf(stderr, "error\n");
    return B_EXIT_CODE_USAGE_ERROR;
  }

  if (!opts.paths.data_path) {
    fprintf(stderr, "need data path\n");
    return B_EXIT_CODE_USAGE_ERROR;
  }

  btoep_dataset dataset;
  if (!btoep_open(&dataset, opts.paths.data_path, opts.paths.index_path, opts.paths.lock_path)) {
    print_error(&dataset);
    return B_EXIT_CODE_APP_ERROR;
  }

  bool success = true;
  btoep_range range;
  if (opts.length.exists) {
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
