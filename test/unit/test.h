#ifdef NDEBUG
# error Assertions must be enabled during unit tests.
#endif

#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#define TEST_TIMEOUT 10

#define TEST_MAIN(fn)                                                          \
  void on_test_timeout(__attribute__((unused)) int sig) {                      \
    abort();                                                                   \
  }                                                                            \
                                                                               \
  int main(void) {                                                             \
    signal(SIGALRM, on_test_timeout);                                          \
    alarm(TEST_TIMEOUT);                                                       \
    fn();                                                                      \
    return 0;                                                                  \
  }
