#include <btoep/dataset.h>
#include <stdio.h>
#include <string.h>

#define B_EXIT_CODE_SUCCESS      0
#define B_EXIT_CODE_NO_RESULT    1
#define B_EXIT_CODE_USAGE_ERROR  2
#define B_EXIT_CODE_APP_ERROR    3

static inline void print_system_error(int error_code) {
  const char* message = strerror(error_code);
  fprintf(stderr, "%s (code %d)\n", message, error_code);
}

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
