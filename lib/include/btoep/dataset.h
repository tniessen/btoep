#ifndef __BTOEP__DATASET_H__
#define __BTOEP__DATASET_H__

#include <stddef.h>

#ifdef _MSC_VER
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <tchar.h>
#else
# ifdef __linux__
#  include <linux/limits.h>
# else
#  include <limits.h>
# endif
#endif

#include "range.h"

#define BTOEP_INDEX_CACHE_SIZE 65536 // 64 KiB

#define B_ERR_IO                   1
#define B_ERR_DATASET_LOCKED       2
#define B_ERR_DESTRUCTIVE_ACTION   3
#define B_ERR_INVALID_INDEX_FORMAT 4
#define B_ERR_DATA_CONFLICT        5
#define B_ERR_READ_OUT_OF_BOUNDS   6

#ifdef _MSC_VER
typedef LPCTSTR btoep_path;
typedef TCHAR btoep_path_buffer[MAX_PATH];
typedef HANDLE btoep_fd;
#else
typedef const char* btoep_path;
typedef char btoep_path_buffer[PATH_MAX];
typedef int btoep_fd;
#endif

typedef struct {
  // Configurable paths.
  btoep_path_buffer data_path;
  btoep_path_buffer index_path;
  btoep_path_buffer lock_path;

  // File descriptors.
  btoep_fd data_fd;
  btoep_fd index_fd;

  // Error information.
  int last_error;
#ifdef _MSC_VER
  DWORD last_system_error;
#else
  int last_system_error;
#endif

  // Index file size and fd position within the index file.
  uint64_t current_index_offset;
  uint64_t total_index_size;
  uint64_t total_index_size_on_disk;

  // Position of index iterator.
  uint64_t iter_index_offset;
  uint64_t iter_data_offset;

  // Index cache.
  uint8_t index_cache[BTOEP_INDEX_CACHE_SIZE];
  btoep_range index_cache_range;
  bool index_cache_is_dirty;
  btoep_range index_cache_dirty_range;
} btoep_dataset;

/*
 * State management
 */

// TODO: Allow read-only open
bool btoep_open(btoep_dataset* dataset, btoep_path data_path, btoep_path index_path, btoep_path lock_path);

bool btoep_close(btoep_dataset* dataset);

void btoep_get_error(btoep_dataset* dataset, int* code, const char** message);

/*
 * Data API
 */

#define BTOEP_CONFLICT_ERROR     1
#define BTOEP_CONFLICT_KEEP_OLD  2
#define BTOEP_CONFLICT_OVERWRITE 4

bool btoep_data_add_range(btoep_dataset* dataset, btoep_range range, const void* data, int conflict_mode);

bool btoep_data_write(btoep_dataset* dataset, btoep_range range, const void* data, int conflict_mode);

/*
 * Reads a range of data. The given range must be a subset of an existing range.
 * In other words, this function does not permit reading outside of existing
 * ranges.
 *
 * data_size may be NULL, in which case range->length is used. If it is not
 * NULL, this function only reads up to data_size bytes of the given range,
 * and then sets data_size to the number of bytes read.
 *
 * Unlike btoep_data_read, this function will attempt to read the entire range
 * (or however many bytes fit into the buffer based on data_size).
 */
bool btoep_data_read_range(btoep_dataset* dataset, btoep_range range, void* data, size_t* data_size);

/*
 * This function is similar to the read() function. It attempts to read up to
 * length bytes starting at the given offset, but may return fewer bytes.
 *
 * If the given offset is beyond the end of the file, this function will fail.
 *
 * Depending on the underlying read() system call, this function may return less
 * data than length. However, it is guaranteed to return more than zero bytes,
 * unless either length is zero, or the offset is at the end of the file.
 *
 * Unlike btoep_data_read_range, this function allows reading outside of
 * existing data ranges.
 */
bool btoep_data_read(btoep_dataset* dataset, uint64_t offset, void* data, size_t* length);

bool btoep_data_get_size(btoep_dataset* dataset, uint64_t* size);

bool btoep_data_set_size(btoep_dataset* dataset, uint64_t size, bool allow_destructive);

/*
 * Index API
 */

bool btoep_index_iterator_start(btoep_dataset* dataset);

bool btoep_index_iterator_next(btoep_dataset* dataset, btoep_range* range);

bool btoep_index_iterator_peek(btoep_dataset* dataset, btoep_range* range);

bool btoep_index_iterator_skip(btoep_dataset* dataset);

bool btoep_index_iterator_is_eof(btoep_dataset* dataset);

bool btoep_index_add(btoep_dataset* dataset, btoep_range range);

bool btoep_index_remove(btoep_dataset* dataset, btoep_range range);

#define BTOEP_FIND_DATA    1
#define BTOEP_FIND_NO_DATA 2

bool btoep_index_find_offset(btoep_dataset* dataset, uint64_t start, int mode,
                             bool* exists, uint64_t* offset);

bool btoep_index_contains(btoep_dataset* dataset, btoep_range relevant_range, bool* contains);

bool btoep_index_contains_any(btoep_dataset* dataset, btoep_range relevant_range, bool* contains_any);

bool btoep_index_flush(btoep_dataset* dataset);

#endif  // __BTOEP__DATASET_H__
