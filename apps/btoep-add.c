#include <assert.h>
#include <btoep/dataset.h>
#include <stdio.h>
#include <string.h>

#include "util/common.h"

typedef struct {
  dataset_path_opts paths;
  const char* source_path;
  optional_int on_conflict;
  optional_uint64 offset;
  optional_uint64 enforce_length;
} cmd_opts;

#define ON_CONFLICT_ENUM(CASE)                                                 \
  CASE("error",     BTOEP_CONFLICT_ERROR)                                      \
  CASE("keep",      BTOEP_CONFLICT_KEEP_OLD)                                   \
  CASE("overwrite", BTOEP_CONFLICT_OVERWRITE)                                  \

static bool OPT_ACCEPT_ENUM_ONCE(on_conflict, optional_int, ON_CONFLICT_ENUM)

int main(int argc, char** argv) {
  opt_def options[7] = {
    CUSTOM_OPTION("--on-conflict", opt_accept_on_conflict),
    UINT64_OPTION("--offset", offset),
    UINT64_OPTION("--enforce-length", enforce_length),
    STRING_OPTION("--source", source_path)
  };

  opt_add_nested(options + 4, dataset_path_opt_defs, 3, offsetof(cmd_opts, paths));

  cmd_opts opts = {
    .on_conflict = {
      .value = BTOEP_CONFLICT_ERROR
    }
  };
  parse_cmd_opts(options, 7, &opts, (size_t) argc - 1, argv + 1,
                 add_usage_string, "btoep-add");

  if (!opts.paths.data_path) {
    fprintf(stderr, "Error: The --dataset option is required.\n");
    return offer_more_info("btoep-add");
  }

  if (!opts.offset.set_by_user) {
    fprintf(stderr, "Error: The --offset option is required.\n");
    return offer_more_info("btoep-add");
  }

  FILE* source = stdin;
  if (opts.source_path != NULL && strcmp(opts.source_path, "-") != 0) {
    source = fopen(opts.source_path, "rb");
    if (source == NULL) {
      print_stdlib_error(errno, "fopen");
      return B_EXIT_CODE_APP_ERROR;
    }
  }

  btoep_dataset dataset;
  if (!btoep_open(&dataset, opts.paths.data_path, opts.paths.index_path,
                  opts.paths.lock_path, B_OPEN_OR_CREATE_READ_WRITE)) {
    print_lib_error(&dataset);
    return B_EXIT_CODE_APP_ERROR;
  }

  /*uint64_t max_length = (uint64_t) -1;
  if (opts.enforce_length.exists)
    max_length = opts.enforce_length.value;*/

  bool btoep_ok = true, source_ok = true;

  char buffer[64 * 1024];
  btoep_range added_range = btoep_mkrange(opts.offset.value, 0);
  while (!feof(source)) {
    size_t n_read = fread(buffer, 1, sizeof(buffer), source);
    if (n_read < sizeof(buffer) && ferror(source)) {
      print_stdlib_error(errno, "fread");
      source_ok = false;
      break;
    }

    // We intentionally do not use btoep_data_add_range here to avoid modifying
    // the index until all data has been written successfully.
    btoep_range new_range = btoep_mkrange(added_range.offset + added_range.length, n_read);
    if (!btoep_data_write(&dataset, new_range, buffer, n_read, opts.on_conflict.value)) {
      btoep_ok = false;
      break;
    }

    added_range.length += n_read;
  }

  fclose(source); // TODO: Check the return value

  if (added_range.length != 0 && btoep_ok && source_ok) {
    if (!btoep_index_add(&dataset, added_range))
      btoep_ok = false;
  }

  btoep_ok = btoep_close(&dataset) && btoep_ok;
  if (!btoep_ok)
    print_lib_error(&dataset);

  return (btoep_ok && source_ok) ? B_EXIT_CODE_SUCCESS : B_EXIT_CODE_APP_ERROR;
}
