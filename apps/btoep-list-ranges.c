#include <assert.h>
#include <btoep/dataset.h>
#include <inttypes.h>
#include <stdio.h>

#include "util/common.h"
#include "util/opt.h"

typedef void (*print_range_fn)(btoep_range range);
typedef bool (*list_fn)(btoep_dataset* dataset, print_range_fn print_range);

static void print_range_excl(btoep_range range) {
  printf("%" PRIu64 "...%" PRIu64 "\n",
         range.offset, range.offset + range.length);
}

static void print_range_incl(btoep_range range) {
  printf("%" PRIu64 "..%" PRIu64 "\n",
         range.offset, range.offset + range.length - 1);
}

static bool list_data_ranges(btoep_dataset* dataset, print_range_fn print_range) {
  uint64_t total_size;
  if (!btoep_data_get_size(dataset, &total_size))
    return false;

  while (!btoep_index_iterator_is_eof(dataset)) {
    btoep_range range;
    if (!btoep_index_iterator_next(dataset, &range))
      return false;
    print_range(range);
  }

  return true;
}

static bool list_missing_ranges(btoep_dataset* dataset, print_range_fn print_range) {
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
      print_range(btoep_mkrange(prev_end_offset, range.offset - prev_end_offset));
    }

    prev_end_offset = range.offset + range.length;
  }

  // TODO: What if prev_end_offset > total_size ? Fail ?
  if (prev_end_offset != total_size) {
    print_range(btoep_mkrange(prev_end_offset, total_size - prev_end_offset));
  }

  return true;
}

typedef struct {
  dataset_path_opts paths;
  print_range_fn range_format;
  bool missing;
} cmd_opts;

static bool opt_accept_range_format(void* out, const char* value) {
  cmd_opts* result = out;
  if (result->range_format != NULL)
    return false;
  if (strcmp(value, "exclusive") == 0) {
    result->range_format = print_range_excl;
  } else if (strcmp(value, "inclusive") == 0) {
    result->range_format = print_range_incl;
  } else {
    return false;
  }

  return true;
}

int main(int argc, char** argv) {
  opt_def options[5] = {
    {
      .name = "--range-format",
      .accept = opt_accept_range_format
    },
    {
      .name = "--missing",
      .accept = opt_accept_bool_flag_once,
      .out_offset = offsetof(cmd_opts, missing)
    }
  };

  opt_add_nested(options + 2, dataset_path_opt_defs, 3, offsetof(cmd_opts, paths));

  cmd_opts opts = {
    .missing = false,
    .range_format = NULL
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
    perror("btoep_open");
    return B_EXIT_CODE_APP_ERROR;
  }

  list_fn list_ranges = opts.missing ? list_missing_ranges : list_data_ranges;
  print_range_fn print_range = opts.range_format;
  if (print_range == NULL)
    print_range = print_range_incl;

  if (!btoep_index_iterator_start(&dataset) ||
      !list_ranges(&dataset, print_range)) {
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
