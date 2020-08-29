#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "opt.h"

static bool find_def(const opt_def* defs, size_t n_defs, size_t* offset, const char* name, size_t name_len) {
  for (size_t i = *offset; i < n_defs; i++) {
    // TODO: Simplify this
    if (name == NULL && defs[i].name != NULL) continue;
    if (name != NULL && defs[i].name == NULL) continue;
    if (name != NULL && (strlen(defs[i].name) != name_len || strncmp(name, defs[i].name, name_len) != 0)) continue;
    *offset = i;
    return true;
  }
  return false;
}

void opt_add_nested(opt_def* out, const opt_def* nested_defs, size_t n_nested_defs, size_t out_offset) {
  for (size_t i = 0; i < n_nested_defs; i++) {
    out[i].name = nested_defs[i].name;
    out[i].has_value = nested_defs[i].has_value;
    out[i].accept = nested_defs[i].accept;
    out[i].out_offset = nested_defs[i].out_offset + out_offset;
  }
}

// TODO: Simplify this
bool opt_parse(const opt_def* defs, size_t n_defs, void *out, size_t argc, char * const * argv) {
  bool only_positional = false;
  size_t positional_offset = 0;

  for (size_t i = 0; i < argc; i++) {
    const char* value = NULL;
    const opt_def* def;

    if (*argv[i] == '-' && !only_positional) {
      if (strcmp(argv[i], "--") == 0) {
        only_positional = true;
        continue;
      }

      value = strchr(argv[i], '=');
      if (value) value++;

      size_t opt_index = 0;
      size_t name_len = value ? (size_t) (value - argv[i] - 1) : strlen(argv[i]);
      if (!find_def(defs, n_defs, &opt_index, argv[i], name_len))
        return false;
      def = defs + opt_index;

      if (value == NULL && defs[opt_index].has_value) {
        if (i + 1 >= argc)
          return false;
        value = argv[++i];
      }
    } else {
      if (!find_def(defs, n_defs, &positional_offset, NULL, 0))
        return false;
      def = defs + positional_offset;
      value = argv[i];
    }

    if (!def->accept(((uint8_t*) out) + def->out_offset, value))
      return false;
  }

  return true;
}

bool opt_accept_string_once(void* out, const char* value) {
  const char** str = out;
  if (*str != NULL)
    return false;
  *str = value;
  return true;
}

bool opt_accept_uint64_once(void* out, const char* value) {
  optional_uint64* result = out;
  if (result->set_by_user)
    return false;
  char* endptr;
  result->value = strtoull(value, &endptr, 10);
  result->set_by_user = true;
  return *endptr == 0;
}

bool opt_accept_bool_flag_once(void* out, const char* value) {
  (void) value;
  bool* b = out;
  return (!*b) && (*b = true);
}

opt_def _dataset_path_opt_defs[3] = {
  __STRING_OPTION(dataset_path_opts, "--dataset", data_path),
  __STRING_OPTION(dataset_path_opts, "--index-path", index_path),
  __STRING_OPTION(dataset_path_opts, "--lockfile-path", lock_path)
};

const opt_def* dataset_path_opt_defs = _dataset_path_opt_defs;
