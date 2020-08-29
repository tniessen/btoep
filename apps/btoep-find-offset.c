#include <assert.h>
#include <btoep/dataset.h>
#include <inttypes.h>
#include <stdio.h>

#include "util/common.h"
#include "util/opt.h"

typedef struct {
  dataset_path_opts paths;
  maybe_uint64 start_at_offset;
  bool has_mode;
  int mode;
} cmd_opts;

// TODO: Make this prettier with less returns
bool opt_accept_mode(void* out, const char* value) {
  cmd_opts* result = out;
  if (result->has_mode)
    return false;
  if (strcmp(value, "data") == 0) {
    result->has_mode = true;
    result->mode = BTOEP_FIND_DATA;
  } else if (strcmp(value, "no-data") == 0) {
    result->has_mode = true;
    result->mode = BTOEP_FIND_NO_DATA;
  } else {
    return false;
  }

  return true;
}

int main(int argc, char** argv) {
  opt_def options[5] = {
    {
      .name = "--start-at",
      .has_value = true,
      .accept = opt_accept_uint64_once,
      .out_offset = offsetof(cmd_opts, start_at_offset)
    },
    {
      .name = "--stop-at",
      .has_value = true,
      .accept = opt_accept_mode
    }
  };

  opt_add_nested(options + 2, dataset_path_opt_defs, 3, offsetof(cmd_opts, paths));

  cmd_opts opts = {
    .start_at_offset = {
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

  if (!opts.has_mode) {
    fprintf(stderr, "--stop-at is required\n");
    return B_EXIT_CODE_USAGE_ERROR;
  }

  btoep_dataset dataset;
  if (!btoep_open(&dataset, opts.paths.data_path, opts.paths.index_path,
                  opts.paths.lock_path)) {
    print_error(&dataset);
    return B_EXIT_CODE_APP_ERROR;
  }

  bool exists;
  uint64_t result;
  bool success = btoep_index_find_offset(&dataset, opts.start_at_offset.value,
                                         opts.mode, &exists, &result);

  // The order is important here. Even if the previous call failed, the dataset
  // should still be closed.
  success = btoep_close(&dataset) && success;

  if (!success) {
    print_error(&dataset);
    return B_EXIT_CODE_APP_ERROR;
  }

  if (exists) {
    printf("%" PRIu64 "\n", result);
    return B_EXIT_CODE_SUCCESS;
  } else {
    return B_EXIT_CODE_NO_RESULT;
  }
}
