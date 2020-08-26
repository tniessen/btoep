#include <assert.h>
#include <btoep/dataset.h>
#include <inttypes.h>
#include <stdio.h>

#include "util/common.h"
#include "util/opt.h"

typedef struct {
  dataset_path_opts paths;
  maybe_uint64 start_at_offset;
  bool stop_at_data;
  bool stop_at_missing;
} cmd_opts;

// TODO: Make this prettier with less returns
bool opt_accept_stop_at(void* out, const char* value) {
  cmd_opts* result = out;
  if (result->stop_at_data || result->stop_at_missing)
    return false;
  if (strcmp(value, "data") == 0) {
    result->stop_at_data = true;
  } else if (strcmp(value, "missing") == 0) {
    result->stop_at_missing = true;
  } else {
    return false;
  }

  return true;
}

bool maybe_result(uint64_t offset, bool looking_for_data,
                  btoep_range range, uint64_t* result) {
  if (range.offset > offset) {
    *result = looking_for_data ? range.offset : offset;
    return true;
  } else if (btoep_range_contains(range, offset)) {
    *result = looking_for_data ? offset : range.offset + range.length;
    return true;
  } else {
    return false;
  }
}

bool find_offset(btoep_dataset* dataset, const cmd_opts* opts,
                 bool* exists, uint64_t* result) {
  if (!btoep_index_iterator_start(dataset))
    return false;

  uint64_t offset = opts->start_at_offset.value;
  btoep_range range;

  while (!btoep_index_iterator_is_eof(dataset)) {
    if (!btoep_index_iterator_next(dataset, &range))
      return false;
    if (maybe_result(offset, opts->stop_at_data, range, result)) {
      *exists = true;
      return true;
    }
  }

  if ((*exists = !opts->stop_at_data))
    *result = opts->start_at_offset.value;

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
      .accept = opt_accept_stop_at
    }
  };

  opt_add_nested(options + 2, dataset_path_opt_defs, 3, offsetof(cmd_opts, paths));

  cmd_opts opts = {
    .start_at_offset = {
      .value = 0
    },
    .stop_at_data = false,
    .stop_at_missing = false
  };
  if (!opt_parse(options, 5, &opts, argc - 1, argv + 1)) {
    fprintf(stderr, "error\n");
    return B_EXIT_CODE_USAGE_ERROR;
  }

  if (!opts.paths.data_path) {
    fprintf(stderr, "need data path\n");
    return B_EXIT_CODE_USAGE_ERROR;
  }

  if (!opts.stop_at_data && !opts.stop_at_missing) {
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
  bool success = find_offset(&dataset, &opts, &exists, &result);

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
