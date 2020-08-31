#include <assert.h>
#include <btoep/dataset.h>
#include <inttypes.h>
#include <stdio.h>

#ifdef _MSC_VER
# include <io.h>
# include <fcntl.h>
#endif

#include "util/common.h"
#include "util/opt.h"

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
  if (!btoep_index_iterator_start(dataset))
    return false;

  uint64_t prev_end = 0;

  while (!btoep_index_iterator_is_eof(dataset)) {
    btoep_range range;
    if (!btoep_index_iterator_next(dataset, &range))
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
  if (!opt_parse(options, 4, &opts, (size_t) argc - 1, argv + 1)) {
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

#ifdef _MSC_VER
  // Prevent Windows from replacing '\n' with '\r\n' when calling fputc.
  _setmode(fileno(stdout), _O_BINARY);
#endif

  if (!btoep_index_iterator_start(&dataset) ||
      !print_index(&dataset, opts.min_range_length.value)) {
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