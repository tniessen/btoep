#include <assert.h>
#include <btoep/dataset.h>
#include <inttypes.h>
#include <stdio.h>

#include "util/common.h"
#include "util/res.h"

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

typedef OPTIONAL_VALUE(print_range_fn) optional_print_range_fn;

typedef struct {
  dataset_path_opts paths;
  optional_print_range_fn range_format;
  bool missing;
} cmd_opts;

#define RANGE_FORMAT_ENUM(CASE)                                                \
  CASE("exclusive", print_range_excl)                                          \
  CASE("inclusive", print_range_incl)                                          \

static bool OPT_ACCEPT_ENUM_ONCE(range_format, optional_print_range_fn,
                                 RANGE_FORMAT_ENUM)

int main(int argc, char** argv) {
  opt_def options[5] = {
    CUSTOM_OPTION("--range-format", opt_accept_range_format),
    BOOL_FLAG("--missing", missing)
  };

  opt_add_nested(options + 2, dataset_path_opt_defs, 3, offsetof(cmd_opts, paths));

  cmd_opts opts = {
    .missing = false,
    .range_format = {
      .value = print_range_incl
    }
  };
  parse_cmd_opts(options, 5, &opts, (size_t) argc - 1, argv + 1,
                 list_ranges_usage_string, "btoep-list-ranges");

  if (!opts.paths.data_path) {
    fprintf(stderr, "Error: The --dataset option is required.\n");
    return offer_more_info("btoep-list-ranges");
  }

  btoep_dataset dataset;
  if (!btoep_open(&dataset, opts.paths.data_path, opts.paths.index_path, opts.paths.lock_path)) {
    perror("btoep_open");
    return B_EXIT_CODE_APP_ERROR;
  }

  list_fn list_ranges = opts.missing ? list_missing_ranges : list_data_ranges;

  if (!btoep_index_iterator_start(&dataset) ||
      !list_ranges(&dataset, opts.range_format.value)) {
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
