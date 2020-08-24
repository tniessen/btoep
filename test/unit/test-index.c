#include "test.h"

#include <btoep/dataset.h>

static inline btoep_range mkrange(uint64_t offset, uint64_t length) {
  btoep_range range = { offset, length };
  return range;
}

static void test_index(void) {
  btoep_dataset dataset;
  btoep_range range;
  bool b;

  // Create the (empty) dataset.
  assert(btoep_open(&dataset, "test_index", NULL, NULL));

  // Make sure the index is empty.
  assert(btoep_index_iterator_start(&dataset));
  assert(btoep_index_iterator_is_eof(&dataset));

  // Add a small range to the index.
  range = mkrange(512, 128);
  assert(btoep_index_add(&dataset, &range));

  // Make sure the range was created correctly.
  assert(btoep_index_iterator_start(&dataset));
  assert(!btoep_index_iterator_is_eof(&dataset));
  assert(btoep_index_iterator_next(&dataset, &range));
  assert(range.offset == 512 && range.length == 128);
  assert(btoep_index_iterator_is_eof(&dataset));

  // Add another range, and iterate again.
  range = mkrange(1024, 512);
  assert(btoep_index_add(&dataset, &range));
  assert(btoep_index_iterator_start(&dataset));
  assert(!btoep_index_iterator_is_eof(&dataset));
  assert(btoep_index_iterator_next(&dataset, &range));
  assert(range.offset == 512 && range.length == 128);
  assert(!btoep_index_iterator_is_eof(&dataset));
  assert(btoep_index_iterator_next(&dataset, &range));
  assert(range.offset == 1024 && range.length == 512);
  assert(btoep_index_iterator_is_eof(&dataset));

  // Add another range inbetween the previous ones.
  range = mkrange(640, 384);
  assert(btoep_index_add(&dataset, &range));
  assert(btoep_index_iterator_start(&dataset));
  assert(!btoep_index_iterator_is_eof(&dataset));
  assert(btoep_index_iterator_next(&dataset, &range));
  assert(range.offset == 512 && range.length == 1024);
  assert(btoep_index_iterator_is_eof(&dataset));

  // Add an overlapping range.
  range = mkrange(256, 512);
  assert(btoep_index_add(&dataset, &range));
  assert(btoep_index_iterator_start(&dataset));
  assert(!btoep_index_iterator_is_eof(&dataset));
  assert(btoep_index_iterator_next(&dataset, &range));
  assert(range.offset == 256 && range.length == 1280);
  assert(btoep_index_iterator_is_eof(&dataset));

  // Add another overlapping range.
  range = mkrange(1024, 1024);
  assert(btoep_index_add(&dataset, &range));
  assert(btoep_index_iterator_start(&dataset));
  assert(!btoep_index_iterator_is_eof(&dataset));
  assert(btoep_index_iterator_next(&dataset, &range));
  assert(range.offset == 256 && range.length == 1792);
  assert(btoep_index_iterator_is_eof(&dataset));

  // Add a superset.
  range = mkrange(128, 4096);
  assert(btoep_index_add(&dataset, &range));
  assert(btoep_index_iterator_start(&dataset));
  assert(!btoep_index_iterator_is_eof(&dataset));
  assert(btoep_index_iterator_next(&dataset, &range));
  assert(range.offset == 128 && range.length == 4096);
  assert(btoep_index_iterator_is_eof(&dataset));

  // This does not change anything.
  range = mkrange(1024, 512);
  assert(btoep_index_add(&dataset, &range));
  assert(btoep_index_iterator_start(&dataset));
  assert(!btoep_index_iterator_is_eof(&dataset));
  assert(btoep_index_iterator_next(&dataset, &range));
  assert(range.offset == 128 && range.length == 4096);
  assert(btoep_index_iterator_is_eof(&dataset));

  // Test btoep_index_contains_any.
  range = mkrange(0, 128);
  assert(btoep_index_contains_any(&dataset, &range, &b) && !b);
  range = mkrange(0, 129);
  assert(btoep_index_contains_any(&dataset, &range, &b) && b);
  range = mkrange(600, 1);
  assert(btoep_index_contains_any(&dataset, &range, &b) && b);
  range = mkrange(4224, 1);
  assert(btoep_index_contains_any(&dataset, &range, &b) && !b);
  range = mkrange(4223, 1);
  assert(btoep_index_contains_any(&dataset, &range, &b) && b);

  // Split the existing range.
  range = mkrange(1024, 1024);
  assert(btoep_index_remove(&dataset, &range));
  assert(btoep_index_iterator_start(&dataset));
  assert(!btoep_index_iterator_is_eof(&dataset));
  assert(btoep_index_iterator_next(&dataset, &range));
  assert(range.offset == 128 && range.length == 896);
  assert(!btoep_index_iterator_is_eof(&dataset));
  assert(btoep_index_iterator_next(&dataset, &range));
  assert(range.offset == 2048 && range.length == 2176);
  assert(btoep_index_iterator_is_eof(&dataset));

  // Split it again.
  range = mkrange(3000, 1);
  assert(btoep_index_remove(&dataset, &range));
  assert(btoep_index_iterator_start(&dataset));
  assert(!btoep_index_iterator_is_eof(&dataset));
  assert(btoep_index_iterator_next(&dataset, &range));
  assert(range.offset == 128 && range.length == 896);
  assert(!btoep_index_iterator_is_eof(&dataset));
  assert(btoep_index_iterator_next(&dataset, &range));
  assert(range.offset == 2048 && range.length == 952);
  assert(!btoep_index_iterator_is_eof(&dataset));
  assert(btoep_index_iterator_next(&dataset, &range));
  assert(range.offset == 3001 && range.length == 1223);
  assert(btoep_index_iterator_is_eof(&dataset));

  // This should remove the range in the middle, and shrink the other two.
  range = mkrange(256, 3072);
  assert(btoep_index_remove(&dataset, &range));
  assert(btoep_index_iterator_start(&dataset));
  assert(!btoep_index_iterator_is_eof(&dataset));
  assert(btoep_index_iterator_next(&dataset, &range));
  assert(range.offset == 128 && range.length == 128);
  assert(!btoep_index_iterator_is_eof(&dataset));
  assert(btoep_index_iterator_next(&dataset, &range));
  assert(range.offset == 3328 && range.length == 896);
  assert(btoep_index_iterator_is_eof(&dataset));

  // Removing the same range again should have no effect.
  range = mkrange(256, 3072);
  assert(btoep_index_remove(&dataset, &range));
  assert(btoep_index_iterator_start(&dataset));
  assert(!btoep_index_iterator_is_eof(&dataset));
  assert(btoep_index_iterator_next(&dataset, &range));
  assert(range.offset == 128 && range.length == 128);
  assert(!btoep_index_iterator_is_eof(&dataset));
  assert(btoep_index_iterator_next(&dataset, &range));
  assert(range.offset == 3328 && range.length == 896);
  assert(btoep_index_iterator_is_eof(&dataset));

  // This should remove everything.
  range = mkrange(128, 1000000);
  assert(btoep_index_remove(&dataset, &range));
  assert(btoep_index_iterator_start(&dataset));
  assert(btoep_index_iterator_is_eof(&dataset));

  // Removing the same range again should have no effect.
  assert(btoep_index_remove(&dataset, &range));
  assert(btoep_index_iterator_start(&dataset));
  assert(btoep_index_iterator_is_eof(&dataset));

  assert(btoep_close(&dataset));
}

TEST_MAIN(test_index)
