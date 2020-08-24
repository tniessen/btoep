#include <stdbool.h>
#include <stdint.h>

/* Generic option API */

typedef struct {
  const char* name;
  bool has_value;
  bool (*accept)(void* out, const char* value);
  size_t out_offset;
} opt_def;

void opt_add_nested(opt_def* out, const opt_def* nested_defs, size_t n_nested_defs, size_t out_offset);

bool opt_parse(const opt_def* defs, size_t n_defs, void* out, size_t argc, char * const * argv);

/* Utility functions */

bool opt_accept_string_once(void* out, const char* value);

typedef struct {
  bool exists;
  uint64_t value;
} maybe_uint64;

bool opt_accept_uint64_once(void* out, const char* value);

bool opt_accept_bool_flag_once(void* out, const char* value);

/* Specific options that are used across apps */

typedef struct {
  const char* data_path;
  const char* index_path;
  const char* lock_path;
} dataset_path_opts;

extern const opt_def* dataset_path_opt_defs;
