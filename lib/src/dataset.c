#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "../include/btoep/dataset.h"

static bool set_error(btoep_dataset* dataset, int error, bool system_error) {
  dataset->last_error = error;
  dataset->last_system_error = system_error ? errno : 0;
  return false;
}

static inline bool set_io_error(btoep_dataset* dataset) {
  return set_error(dataset, B_ERR_IO, true);
}

static bool fd_seek(btoep_dataset* dataset, int fd, off_t offset, int whence, uint64_t* out) {
  offset = lseek(fd, offset, whence);
  if (offset == (off_t) -1)
    return set_io_error(dataset);
  if (out != NULL)
    *out = offset;
  return true;
}

static bool fd_read(btoep_dataset* dataset, int fd, void* out, size_t* n_read) {
  ssize_t ret = read(fd, out, *n_read);
  if (ret == -1)
    return set_io_error(dataset);
  *n_read = ret;
  return true;
}

static bool fd_write(btoep_dataset* dataset, int fd, const void* data, size_t length) {
  const uint8_t* bytes = data;
  while (length > 0) {
    ssize_t written = write(fd, bytes, length);
    if (written == -1)
      return set_io_error(dataset);
    assert(written >= 0 && (size_t) written <= length);
    bytes += written;
    length -= written;
  }
  return true;
}

static bool copy_path(char* out, const char* in, const char* def, const char* ext) {
  if (in == NULL) {
    strcpy(out, def);
    strcat(out, ext);
  } else {
    strcpy(out, in);
  }
  return true; // TODO: Bound checking etc
}

static bool btoep_lock(btoep_dataset* dataset) {
  int fd = open(dataset->lock_path, O_CREAT | O_EXCL | O_WRONLY, 0000);
  if (fd == -1) {
    if (errno == EEXIST)
      return set_error(dataset, B_ERR_DATASET_LOCKED, false);
    return set_io_error(dataset);
  }
  close(fd); // TODO: Check return value
  return true;
}

static bool btoep_unlock(btoep_dataset* dataset) {
  return unlink(dataset->lock_path) == 0 || set_io_error(dataset);
}

// TODO: Allow read-only open
bool btoep_open(btoep_dataset* dataset, const char* data_path, const char* index_path, const char* lock_path) {
  strcpy(dataset->data_path, data_path);
  copy_path(dataset->index_path, index_path, data_path, ".idx");
  copy_path(dataset->lock_path, lock_path, data_path, ".lck");

  if (!btoep_lock(dataset))
    return false;

  dataset->data_fd = open(data_path, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
  if (dataset->data_fd == -1) {
    btoep_unlock(dataset); // TODO: Return value
    return false;
  }
  dataset->index_fd = open(dataset->index_path, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
  if (dataset->index_fd == -1) {
    close(dataset->data_fd); // TODO: Return value
    btoep_unlock(dataset); // TODO: Return value
    return set_io_error(dataset);
  }

  // Determine the index size.
  if (!fd_seek(dataset, dataset->index_fd, 0, SEEK_END, &dataset->total_index_size_on_disk) ||
      !fd_seek(dataset, dataset->index_fd, 0, SEEK_SET, NULL))
    return false;
  dataset->total_index_size = dataset->total_index_size_on_disk;
  dataset->current_index_offset = 0;

  dataset->index_cache_range = btoep_mkrange(0, 0);
  dataset->index_cache_is_dirty = false;

  return true;
}

bool btoep_close(btoep_dataset* dataset) {
  if (!btoep_index_flush(dataset))
    return false;

  // TODO: Return values
  close(dataset->data_fd);
  close(dataset->index_fd);
  btoep_unlock(dataset);
  return true;
}

const char* error_messages[] = {
  "Success",
  "System input/output error",
  "Dataset locked",
  "Destructive action",
  "Invalid index format",
  "Data conflict",
  "Read out of bounds"
};

void btoep_get_error(btoep_dataset* dataset, int* code, const char** message) {
  if (code != NULL)
    *code = dataset->last_error;

  if (message != NULL) {
    *message = error_messages[dataset->last_error];
  }
}

bool btoep_data_add_range(btoep_dataset* dataset, btoep_range range, const void* data, int conflict_mode) {
  return btoep_data_write(dataset, range.offset, data, range.length, conflict_mode) &&
         btoep_index_add(dataset, range);
}

bool btoep_data_write(btoep_dataset* dataset, uint64_t offset, const void* data, size_t length, int conflict_mode) {
  if (!btoep_index_iterator_start(dataset))
    return false;

  btoep_range entry;

  if (!fd_seek(dataset, dataset->data_fd, offset, SEEK_SET, NULL))
    return false;

  while (length != 0) {
    // Try to find an index entry that covers at least some area after the start of the remaining data.
    while (!btoep_index_iterator_is_eof(dataset)) {
      if (!btoep_index_iterator_peek(dataset, &entry))
        return false;
      if (btoep_range_intersect(&entry, btoep_mkrange(offset, length)))
        break;
      if (!btoep_index_iterator_skip(dataset))
        return false;
    }

    // If no entry exists, we can write the rest of the data.
    // If an entry exists, we can write up to the entry.
    uint64_t safe_length = length;
    if (!btoep_index_iterator_is_eof(dataset))
      safe_length = entry.offset - offset;

    if (!fd_write(dataset, dataset->data_fd, data, safe_length))
      return false;

    offset += safe_length;
    length -= safe_length;
    data = ((uint8_t*) data) + safe_length;

    if (!btoep_index_iterator_is_eof(dataset)) {
      // This is existing data.
      if (conflict_mode == BTOEP_CONFLICT_KEEP_OLD) {
        // Simply ignore the data and skip ahead.
        // TODO: Fail if the data does not exist because the file is too short?
        uint64_t fd_offset;
        if (!fd_seek(dataset, dataset->data_fd, entry.length, SEEK_CUR, &fd_offset))
          return false;
        offset += entry.length;
        length -= entry.length;
        data = ((uint8_t*) data) + entry.length;
        assert(offset == fd_offset);
      } else if (conflict_mode == BTOEP_CONFLICT_ERROR) {
        // Ensure the data is the same.
        uint8_t buf[64 * 1024];
        ssize_t n_read = read(dataset->data_fd, buf, entry.length); // TODO: Error handling, loop etc.
        if (memcmp(buf, data, n_read) != 0)
          return set_error(dataset, B_ERR_DATA_CONFLICT, false);
        offset += entry.length;
        length -= entry.length;
        data = ((uint8_t*) data) + entry.length;
      } else {
        assert(conflict_mode == BTOEP_CONFLICT_OVERWRITE);
        // TODO
      }
    } else {
      assert(length == 0);
    }
  }

  return true;
}

bool btoep_data_read_range(btoep_dataset* dataset, btoep_range range, void* data, size_t* data_size) {
  // First, ensure that the given range exists.
  bool valid;
  if (!btoep_index_contains(dataset, range, &valid))
    return false;
  if (!valid)
    return set_error(dataset, B_ERR_READ_OUT_OF_BOUNDS, false);

  if (data_size != NULL) {
    if (range.length > *data_size)
      range.length = *data_size;
    *data_size = range.length;
  }

  while (range.length != 0) {
    size_t n_read = range.length;
    if (!btoep_data_read(dataset, range.offset, data, &n_read))
      return false;
    assert(n_read != 0);
    range.offset += n_read;
    range.length -= n_read;
    data = ((uint8_t*) data) + n_read;
  }

  return true;
}

bool btoep_data_read(btoep_dataset* dataset, uint64_t offset, void* data, size_t* length) {
  uint64_t size;
  if (!btoep_data_get_size(dataset, &size))
    return false;
  if (offset > size)
    return set_error(dataset, B_ERR_READ_OUT_OF_BOUNDS, false);
  if (!fd_seek(dataset, dataset->data_fd, offset, SEEK_SET, NULL))
    return false;
  return fd_read(dataset, dataset->data_fd, data, length);
}

bool btoep_data_get_size(btoep_dataset* dataset, uint64_t* size) {
  return fd_seek(dataset, dataset->data_fd, 0, SEEK_END, size);
}

static uint64_t max_u64 = -1;

bool btoep_data_set_size(btoep_dataset* dataset, uint64_t size, bool allow_destructive) {
  btoep_range relevant_range = { size, max_u64 - size };

  if (allow_destructive) {
    // This might not actually remove anything from the index, in which case the
    // action was not destructive.
    if (!btoep_index_remove(dataset, relevant_range))
      return false;
  } else {
    bool is_destructive;
    if (!btoep_index_contains_any(dataset, relevant_range, &is_destructive))
      return false;
    if (is_destructive)
      return set_error(dataset, B_ERR_DESTRUCTIVE_ACTION, false);
  }

  return ftruncate(dataset->data_fd, size) == 0;
}

static void btoep_index_cache_mark_dirty(btoep_dataset* dataset, btoep_range range) {
  if (dataset->index_cache_is_dirty) {
    dataset->index_cache_dirty_range =
        btoep_range_outer(dataset->index_cache_dirty_range, range);
  } else {
    dataset->index_cache_is_dirty = true;
    dataset->index_cache_dirty_range = range;
  }

  assert(btoep_range_is_subset(dataset->index_cache_range,
                               dataset->index_cache_dirty_range));
}

static bool btoep_index_resize(btoep_dataset* dataset, uint64_t new_size) {
  dataset->total_index_size = new_size;
  // TODO: Make this work without requiring the entire index to be in the cache
  dataset->index_cache_range.length = new_size - dataset->index_cache_range.offset;
  if (dataset->index_cache_is_dirty) {
    if (!btoep_range_intersect(&dataset->index_cache_dirty_range,
                               dataset->index_cache_range)) {
      dataset->index_cache_is_dirty = false;
    }
  }
  return true;
}

static bool btoep_set_index_fd_offset(btoep_dataset* dataset, uint64_t offset) {
  if (dataset->current_index_offset != offset) {
    if (!fd_seek(dataset, dataset->index_fd, offset, SEEK_SET, NULL))
      return false;
    dataset->current_index_offset = offset;
  }
  return true;
}

static bool btoep_read_index(btoep_dataset* dataset, uint64_t offset, uint8_t* dest, size_t min_length, size_t max_length, size_t* actual_length) {
  if (!btoep_set_index_fd_offset(dataset, offset))
    return false;
  *actual_length = max_length;
  // TODO: This might read less data. Fix that.
  if (!fd_read(dataset, dataset->index_fd, dest, actual_length))
    return false;
  assert(*actual_length != 0 || min_length == 0);
  if (*actual_length < min_length) // TODO: Think about this again
    return false; // TODO: Set an error
  dataset->current_index_offset = offset + *actual_length;
  return true;
}

/*
 * This function ensures that the given range is available in the cache. If it
 * is not, more data is read from the file.
 *
 * This function may succeed without filling the cache sufficiently if the file
 * itself does not contain more data.
 */
static bool btoep_index_fill_cache(btoep_dataset* dataset, btoep_range range) {
  btoep_range index_range = { 0, dataset->total_index_size };
  if (!btoep_range_intersect(&range, index_range))
    return true;

  // If the range is already in the cache, don't do anything.
  if (btoep_range_is_subset(dataset->index_cache_range, range))
    return true;

  assert(range.length <= BTOEP_INDEX_CACHE_SIZE);

  // Does the required range fit into the cache with its current offset?
  btoep_range max_range = { dataset->index_cache_range.offset, BTOEP_INDEX_CACHE_SIZE };
  if (!btoep_range_is_subset(range, max_range)) {
    // TODO: Keep the existing data in the cache (move it), don't just throw it away.
    if (!btoep_index_flush(dataset))
      return false;
    dataset->index_cache_range = btoep_mkrange(range.offset, 0);
  }

  size_t read_bytes;
  if (!btoep_read_index(dataset,
                        dataset->index_cache_range.offset + dataset->index_cache_range.length,
                        dataset->index_cache + dataset->index_cache_range.length,
                        range.length,
                        BTOEP_INDEX_CACHE_SIZE - dataset->index_cache_range.length,
                        &read_bytes))
    return false;
  dataset->index_cache_range.length += read_bytes;

  return true;
}

bool btoep_index_iterator_start(btoep_dataset* dataset) {
  dataset->iter_index_offset = 0;
  dataset->iter_data_offset = 0;
  return true;
}

// TODO: Make this function prettier
static inline bool btoep_index_uleb128(btoep_dataset* dataset, uint64_t* where, uint64_t* result) {
  assert(*where >= dataset->index_cache_range.offset);

  *result = 0;
  uint8_t byte;
  uint64_t exponent = 0;
  while (1) {
    if (*where >= dataset->index_cache_range.offset + dataset->index_cache_range.length)
      return set_error(dataset, B_ERR_INVALID_INDEX_FORMAT, false);
    byte = dataset->index_cache[(*where)++ - dataset->index_cache_range.offset];
    *result |= (byte & 0x7f) << exponent;
    if (byte & 0x80) {
      exponent += 7;
      if (exponent >= 56)
        return set_error(dataset, B_ERR_INVALID_INDEX_FORMAT, false);
    } else {
      break;
    }
  }

  return true;
}

// TODO: Make this function prettier
static inline void write_uleb128(uint8_t* out, uint64_t value, size_t* length) {
  do {
    *out = (value & 0x7f) | (value > 0x7f ? 0x80 : 0);
    value >>= 7;
    out++;
    (*length)++;
  } while (value > 0);
}

static bool btoep_index_read(btoep_dataset* dataset, uint64_t* next_offset, btoep_range* range) {
  btoep_range cache_range = { dataset->iter_index_offset, 1024 };
  if (!btoep_index_fill_cache(dataset, cache_range))
    return false;

  uint64_t missing_bytes, existing_bytes;
  *next_offset = dataset->iter_index_offset;
  if (!btoep_index_uleb128(dataset, next_offset, &missing_bytes))
    return false;
  if (!btoep_index_uleb128(dataset, next_offset, &existing_bytes))
    return false;

  int is_first = dataset->iter_data_offset == 0;
  if (is_first) {
    range->offset = missing_bytes;
  } else {
    range->offset = dataset->iter_data_offset + missing_bytes + 1;
  }

  range->length = existing_bytes + 1;

  return 1;
}

bool btoep_index_iterator_next(btoep_dataset* dataset, btoep_range* range) {
  uint64_t offset_in_index;
  bool ret = btoep_index_read(dataset, &offset_in_index, range);
  if (ret) {
    dataset->iter_index_offset = offset_in_index;
    dataset->iter_data_offset = range->offset + range->length;
  }
  return ret;
}

bool btoep_index_iterator_peek(btoep_dataset* dataset, btoep_range* range) {
  uint64_t offset_in_index;
  return btoep_index_read(dataset, &offset_in_index, range);
}

bool btoep_index_iterator_skip(btoep_dataset* dataset) {
  btoep_range range;
  return btoep_index_iterator_next(dataset, &range);
}

bool btoep_index_iterator_is_eof(btoep_dataset* dataset) {
  assert(dataset->iter_index_offset <= dataset->total_index_size);
  return dataset->iter_index_offset == dataset->total_index_size;
}

typedef struct {
  btoep_dataset* dataset;
  uint8_t* buffer;
  size_t insert_size;
  uint64_t prev_entry_end;
  uint64_t replace_start;
  uint64_t replace_length;
} index_editor;

static void editor_init(btoep_dataset* dataset, index_editor* editor, uint8_t* buffer) {
  editor->dataset = dataset;
  editor->buffer = buffer;
  editor->insert_size = 0;
  editor->replace_length = 0;
}

static void editor_set_start(index_editor* editor, uint64_t replace_start, uint64_t prev_entry_end) {
  editor->replace_start = replace_start;
  editor->prev_entry_end = prev_entry_end;
}

static void editor_set_end(index_editor* editor, uint64_t replace_end) {
  assert(editor->replace_start <= replace_end);
  editor->replace_length = replace_end - editor->replace_start;
}

static void editor_write_range(index_editor* editor, const btoep_range* range) {
  bool is_first = editor->prev_entry_end == 0;
  // TODO: Maybe handle length == 0 better? This would remove special handling from btoep-add.c
  assert(range->length > 0 && (range->offset != 0 || is_first));

  uint64_t relative_offset = range->offset - editor->prev_entry_end;
  if (!is_first)
    relative_offset--;
  write_uleb128(editor->buffer + editor->insert_size, relative_offset, &editor->insert_size);
  write_uleb128(editor->buffer + editor->insert_size, range->length - 1, &editor->insert_size);
  editor->prev_entry_end = range->offset + range->length;
}

static bool editor_commit(index_editor* editor) {
  // Now reassemble the index. This is a little tricky if it doesn't fit into the cache.
  btoep_dataset* dataset = editor->dataset;

  // TODO: Make this work if it does not fit into the cache.
  // TODO: Don't put everything into the cache if the size of the entries did not change.
  btoep_range cache_range = {
    .offset = editor->replace_start,
    .length = dataset->total_index_size - editor->replace_start
  };
  if (!btoep_index_fill_cache(dataset, cache_range))
    return false;

  uint64_t replace_end = editor->replace_start + editor->replace_length;

  memmove(dataset->index_cache + editor->replace_start + editor->insert_size,
          dataset->index_cache + replace_end,
          dataset->total_index_size - replace_end);
  memcpy(dataset->index_cache + editor->replace_start - dataset->index_cache_range.offset, editor->buffer, editor->insert_size);

  // Adapt the size of the index.
  uint64_t new_index_size = dataset->total_index_size + editor->insert_size - editor->replace_length;
  if (!btoep_index_resize(dataset, new_index_size))
    return false; // TODO: Mark the cache as corrupted

  // Make sure the changes in the cache will be written to disk eventually.
  // TODO: Make this work if the data does not fit into the cache
  cache_range.length = new_index_size - editor->replace_start;
  btoep_index_cache_mark_dirty(dataset, cache_range);

  return true;
}

// TODO: Avoid writing the same entry if a duplicate entry is added (just to avoid dirtying the cache)
bool btoep_index_add(btoep_dataset* dataset, btoep_range range) {
  if (!btoep_index_iterator_start(dataset))
    return false;

  btoep_range entry;

  index_editor editor;
  uint8_t editor_buffer[40];
  editor_init(dataset, &editor, editor_buffer);

  // First, skip all entries to the left of the new range.
  while (!btoep_index_iterator_is_eof(dataset)) {
    if (!btoep_index_iterator_peek(dataset, &entry))
      return false;
    if (entry.offset + entry.length >= range.offset)
      break;
    if (!btoep_index_iterator_skip(dataset))
      return false;
  }

  editor_set_start(&editor, dataset->iter_index_offset, dataset->iter_data_offset);

  // Next, find all entries that we can merge.
  while (!btoep_index_iterator_is_eof(dataset)) {
    if (!btoep_index_iterator_peek(dataset, &entry))
      return false;
    if (!btoep_range_union(&range, entry))
      break;
    if (!btoep_index_iterator_skip(dataset))
      return false;
  }

  // Create the new entry outside of the index.
  editor_write_range(&editor, &range);

  // If we are not at the end of the index yet, we will also need to modify the next entry.
  if (!btoep_index_iterator_is_eof(dataset)) {
    if (!btoep_index_iterator_next(dataset, &entry))
      return false;
    editor_write_range(&editor, &entry);
  }

  editor_set_end(&editor, dataset->iter_index_offset);
  return editor_commit(&editor);
}

bool btoep_index_remove(btoep_dataset* dataset, btoep_range range) {
  if (!btoep_index_iterator_start(dataset))
    return false;

  btoep_range entry;

  index_editor editor;
  uint8_t editor_buffer[40];
  editor_init(dataset, &editor, editor_buffer);

  // First, skip all entries to the left of the range that we need to delete.
  while (!btoep_index_iterator_is_eof(dataset)) {
    if (!btoep_index_iterator_peek(dataset, &entry))
      return false;
    if (entry.offset + entry.length >= range.offset)
      break;
    if (!btoep_index_iterator_skip(dataset))
      return false;
  }

  editor_set_start(&editor, dataset->iter_index_offset, dataset->iter_data_offset);

  while (!btoep_index_iterator_is_eof(dataset)) {
    if (!btoep_index_iterator_peek(dataset, &entry))
      return false;
    if (!btoep_range_overlaps(entry, range))
      break;
    if (!btoep_index_iterator_skip(dataset))
      return false;

    btoep_range right_part_of_split_entry;
    btoep_range_remove(&entry, &right_part_of_split_entry, range);
    if (entry.length != 0)
      editor_write_range(&editor, &entry); // This can only happen for the first entry, TOOD: assert that
    if (right_part_of_split_entry.length != 0)
      editor_write_range(&editor, &right_part_of_split_entry); // This can only happen for the last entry, TODO: assert that
  }

  // If we are not at the end of the index yet, we will also need to modify the next entry.
  // TODO: This is only necessary if we did not create a right part for the previous entry. Assert that.
  if (!btoep_index_iterator_is_eof(dataset)) {
    if (!btoep_index_iterator_next(dataset, &entry))
      return false;
    editor_write_range(&editor, &entry);
  }

  editor_set_end(&editor, dataset->iter_index_offset);
  return editor_commit(&editor);
}

bool btoep_index_contains(btoep_dataset* dataset, btoep_range range, bool* result) {
  if (range.length == 0) {
    uint64_t max_offset;
    // TODO: What if size < last index entry end?
    if (!btoep_data_get_size(dataset, &max_offset))
      return false;
    *result = range.offset <= max_offset;
    return true;
  }

  if (!btoep_index_iterator_start(dataset))
    return false;

  while (!btoep_index_iterator_is_eof(dataset)) {
    btoep_range entry;
    if (!btoep_index_iterator_next(dataset, &entry))
      return false;

    if (btoep_range_is_subset(entry, range)) {
      *result = true;
      return true;
    }

    if (entry.offset >= range.offset)
      break;
  }

  *result = false;
  return true;
}

bool btoep_index_contains_any(btoep_dataset* dataset, btoep_range range, bool* result) {
  if (!btoep_index_iterator_start(dataset))
    return false;

  while (!btoep_index_iterator_is_eof(dataset)) {
    btoep_range entry;
    if (!btoep_index_iterator_next(dataset, &entry))
      return false;

    if (btoep_range_intersect(&entry, range)) {
      *result = true;
      return true;
    }

    if (entry.offset >= range.offset + range.length)
      break;
  }

  *result = false;
  return true;
}

bool btoep_index_flush(btoep_dataset* dataset) {
  if (!dataset->index_cache_is_dirty)
    return true;

  if (!btoep_set_index_fd_offset(dataset, dataset->index_cache_dirty_range.offset))
    return false;

  if (dataset->total_index_size_on_disk != dataset->total_index_size) {
    if (ftruncate(dataset->index_fd, dataset->total_index_size) != 0)
      return set_io_error(dataset);
    dataset->total_index_size_on_disk = dataset->total_index_size;
  }

  if (!fd_write(dataset, dataset->index_fd,
                dataset->index_cache + dataset->index_cache_dirty_range.offset,
                dataset->index_cache_dirty_range.length))
    return false;

  dataset->index_cache_is_dirty = false;

  return true;
}
