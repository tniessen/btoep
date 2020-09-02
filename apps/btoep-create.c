#include <assert.h>
#include <btoep/dataset.h>
#include <stdio.h>
#include <string.h>

#include "util/common.h"

typedef struct {
  dataset_path_opts paths;
  optional_uint64 size;
} cmd_opts;

int main(int argc, char** argv) {
  opt_def options[4] = {
    UINT64_OPTION("--size", size)
  };

  opt_add_nested(options + 1, dataset_path_opt_defs, 3, offsetof(cmd_opts, paths));

  cmd_opts opts;
  memset(&opts, 0, sizeof(opts));
  parse_cmd_opts(options, 4, &opts, (size_t) argc - 1, argv + 1,
                 create_usage_string, "btoep-create");

  if (!opts.paths.data_path) {
    fprintf(stderr, "Error: The --dataset option is required.\n");
    return offer_more_info("btoep-create");
  }

  btoep_dataset dataset;
  bool success = btoep_open(&dataset, opts.paths.data_path,
                            opts.paths.index_path, opts.paths.lock_path,
                            B_CREATE_NEW_READ_WRITE);

  if (success) {
    if (opts.size.set_by_user)
      success = btoep_data_set_size(&dataset, opts.size.value, false);

    // The order is important here. Even if the previous call failed, the dataset
    // should still be closed.
    success = btoep_close(&dataset) && success;
  }

  if (!success) {
    print_lib_error(&dataset);
    return B_EXIT_CODE_APP_ERROR;
  }

  return B_EXIT_CODE_SUCCESS;
}
