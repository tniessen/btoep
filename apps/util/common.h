#include <btoep/dataset.h>
#include <btoep/version.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "opt.h"

#define BTOEP_TOOL_VERSION "0.1.0-pre"

#define B_EXIT_CODE_SUCCESS      0
#define B_EXIT_CODE_NO_RESULT    1
#define B_EXIT_CODE_USAGE_ERROR  2
#define B_EXIT_CODE_APP_ERROR    3

#ifdef _MSC_VER
static inline void print_system_error(DWORD error_code) {
  LPTSTR message;
  // TODO: Check return value
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_ALLOCATE_BUFFER |
                FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, error_code, 0, (LPTSTR) &message, 0, NULL);
  // Remove trailing newline characters.
  message[strcspn(message, "\r\n\0")] = 0;
  fprintf(stderr, "%s (code %d)\n", message, error_code);
}
#else
static inline void print_system_error(int error_code) {
  const char* message = strerror(error_code);
  fprintf(stderr, "%s (code %d)\n", message, error_code);
}
#endif

static inline void print_error(btoep_dataset* dataset) {
  int error;
  const char* message;
  btoep_get_error(dataset, &error, &message);

  if (error == B_ERR_IO) {
    fprintf(stderr, "Error: %s: ", message);
    // TODO: Improve the dataset API to make this prettier/easier
    print_system_error(dataset->last_system_error);
  } else {
    fprintf(stderr, "Error: %s\n", message);
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
