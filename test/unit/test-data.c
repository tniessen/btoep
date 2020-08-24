#include "test.h"

#include <btoep/dataset.h>
#include <string.h>

static inline btoep_range mkrange(uint64_t offset, uint64_t length) {
  btoep_range range = { offset, length };
  return range;
}

static inline bool memeqb(const uint8_t* ptr, uint8_t value, size_t n) {
  for (size_t i = 0; i < n; i++)
    if (ptr[i] != value) return false;
  return true;
}

static void test_data(void) {
  btoep_dataset dataset;
  btoep_range range;
  uint8_t buffer[64 * 1024];
  uint64_t data_size;
  int error;

  // Create the (empty) dataset.
  assert(btoep_open(&dataset, "test_data", NULL, NULL));

  // Make sure the index is empty.
  assert(btoep_index_iterator_start(&dataset));
  assert(btoep_index_iterator_is_eof(&dataset));

  // Add some data.
  range = mkrange(7168, 1024);
  memset(buffer, 0xcc, range.length);
  assert(btoep_data_add_range(&dataset, &range, buffer, BTOEP_CONFLICT_ERROR));

  // The file should have been created with the proper size.
  assert(btoep_data_get_size(&dataset, &data_size));
  assert(data_size == 8192);

  // Make sure the range was created correctly.
  assert(btoep_index_iterator_start(&dataset));
  assert(!btoep_index_iterator_is_eof(&dataset));
  assert(btoep_index_iterator_next(&dataset, &range));
  assert(range.offset == 7168 && range.length == 1024);
  assert(btoep_index_iterator_is_eof(&dataset));

  // Add another range, and iterate again.
  range = mkrange(1024, 512);
  memset(buffer, 0xff, range.length);
  assert(btoep_data_add_range(&dataset, &range, buffer, BTOEP_CONFLICT_ERROR));
  assert(btoep_index_iterator_start(&dataset));
  assert(!btoep_index_iterator_is_eof(&dataset));
  assert(btoep_index_iterator_next(&dataset, &range));
  assert(range.offset == 1024 && range.length == 512);
  assert(!btoep_index_iterator_is_eof(&dataset));
  assert(btoep_index_iterator_next(&dataset, &range));
  assert(range.offset == 7168 && range.length == 1024);
  assert(btoep_index_iterator_is_eof(&dataset));

  // Add another range inbetween the previous ones.
  range = mkrange(1536, 5632);
  memset(buffer, 0xdd, range.length);
  assert(btoep_data_add_range(&dataset, &range, buffer, BTOEP_CONFLICT_ERROR));
  assert(btoep_index_iterator_start(&dataset));
  assert(!btoep_index_iterator_is_eof(&dataset));
  assert(btoep_index_iterator_next(&dataset, &range));
  assert(range.offset == 1024 && range.length == 7168);
  assert(btoep_index_iterator_is_eof(&dataset));

  // The file should still have the same size.
  assert(btoep_data_get_size(&dataset, &data_size));
  assert(data_size == 8192);

  // Attempting to shrink it should fail.
  assert(!btoep_data_set_size(&dataset, 8191, false));
  btoep_get_error(&dataset, &error, NULL);
  assert(error == B_ERR_DESTRUCTIVE_ACTION);

  // But enlarging it should work.
  assert(btoep_data_set_size(&dataset, 16384, false));
  assert(btoep_data_get_size(&dataset, &data_size));
  assert(data_size == 16384);

  // Add another range, and iterate again.
  range = mkrange(9216, 1024);
  memset(buffer, 0xaa, range.length);
  assert(btoep_data_add_range(&dataset, &range, buffer, BTOEP_CONFLICT_ERROR));
  assert(btoep_index_iterator_start(&dataset));
  assert(!btoep_index_iterator_is_eof(&dataset));
  assert(btoep_index_iterator_next(&dataset, &range));
  assert(range.offset == 1024 && range.length == 7168);
  assert(!btoep_index_iterator_is_eof(&dataset));
  assert(btoep_index_iterator_next(&dataset, &range));
  assert(range.offset == 9216 && range.length == 1024);
  assert(btoep_index_iterator_is_eof(&dataset));

  // There is some empty space at the end, so shrinking should work.
  assert(btoep_data_set_size(&dataset, 10240, false));
  assert(btoep_data_get_size(&dataset, &data_size));
  assert(data_size == 10240);

  // Shrinking further via a destructive action should also work.
  assert(!btoep_data_set_size(&dataset, 9728, false));
  btoep_get_error(&dataset, &error, NULL);
  assert(error == B_ERR_DESTRUCTIVE_ACTION);
  assert(btoep_data_set_size(&dataset, 9728, true));
  assert(btoep_data_get_size(&dataset, &data_size));
  assert(data_size == 9728);

  // Destructive shrinking should automatically update the index.
  assert(btoep_index_iterator_start(&dataset));
  assert(!btoep_index_iterator_is_eof(&dataset));
  assert(btoep_index_iterator_next(&dataset, &range));
  assert(range.offset == 1024 && range.length == 7168);
  assert(!btoep_index_iterator_is_eof(&dataset));
  assert(btoep_index_iterator_next(&dataset, &range));
  assert(range.offset == 9216 && range.length == 512);
  assert(btoep_index_iterator_is_eof(&dataset));

  // Adding an overlapping range with conflicting data should fail.
  range = mkrange(1000, 3000);
  memset(buffer, 0x01, range.length);
  assert(!btoep_data_add_range(&dataset, &range, buffer, BTOEP_CONFLICT_ERROR));
  btoep_get_error(&dataset, &error, NULL);
  assert(error == B_ERR_DATA_CONFLICT);

  // This should not have updated the index.
  assert(btoep_index_iterator_start(&dataset));
  assert(!btoep_index_iterator_is_eof(&dataset));
  assert(btoep_index_iterator_next(&dataset, &range));
  assert(range.offset == 1024 && range.length == 7168);
  assert(!btoep_index_iterator_is_eof(&dataset));
  assert(btoep_index_iterator_next(&dataset, &range));
  assert(range.offset == 9216 && range.length == 512);
  assert(btoep_index_iterator_is_eof(&dataset));

  // Summary:
  // The file now has a size of 9728 bytes.
  //    0..1023 do not exist.
  // 1024..1535 are set to 0xff.
  // 1536..7167 are set to 0xdd.
  // 7168..8191 are set to 0xcc.
  // 9216..9727 are set to 0xaa.

  // Existing ranges should contain correct data.
  range = mkrange(1024, 7168);
  size_t n_read = sizeof(buffer);
  assert(btoep_data_read_range(&dataset, &range, buffer, &n_read));
  assert(n_read == range.length);
  assert(memeqb(buffer, 0xff, 512));
  assert(memeqb(buffer + 512, 0xdd, 5632));
  assert(memeqb(buffer + 6144, 0xcc, 1024));
  range = mkrange(9216, 512);
  n_read = sizeof(buffer);
  assert(btoep_data_read_range(&dataset, &range, buffer, &n_read));
  assert(n_read == range.length);
  assert(memeqb(buffer, 0xaa, 512));

  // Reading out of bounds should fail.
  range = mkrange(0, 2048);
  assert(!btoep_data_read_range(&dataset, &range, buffer, NULL));
  btoep_get_error(&dataset, &error, NULL);
  assert(error == B_ERR_READ_OUT_OF_BOUNDS);

  // Reading an empty range should succeed as long as the offset is within the
  // file.
  range = mkrange(0, 0);
  n_read = sizeof(buffer);
  assert(btoep_data_read_range(&dataset, &range, buffer, &n_read));
  assert(n_read == 0);
  range = mkrange(9728, 0);
  n_read = sizeof(buffer);
  assert(btoep_data_read_range(&dataset, &range, buffer, &n_read));
  assert(n_read == 0);

  // But reading an empty range outside of the file should still fail.
  range = mkrange(9729, 0);
  assert(!btoep_data_read_range(&dataset, &range, buffer, NULL));
  btoep_get_error(&dataset, &error, NULL);
  assert(error == B_ERR_READ_OUT_OF_BOUNDS);

    //    0..1023 do not exist.
  // 1024..1535 are set to 0xff.
  // 1536..7167 are set to 0xdd.
  // 7168..8191 are set to 0xcc.
  // 9216..9727 are set to 0xaa.

  // Write a non-conflicting superset.
  range = mkrange(0, 10240);
  memset(buffer, 0xee, 1024);
  memset(buffer + 1024, 0xff, 512);
  memset(buffer + 1536, 0xdd, 5632);
  memset(buffer + 7168, 0xcc, 1024);
  memset(buffer + 8192, 0xbb, 1024);
  memset(buffer + 9216, 0xaa, 512);
  memset(buffer + 9728, 0x0f, 512);
  assert(btoep_data_add_range(&dataset, &range, buffer, BTOEP_CONFLICT_ERROR));

  // Make sure that the index was updated correctly.
  assert(btoep_index_iterator_start(&dataset));
  assert(!btoep_index_iterator_is_eof(&dataset));
  assert(btoep_index_iterator_next(&dataset, &range));
  assert(range.offset == 0 && range.length == 10240);
  assert(btoep_index_iterator_is_eof(&dataset));

  assert(btoep_close(&dataset));
}

TEST_MAIN(test_data)
