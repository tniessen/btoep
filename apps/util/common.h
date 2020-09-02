#include <btoep/dataset.h>
#include <btoep/version.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "opt.h"
#include "res.h"

#define BTOEP_TOOL_VERSION "0.1.0-pre"

#define B_EXIT_CODE_SUCCESS      0
#define B_EXIT_CODE_NO_RESULT    1
#define B_EXIT_CODE_USAGE_ERROR  2
#define B_EXIT_CODE_APP_ERROR    3

#ifdef _MSC_VER
static inline const char* system_strerror(DWORD error_code) {
  if (error_code == ERROR_SUCCESS)
    return NULL;

  static char message[1024];
  // TODO: Check return value
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, error_code, 0, message, 1024, NULL);
  // Remove trailing newline characters.
  message[strcspn(message, "\r\n\0")] = 0;
  return message;
}
#else
static inline const char* system_strerror(int error_code) {
  return (error_code == 0) ? NULL : strerror(error_code);
}
#endif

static inline void print_error_message_line(const char* msg, const char* ext_msg) {
  fprintf(stderr, "Error: %s", msg);
  if (ext_msg != NULL)
    fprintf(stderr, ": %s", ext_msg);
  fputs("\n\n", stderr);
}

static inline void print_system_error_details(const char* name, int code, const char* func) {
  if (name != NULL)
    fprintf(stderr, "System error name: %s\n", name);
  fprintf(stderr, "System error code: %d\n", code);
  fprintf(stderr, "System function: %s\n", func);
}

static inline void print_errno_details(int error_code, const char* func) {
  const char* name = get_errno_error_name(error_code);
  print_system_error_details(name, error_code, func);
}

static inline void print_stdlib_error(int error_code, const char* func) {
  print_error_message_line(strerror(error_code), NULL);
  print_errno_details(error_code, func);
}

static inline void print_lib_error(btoep_dataset* dataset) {
  btoep_last_error_info info;
  btoep_last_error(dataset, &info);

  const char* msg = btoep_strerror(info.code);
  const char* ext_msg = system_strerror(info.system_error_code);
  print_error_message_line(msg, ext_msg);

  fprintf(stderr, "Library error name: %s\n", btoep_strerror_name(info.code));
  fprintf(stderr, "Library error code: %d\n", info.code);

  if (info.system_error_code != 0) {
    const char* name;
#ifdef _MSC_VER
    name = get_windows_error_name(info.system_error_code);
#else
    name = get_errno_error_name(info.system_error_code);
#endif
    print_system_error_details(name, info.system_error_code, info.system_func);
  }
}

static inline int offer_more_info(const char* name) {
  fprintf(stderr, "Use '%s --help' for more information.\n", name);
  return B_EXIT_CODE_USAGE_ERROR;
}

static inline void parse_cmd_opts(const opt_def* defs, size_t n_defs, void* out,
                                  size_t argc, char* const* argv,
                                  const char* usage, const char* name) {
  if (argc == 1) {
    if (strcmp(argv[0], "--help") == 0) {
      printf("%s\n", usage);
      exit(B_EXIT_CODE_SUCCESS);
    } else if (strcmp(argv[0], "--version") == 0) {
      printf("%s %s (using btoep %d.%d.%d%s)\n", name, BTOEP_TOOL_VERSION,
             BTOEP_LIB_VERSION_MAJOR, BTOEP_LIB_VERSION_MINOR,
             BTOEP_LIB_VERSION_PATCH, BTOEP_LIB_VERSION_PRERELEASE);
      exit(B_EXIT_CODE_SUCCESS);
    }
  }

  size_t n = opt_parse(defs, n_defs, out, argc, argv);
  if (n != argc) {
    fprintf(stderr, "Error: Failed to understand argument '%s'\n", argv[n]);
    exit(offer_more_info(name));
  }
}
