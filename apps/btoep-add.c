#include <assert.h>
#include <btoep/dataset.h>
#include <stdio.h>
#include <string.h>

#include "util/common.h"
#include "util/opt.h"

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
  if (!opt_parse(options, 7, &opts, (size_t) argc - 1, argv + 1)) {
    fprintf(stderr, "error\n");
    return B_EXIT_CODE_USAGE_ERROR;
  }

  if (!opts.paths.data_path) {
    fprintf(stderr, "need data path\n");
    return B_EXIT_CODE_USAGE_ERROR;
  }

  if (!opts.offset.set_by_user) {
    fprintf(stderr, "need offset\n");
    return B_EXIT_CODE_USAGE_ERROR;
  }

  FILE* source = stdin;
  if (opts.source_path != NULL && strcmp(opts.source_path, "-") != 0) {
    source = fopen(opts.source_path, "rb");
    if (source == NULL) {
      perror("fopen");
      return 1;
    }
  }

  btoep_dataset dataset;
  if (!btoep_open(&dataset, opts.paths.data_path, opts.paths.index_path, opts.paths.lock_path)) {
    perror("btoep_open");
    return 1;
  }

  /*uint64_t max_length = (uint64_t) -1;
  if (opts.enforce_length.exists)
    max_length = opts.enforce_length.value;*/

  char buffer[64 * 1024];
  uint64_t offset = opts.offset.value, length = 0;
  while (!feof(source)) {
    size_t n_read = fread(buffer, 1, sizeof(buffer), source);
    if (n_read < sizeof(buffer) && ferror(source)) {
      perror("fread");
      btoep_close(&dataset); // TODO: Error handling
      return 1;
    }

    // We intentionally do not use btoep_data_add_range here to avoid modifying
    // the index until all data has been written successfully.
    if (!btoep_data_write(&dataset, offset + length, buffer, n_read, opts.on_conflict.value)) {
      // TODO
      const char* message;
      btoep_get_error(&dataset, NULL, &message);
      printf("error %s\n", message);
    }

    length += n_read;
  }

  fclose(source); // TODO: Check the return value

  if (length != 0) {
    btoep_range added_range = { offset, length };
    if (!btoep_index_add(&dataset, added_range)) {
      printf("error\n");
      // TODO
    }
  }

  if (!btoep_close(&dataset)) {
    print_error(&dataset);
    return B_EXIT_CODE_APP_ERROR;
  }

  return B_EXIT_CODE_SUCCESS;
}
