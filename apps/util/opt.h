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

/* Utility functions and macros */

bool opt_accept_string_once(void* out, const char* value);

#define OPTIONAL_VALUE(type)                                                   \
  struct {                                                                     \
    bool set_by_user;                                                          \
    type value;                                                                \
  }                                                                            \

typedef OPTIONAL_VALUE(int)      optional_int;
typedef OPTIONAL_VALUE(uint64_t) optional_uint64;

bool opt_accept_uint64_once(void* out, const char* value);

bool opt_accept_bool_flag_once(void* out, const char* value);

#define __OPT_ACCEPT_ENUM_ONCE_CASE(strval, enumval)                           \
    if (strcmp(value, strval) == 0) {                                          \
      __opt_member->value = enumval;                                           \
      __opt_member->set_by_user = true;                                        \
      return true;                                                             \
    }                                                                          \

#define OPT_ACCEPT_ENUM_ONCE(name, type, G)                                    \
  opt_accept_##name(void* out, const char* value) {                            \
    type* __opt_member = &((cmd_opts*) out)->name;                             \
    if (__opt_member->set_by_user) return false;                               \
    G(__OPT_ACCEPT_ENUM_ONCE_CASE)                                             \
    return false;                                                              \
  }                                                                            \

#define __FLAG(stype, flag, member)                                            \
  {                                                                            \
    .name = flag,                                                              \
    .accept = opt_accept_bool_flag_once,                                       \
    .out_offset = offsetof(stype, member)                                      \
  }                                                                            \

#define __OPTION(stype, flag, member, accept_fn)                               \
  {                                                                            \
    .name = flag,                                                              \
    .has_value = true,                                                         \
    .accept = accept_fn,                                                       \
    .out_offset = offsetof(stype, member)                                      \
  }                                                                            \

#define __CUSTOM_OPTION(stype, flag, accept_fn)                                \
  {                                                                            \
    .name = flag,                                                              \
    .has_value = true,                                                         \
    .accept = accept_fn                                                        \
  }                                                                            \

#define CUSTOM_OPTION(flag, accept_fn)                                         \
  __CUSTOM_OPTION(cmd_opts, flag, accept_fn)
#define BOOL_FLAG(flag, member)                                                \
  __FLAG(cmd_opts, flag, member)
#define __STRING_OPTION(stype, flag, member)                                   \
  __OPTION(stype, flag, member, opt_accept_string_once)
#define STRING_OPTION(flag, member)                                            \
  __STRING_OPTION(cmd_opts, flag, member)
#define __UINT64_OPTION(stype, flag, member)                                   \
  __OPTION(stype, flag, member, opt_accept_uint64_once)
#define UINT64_OPTION(flag, member)                                            \
  __UINT64_OPTION(cmd_opts, flag, member)

/* Specific options that are used across apps */

typedef struct {
  const char* data_path;
  const char* index_path;
  const char* lock_path;
} dataset_path_opts;

extern const opt_def* dataset_path_opt_defs;
