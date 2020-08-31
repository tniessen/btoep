#include <btoep/dataset.h>
#include <stdio.h>

#include "util/common.h"
#include "util/res.h"

typedef struct {
  dataset_path_opts paths;
  bool force;
  optional_uint64 size;
} cmd_opts;

int main(int argc, char** argv) {
  opt_def options[5] = {
    BOOL_FLAG("--force", force),
    UINT64_OPTION("--size", size)
  };

  opt_add_nested(options + 2, dataset_path_opt_defs, 3, offsetof(cmd_opts, paths));

  cmd_opts opts = {
    .force = false
  };
  parse_cmd_opts(options, 5, &opts, (size_t) argc - 1, argv + 1,
                 set_size_usage_string, "btoep-set-size");

  if (!opts.paths.data_path) {
    fprintf(stderr, "Error: The --dataset option is required.\n");
    return offer_more_info("btoep-set-size");
  }

  if (!opts.size.set_by_user) {
    fprintf(stderr, "Error: The --size option is required.\n");
    return offer_more_info("btoep-set-size");
  }

  btoep_dataset dataset;
  if (!btoep_open(&dataset, opts.paths.data_path, opts.paths.index_path, opts.paths.lock_path)) {
    print_error(&dataset);
    return B_EXIT_CODE_APP_ERROR;
  }

  bool success = btoep_data_set_size(&dataset, opts.size.value, opts.force);

  // The order is important here. Even if the previous call failed, the dataset
  // should still be closed.
  success = btoep_close(&dataset) && success;

  if (!success) {
    print_error(&dataset);
    return B_EXIT_CODE_APP_ERROR;
  }

  return B_EXIT_CODE_SUCCESS;
}
