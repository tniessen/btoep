#ifdef NDEBUG
# error Assertions must be enabled during unit tests.
#endif

#include <assert.h>

#define TEST_MAIN(fn)                                                          \
  int main(void) {                                                             \
    fn();                                                                      \
    return 0;                                                                  \
  }
