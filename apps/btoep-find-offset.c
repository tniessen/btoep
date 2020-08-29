#include <assert.h>
#include <btoep/dataset.h>
#include <inttypes.h>
#include <stdio.h>

#include "util/common.h"
#include "util/opt.h"

typedef struct {
  dataset_path_opts paths;
  optional_uint64 start_at_offset;
  optional_int mode;
} cmd_opts;

#define MODE_ENUM(CASE)                                                        \
  CASE("data",    BTOEP_FIND_DATA)                                             \
  CASE("no-data", BTOEP_FIND_NO_DATA)                                          \

static bool OPT_ACCEPT_ENUM_ONCE(mode, optional_int, MODE_ENUM)

int main(int argc, char** argv) {
  opt_def options[5] = {
    UINT64_OPTION("--start-at", start_at_offset),
    CUSTOM_OPTION("--stop-at", opt_accept_mode)
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

  if (!opts.mode.set_by_user) {
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
                                         opts.mode.value, &exists, &result);

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
