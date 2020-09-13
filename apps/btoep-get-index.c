#include <assert.h>
#include <btoep/dataset.h>
#include <inttypes.h>
#include <stdio.h>

#ifdef _MSC_VER
# include <io.h>
# include <fcntl.h>
#endif

#include "util/common.h"

static void write_uleb128(uint64_t value) {
  do {
    uint8_t b = (value & 0x7f) | (value > 0x7f ? 0x80 : 0);
    value >>= 7;
    fputc(b, stdout); // TODO: Return value
  } while (value != 0);
}

static uint64_t write_range(btoep_range range, uint64_t prev_end) {
  bool is_first = (prev_end == 0);
  assert(range.length > 0 && (range.offset > 0 || is_first));
  uint64_t relative_offset = (range.offset - prev_end) - !is_first;
  write_uleb128(relative_offset);
  write_uleb128(range.length - 1);
  return range.offset + range.length;
}

static bool print_index(btoep_dataset* dataset, uint64_t min_length) {
  btoep_index_iterator iterator;
  if (!btoep_index_iterator_start(dataset, &iterator))
    return false;

  uint64_t prev_end = 0;

  while (!btoep_index_iterator_is_eof(&iterator)) {
    btoep_range range;
    if (!btoep_index_iterator_next(&iterator, &range))
      return false;
    if (range.length >= min_length)
      prev_end = write_range(range, prev_end);
  }

  return true;
}

typedef struct {
  dataset_path_opts paths;
  optional_uint64 min_range_length;
} cmd_opts;

int main(int argc, char** argv) {
  opt_def options[4] = {
    UINT64_OPTION("--min-range-length", min_range_length)
  };

  opt_add_nested(options + 1, dataset_path_opt_defs, 3, offsetof(cmd_opts, paths));

  cmd_opts opts = {
    .min_range_length = {
      .value = 0
    }
  };
  parse_cmd_opts(options, 4, &opts, (size_t) argc - 1, argv + 1,
                 get_index_usage_string, "btoep-get-index");

  if (!opts.paths.data_path) {
    fprintf(stderr, "Error: The --dataset option is required.\n");
    return offer_more_info("btoep-get-index");
  }

  btoep_dataset dataset;
  if (!btoep_open(&dataset, opts.paths.data_path, opts.paths.index_path,
                  opts.paths.lock_path, B_OPEN_EXISTING_READ_ONLY)) {
    print_lib_error(&dataset);
    return B_EXIT_CODE_APP_ERROR;
  }

#ifdef _MSC_VER
  // Prevent Windows from replacing '\n' with '\r\n' when calling fputc.
  _setmode(fileno(stdout), _O_BINARY);
#endif

  bool success = print_index(&dataset, opts.min_range_length.value);

  success = btoep_close(&dataset) && success;

  if (!success) {
    print_lib_error(&dataset);
    return B_EXIT_CODE_APP_ERROR;
  }

  return B_EXIT_CODE_SUCCESS;
}
