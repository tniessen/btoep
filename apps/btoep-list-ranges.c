#include <assert.h>
#include <btoep/dataset.h>
#include <inttypes.h>
#include <stdio.h>

#include "util/common.h"
#include "util/opt.h"

static inline void prange(uint64_t start, uint64_t end) {
  printf("%" PRIu64 "..%" PRIu64 "\n", start, end - 1);
}

static bool list_data_ranges(btoep_dataset* dataset) {
  uint64_t total_size;
  if (!btoep_data_get_size(dataset, &total_size))
    return false;

  while (!btoep_index_iterator_is_eof(dataset)) {
    btoep_range range;
    if (!btoep_index_iterator_next(dataset, &range))
      return false;
    prange(range.offset, range.offset + range.length);
  }

  return true;
}

static bool list_missing_ranges(btoep_dataset* dataset) {
  uint64_t total_size;
  if (!btoep_data_get_size(dataset, &total_size))
    return false;

  uint64_t prev_end_offset = 0;

  while (!btoep_index_iterator_is_eof(dataset)) {
    btoep_range range;
    if (!btoep_index_iterator_next(dataset, &range))
      return false;

    if (range.offset != 0) {
      assert(range.offset > prev_end_offset);
      prange(prev_end_offset, range.offset);
    }

    prev_end_offset = range.offset + range.length;
  }

  // TODO: What if prev_end_offset > total_size ? Fail ?
  if (prev_end_offset != total_size) {
    prange(prev_end_offset, total_size);
  }

  return true;
}

typedef struct {
  dataset_path_opts paths;
  bool missing;
} cmd_opts;

int main(int argc, char** argv) {
  opt_def options[4] = {
    {
      .name = "--missing",
      .accept = opt_accept_bool_flag_once,
      .out_offset = offsetof(cmd_opts, missing)
    }
  };

  opt_add_nested(options + 1, dataset_path_opt_defs, 3, offsetof(cmd_opts, paths));

  cmd_opts opts = {
    .missing = false
  };
  if (!opt_parse(options, 4, &opts, argc - 1, argv + 1)) {
    fprintf(stderr, "error\n");
    return B_EXIT_CODE_USAGE_ERROR;
  }

  if (!opts.paths.data_path) {
    fprintf(stderr, "need data path\n");
    return B_EXIT_CODE_USAGE_ERROR;
  }

  btoep_dataset dataset;
  if (!btoep_open(&dataset, opts.paths.data_path, opts.paths.index_path, opts.paths.lock_path)) {
    perror("btoep_open");
    return B_EXIT_CODE_APP_ERROR;
  }

  bool (*list_ranges)(btoep_dataset* dataset);
  list_ranges = opts.missing ? list_missing_ranges : list_data_ranges;

  if (!btoep_index_iterator_start(&dataset) ||
      !list_ranges(&dataset)) {
    const char* message;
    btoep_get_error(&dataset, NULL, &message);
    fprintf(stderr, "Error: %s\n", message);
  }

  if (!btoep_close(&dataset)) {
    perror("btoep_close");
    return B_EXIT_CODE_APP_ERROR;
  }

  return B_EXIT_CODE_SUCCESS;
}
