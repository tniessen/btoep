#include <btoep/dataset.h>
#include <stdio.h>

#include "util/common.h"
#include "util/opt.h"

typedef struct {
  dataset_path_opts paths;
  bool force;
  maybe_uint64 size;
} cmd_opts;

int main(int argc, char** argv) {
  opt_def options[5] = {
    {
      .name = "--force",
      .accept = opt_accept_bool_flag_once,
      .out_offset = offsetof(cmd_opts, force)
    },
    {
      .name = "--size",
      .has_value = true,
      .accept = opt_accept_uint64_once,
      .out_offset = offsetof(cmd_opts, size)
    }
  };

  opt_add_nested(options + 2, dataset_path_opt_defs, 3, offsetof(cmd_opts, paths));

  cmd_opts opts = {
    .force = false
  };
  if (!opt_parse(options, 5, &opts, (size_t) argc - 1, argv + 1)) {
    fprintf(stderr, "error\n");
    return B_EXIT_CODE_USAGE_ERROR;
  }

  if (!opts.paths.data_path) {
    fprintf(stderr, "need data path\n");
    return B_EXIT_CODE_USAGE_ERROR;
  }

  if (!opts.size.exists) {
    fprintf(stderr, "need size\n");
    return B_EXIT_CODE_USAGE_ERROR;
  }

  btoep_dataset dataset;
  if (!btoep_open(&dataset, opts.paths.data_path, opts.paths.index_path, opts.paths.lock_path)) {
    print_error(&dataset);
    return B_EXIT_CODE_APP_ERROR;
  }

  if (!btoep_data_set_size(&dataset, opts.size.value, opts.force)) {
    perror("set_size");
  }

  if (!btoep_close(&dataset)) {
    perror("btoep_close");
    return B_EXIT_CODE_APP_ERROR;
  }

  return B_EXIT_CODE_SUCCESS;
}
