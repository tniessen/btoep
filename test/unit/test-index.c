#include "test.h"

#include <btoep/dataset.h>

static void assert_iterator_is_dead(btoep_index_iterator* iterator) {
  btoep_last_error_info error;

  assert(!btoep_index_iterator_skip(iterator));
  btoep_last_error(iterator->dataset, &error);
  assert(error.code == B_ERR_DEAD_INDEX_ITERATOR);
  btoep_index_iterator_start(iterator->dataset, iterator);
}

static void test_index(void) {
  btoep_dataset dataset;
  btoep_range range;
  btoep_index_iterator iterator, dead_iterator;
  bool b;

  // Create the (empty) dataset.
  assert(btoep_open(&dataset, "test_index", NULL, NULL,
                    B_CREATE_NEW_READ_WRITE));

  // Make sure the index is empty.
  assert(btoep_index_iterator_start(&dataset, &iterator));
  assert(btoep_index_iterator_is_eof(&iterator));

  // This iterator will be used to ensure that iterators become invalid ("dead")
  // after modifying the index.
  assert(btoep_index_iterator_start(&dataset, &dead_iterator));

  // Add a small range to the index.
  range = btoep_mkrange(512, 128);
  assert(btoep_index_add(&dataset, range));

  // Make sure the range was created correctly.
  assert(btoep_index_iterator_start(&dataset, &iterator));
  assert(!btoep_index_iterator_is_eof(&iterator));
  assert(btoep_index_iterator_next(&iterator, &range));
  assert(range.offset == 512 && range.length == 128);
  assert(btoep_index_iterator_is_eof(&iterator));

  // The old iterator should be invalid now.
  assert_iterator_is_dead(&dead_iterator);

  // Add another range, and iterate again.
  range = btoep_mkrange(1024, 512);
  assert(btoep_index_add(&dataset, range));
  assert(btoep_index_iterator_start(&dataset, &iterator));
  assert(!btoep_index_iterator_is_eof(&iterator));
  assert(btoep_index_iterator_next(&iterator, &range));
  assert(range.offset == 512 && range.length == 128);
  assert(!btoep_index_iterator_is_eof(&iterator));
  assert(btoep_index_iterator_next(&iterator, &range));
  assert(range.offset == 1024 && range.length == 512);
  assert(btoep_index_iterator_is_eof(&iterator));

  // The old iterator should be invalid now.
  assert_iterator_is_dead(&dead_iterator);

  // Add another range inbetween the previous ones.
  range = btoep_mkrange(640, 384);
  assert(btoep_index_add(&dataset, range));
  assert(btoep_index_iterator_start(&dataset, &iterator));
  assert(!btoep_index_iterator_is_eof(&iterator));
  assert(btoep_index_iterator_next(&iterator, &range));
  assert(range.offset == 512 && range.length == 1024);
  assert(btoep_index_iterator_is_eof(&iterator));

  // The old iterator should be invalid now.
  assert_iterator_is_dead(&dead_iterator);

  // Add an overlapping range.
  range = btoep_mkrange(256, 512);
  assert(btoep_index_add(&dataset, range));
  assert(btoep_index_iterator_start(&dataset, &iterator));
  assert(!btoep_index_iterator_is_eof(&iterator));
  assert(btoep_index_iterator_next(&iterator, &range));
  assert(range.offset == 256 && range.length == 1280);
  assert(btoep_index_iterator_is_eof(&iterator));

  // Add another overlapping range.
  range = btoep_mkrange(1024, 1024);
  assert(btoep_index_add(&dataset, range));
  assert(btoep_index_iterator_start(&dataset, &iterator));
  assert(!btoep_index_iterator_is_eof(&iterator));
  assert(btoep_index_iterator_next(&iterator, &range));
  assert(range.offset == 256 && range.length == 1792);
  assert(btoep_index_iterator_is_eof(&iterator));

  // Add a superset.
  range = btoep_mkrange(128, 4096);
  assert(btoep_index_add(&dataset, range));
  assert(btoep_index_iterator_start(&dataset, &iterator));
  assert(!btoep_index_iterator_is_eof(&iterator));
  assert(btoep_index_iterator_next(&iterator, &range));
  assert(range.offset == 128 && range.length == 4096);
  assert(btoep_index_iterator_is_eof(&iterator));

  // This does not change anything.
  range = btoep_mkrange(1024, 512);
  assert(btoep_index_add(&dataset, range));
  assert(btoep_index_iterator_start(&dataset, &iterator));
  assert(!btoep_index_iterator_is_eof(&iterator));
  assert(btoep_index_iterator_next(&iterator, &range));
  assert(range.offset == 128 && range.length == 4096);
  assert(btoep_index_iterator_is_eof(&iterator));

  // Test btoep_index_contains_any.
  range = btoep_mkrange(0, 128);
  assert(btoep_index_contains_any(&dataset, range, &b) && !b);
  range = btoep_mkrange(0, 129);
  assert(btoep_index_contains_any(&dataset, range, &b) && b);
  range = btoep_mkrange(600, 1);
  assert(btoep_index_contains_any(&dataset, range, &b) && b);
  range = btoep_mkrange(4224, 1);
  assert(btoep_index_contains_any(&dataset, range, &b) && !b);
  range = btoep_mkrange(4223, 1);
  assert(btoep_index_contains_any(&dataset, range, &b) && b);

  // The old iterator should be invalid now.
  assert_iterator_is_dead(&dead_iterator);

  // Split the existing range.
  range = btoep_mkrange(1024, 1024);
  assert(btoep_index_remove(&dataset, range));
  assert(btoep_index_iterator_start(&dataset, &iterator));
  assert(!btoep_index_iterator_is_eof(&iterator));
  assert(btoep_index_iterator_next(&iterator, &range));
  assert(range.offset == 128 && range.length == 896);
  assert(!btoep_index_iterator_is_eof(&iterator));
  assert(btoep_index_iterator_next(&iterator, &range));
  assert(range.offset == 2048 && range.length == 2176);
  assert(btoep_index_iterator_is_eof(&iterator));

  // The old iterator should be invalid now.
  assert_iterator_is_dead(&dead_iterator);

  // Split it again.
  range = btoep_mkrange(3000, 1);
  assert(btoep_index_remove(&dataset, range));
  assert(btoep_index_iterator_start(&dataset, &iterator));
  assert(!btoep_index_iterator_is_eof(&iterator));
  assert(btoep_index_iterator_next(&iterator, &range));
  assert(range.offset == 128 && range.length == 896);
  assert(!btoep_index_iterator_is_eof(&iterator));
  assert(btoep_index_iterator_next(&iterator, &range));
  assert(range.offset == 2048 && range.length == 952);
  assert(!btoep_index_iterator_is_eof(&iterator));
  assert(btoep_index_iterator_next(&iterator, &range));
  assert(range.offset == 3001 && range.length == 1223);
  assert(btoep_index_iterator_is_eof(&iterator));

  // The old iterator should be invalid now.
  assert_iterator_is_dead(&dead_iterator);

  // This should remove the range in the middle, and shrink the other two.
  range = btoep_mkrange(256, 3072);
  assert(btoep_index_remove(&dataset, range));
  assert(btoep_index_iterator_start(&dataset, &iterator));
  assert(!btoep_index_iterator_is_eof(&iterator));
  assert(btoep_index_iterator_next(&iterator, &range));
  assert(range.offset == 128 && range.length == 128);
  assert(!btoep_index_iterator_is_eof(&iterator));
  assert(btoep_index_iterator_next(&iterator, &range));
  assert(range.offset == 3328 && range.length == 896);
  assert(btoep_index_iterator_is_eof(&iterator));

  // The old iterator should be invalid now.
  assert_iterator_is_dead(&dead_iterator);

  // Removing the same range again should have no effect.
  range = btoep_mkrange(256, 3072);
  assert(btoep_index_remove(&dataset, range));
  assert(btoep_index_iterator_start(&dataset, &iterator));
  assert(!btoep_index_iterator_is_eof(&iterator));
  assert(btoep_index_iterator_next(&iterator, &range));
  assert(range.offset == 128 && range.length == 128);
  assert(!btoep_index_iterator_is_eof(&iterator));
  assert(btoep_index_iterator_next(&iterator, &range));
  assert(range.offset == 3328 && range.length == 896);
  assert(btoep_index_iterator_is_eof(&iterator));

  // The old iterator should be invalid, even if nothing changed.
  assert_iterator_is_dead(&dead_iterator);

  // This should remove everything.
  range = btoep_mkrange(128, 1000000);
  assert(btoep_index_remove(&dataset, range));
  assert(btoep_index_iterator_start(&dataset, &iterator));
  assert(btoep_index_iterator_is_eof(&iterator));

  // Removing the same range again should have no effect.
  assert(btoep_index_remove(&dataset, range));
  assert(btoep_index_iterator_start(&dataset, &iterator));
  assert(btoep_index_iterator_is_eof(&iterator));

  // The old iterator should be invalid now.
  assert_iterator_is_dead(&dead_iterator);

  assert(btoep_close(&dataset));
}

TEST_MAIN(test_index)
