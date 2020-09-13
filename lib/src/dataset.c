#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../include/btoep/dataset.h"

#ifndef _MSC_VER
# include <errno.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <unistd.h>
#endif

static bool set_last_error_info(btoep_dataset* dataset, int error_code,
                                const char* func, bool system_error,
                                const char* system_func) {
  dataset->last_error.code = error_code;
  dataset->last_error.func = func;
  if (system_error) {
#ifdef _MSC_VER
    dataset->last_error.system_error_code = GetLastError();
#else
    dataset->last_error.system_error_code = errno;
#endif
  } else {
    dataset->last_error.system_error_code = 0;
  }
  dataset->last_error.system_func = system_func;
  return false;
}

#define set_error(dataset, error) \
  set_last_error_info(dataset, error, __func__, false, NULL)
#define set_io_error(dataset, system_func) \
  set_last_error_info(dataset, B_ERR_INPUT_OUTPUT, __func__, true, system_func)

static bool fd_open(btoep_dataset* dataset, btoep_fd* fd, btoep_path path,
                    int mode) {
  assert(mode <= B_CREATE_NEW_READ_WRITE);
#ifdef _MSC_VER
  DWORD dwDesiredAccess = GENERIC_READ;
  if (mode != B_OPEN_EXISTING_READ_ONLY)
    dwDesiredAccess |= GENERIC_WRITE;
  DWORD dwCreationDisposition = OPEN_EXISTING;
  if (mode == B_CREATE_NEW_READ_WRITE)
    dwCreationDisposition = CREATE_NEW;
  *fd = CreateFile(path, dwDesiredAccess, 0, NULL, dwCreationDisposition,
                   FILE_ATTRIBUTE_NORMAL, NULL);
  if (*fd == INVALID_HANDLE_VALUE)
    return set_io_error(dataset, "CreateFile");
#else
  int flag = (mode == B_OPEN_EXISTING_READ_ONLY) ? O_RDONLY : O_RDWR;
  if (mode & (B_CREATE_NEW_READ_WRITE & B_OPEN_OR_CREATE_READ_WRITE))
    flag |= O_CREAT | O_EXCL;
  *fd = open(path, flag, S_IRUSR | S_IWUSR);
  if (*fd == -1)
    return set_io_error(dataset, "open");
#endif
  return true;
}

static bool fd_open_or_create(btoep_dataset* dataset, btoep_fd* fd, btoep_path path, bool* created) {
  *created = fd_open(dataset, fd, path, B_CREATE_NEW_READ_WRITE);
  if (!*created) {
#ifdef _MSC_VER
    if (GetLastError() == ERROR_FILE_EXISTS) {
#else
    if (errno == EEXIST) {
#endif
      // There is an unfortunate, but almost unavoidable race condition here.
      // We know that the previous call failed since the file already exists, so
      // now we will attempt to open the existing file. However, if it was
      // deleted since the previous call, this will fail as well, and we cannot
      // really recover from that. On the other hand, we also cannot add O_CREAT
      // or the equivalent on Windows in the second attempt, since that would
      // lead to the file being created without our knowledge.
      // TODO: At least on Windows, it should be possible to avoid the race
      // condition by using OPEN_ALWAYS and checking the last-error code.
      return fd_open(dataset, fd, path, B_OPEN_EXISTING_READ_WRITE);
    }
  }
  return *created;
}

static bool fd_seek(btoep_dataset* dataset, btoep_fd fd, uint64_t offset, int whence, uint64_t* out) {
#ifdef _MSC_VER
  // TODO: Prevent overflow since QuadPart is signed (but only if whence != FILE_BEGIN)
  LARGE_INTEGER liDistanceToMove = { .QuadPart = offset };
  LARGE_INTEGER liNewFilePointer;
  if (!SetFilePointerEx(fd, liDistanceToMove, &liNewFilePointer, whence))
    return set_io_error(dataset, "SetFilePointerEx");
  if (out != NULL)
    *out = liNewFilePointer.QuadPart; // TODO: What about signedness?
#else
  off_t ret = lseek(fd, offset, whence);
  if (ret == -1)
    return set_io_error(dataset, "lseek");
  if (out != NULL)
    *out = ret;
#endif
  return true;
}

static bool fd_truncate(btoep_dataset* dataset, btoep_fd fd, uint64_t size) {
#ifdef _MSC_VER
  // Windows doesn't have an equivalent to ftruncate. We need to remember the
  // current position within the file, change it to the desired end position,
  // set the file length to the file pointer, and then restore the previous file
  // pointer.
  LARGE_INTEGER prevPosition = { .QuadPart = 0 };
  if (!SetFilePointerEx(fd, prevPosition, &prevPosition, FILE_CURRENT))
    return false;
  LARGE_INTEGER newPosition = { .QuadPart = size }; // TODO: Signedness
  if (!SetFilePointerEx(fd, newPosition, NULL, FILE_BEGIN))
    return set_io_error(dataset, "SetFilePointerEx");
  if (!SetEndOfFile(fd))
    return set_io_error(dataset, "SetEndOfFile");
  if (!SetFilePointerEx(fd, prevPosition, NULL, FILE_BEGIN))
    return set_io_error(dataset, "SetFilePointerEx");
#else
  if (ftruncate(fd, size) != 0)
    return set_io_error(dataset, "ftruncate");
#endif
  return true;
}

#ifdef _MSC_VER
static inline DWORD limit_dword(size_t sz) {
  return (sz < MAXDWORD) ? (DWORD) sz : MAXDWORD;
}
#endif

static bool fd_read(btoep_dataset* dataset, btoep_fd fd, void* out, size_t* n_read) {
#ifdef _MSC_VER
  DWORD count = limit_dword(*n_read);
  if (!ReadFile(fd, out, count, &count, NULL))
    return set_io_error(dataset, "ReadFile");
  *n_read = count;
#else
  ssize_t ret = read(fd, out, *n_read);
  if (ret == -1)
    return set_io_error(dataset, "read");
  *n_read = ret;
#endif
  return true;
}

static bool fd_write(btoep_dataset* dataset, btoep_fd fd, const void* data, size_t length) {
  assert(!dataset->read_only);

  const uint8_t* bytes = data;
  while (length > 0) {
#ifdef _MSC_VER
    DWORD written;
    if (!WriteFile(fd, data, limit_dword(length), &written, NULL))
      return set_io_error(dataset, "WriteFile");
#else
    ssize_t written = write(fd, bytes, length);
    if (written == -1)
      return set_io_error(dataset, "write");
    assert(written >= 0 && (size_t) written <= length);
#endif
    bytes += written;
    length -= written;
  }
  return true;
}

static bool fd_close(btoep_dataset* dataset, btoep_fd fd) {
#ifdef _MSC_VER
  if (!CloseHandle(fd))
    return set_io_error(dataset, "CloseHandle");
#else
  if (close(fd) != 0)
    return set_io_error(dataset, "close");
#endif
  return true;
}

static bool copy_path(char* out, const char* in, const char* def, const char* ext) {
  int n = (in == NULL) ? snprintf(out, OS_MAX_PATH, "%s%s", def, ext)
                       : snprintf(out, OS_MAX_PATH, "%s", in);
  assert(n >= 0);
  return n < OS_MAX_PATH;
}

static bool btoep_lock(btoep_dataset* dataset) {
#ifdef _MSC_VER
  HANDLE hFile = CreateFile(dataset->lock_path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile == INVALID_HANDLE_VALUE) {
    if (GetLastError() == ERROR_FILE_EXISTS)
      return set_error(dataset, B_ERR_DATASET_LOCKED);
    return set_io_error(dataset, "CreateFile");
  }
  CloseHandle(hFile); // TODO: Check return value
#else
  int fd = open(dataset->lock_path, O_CREAT | O_EXCL | O_WRONLY, 0000);
  if (fd == -1) {
    if (errno == EEXIST)
      return set_error(dataset, B_ERR_DATASET_LOCKED);
    return set_io_error(dataset, "open");
  }
  close(fd); // TODO: Check return value
#endif
  return true;
}

static bool btoep_unlock(btoep_dataset* dataset) {
#ifdef _MSC_VER
  return DeleteFile(dataset->lock_path) || set_io_error(dataset, "DeleteFile");
#else
  return unlink(dataset->lock_path) == 0 || set_io_error(dataset, "unlink");
#endif
}

static bool open_dataset_fds(btoep_dataset* dataset, int mode) {
  bool data_file_created;

  if (mode == B_OPEN_OR_CREATE_READ_WRITE) {
    if (!fd_open_or_create(dataset, &dataset->data_fd, dataset->data_path,
                           &data_file_created))
      return false;
    mode = data_file_created ? B_CREATE_NEW_READ_WRITE :
                               B_OPEN_EXISTING_READ_WRITE;
  } else {
    if (!fd_open(dataset, &dataset->data_fd, dataset->data_path, mode))
      return false;
    data_file_created = (mode == B_CREATE_NEW_READ_WRITE);
  }

  if (fd_open(dataset, &dataset->index_fd, dataset->index_path, mode))
    return true;

  fd_close(dataset, dataset->data_fd); // TODO: Return value
  if (data_file_created) {
    // We created the data file, but failed to create the index file.
    assert(mode == B_CREATE_NEW_READ_WRITE);
#ifdef _MSC_VER
    DeleteFile(dataset->data_path); // TODO: Return value
#else
    unlink(dataset->data_path); // TODO: Return value
#endif
  }
  return false;
}

bool btoep_open(btoep_dataset* dataset, btoep_path data_path,
                btoep_path index_path, btoep_path lock_path, int mode) {
  if (dataset == NULL || data_path == NULL ||
      !copy_path(dataset->data_path, data_path, NULL, NULL) ||
      !copy_path(dataset->index_path, index_path, data_path, ".idx") ||
      !copy_path(dataset->lock_path, lock_path, data_path, ".lck")) {
    return set_error(dataset, B_ERR_INVALID_ARGUMENT);
  }

  if (!btoep_lock(dataset))
    return false;

  dataset->read_only = (mode == B_OPEN_EXISTING_READ_ONLY);
  if (!open_dataset_fds(dataset, mode)) {
    btoep_unlock(dataset); // TODO: Return value
    return false;
  }

  // Determine the index size.
  if (!fd_seek(dataset, dataset->index_fd, 0, SEEK_END, &dataset->total_index_size_on_disk) ||
      !fd_seek(dataset, dataset->index_fd, 0, SEEK_SET, NULL)) {
    // TODO: Return values
    fd_close(dataset, dataset->data_fd);
    fd_close(dataset, dataset->index_fd);
    btoep_unlock(dataset);
    return false;
  }

  dataset->total_index_size = dataset->total_index_size_on_disk;
  dataset->current_index_offset = 0;

  // This is merely to prevent iterators from being used with the wrong dataset,
  // and is a best-effort way to give different datasets very different index
  // revision counters. Shifting the random value to the left means that
  // subsequently incrementing the revision counter is unlikely to cause
  // collisions.
  dataset->index_rev = ((uint64_t) rand()) << (64 - 8 * sizeof(int));

  dataset->index_cache_range = btoep_mkrange(0, 0);
  dataset->index_cache_is_dirty = false;

  return true;
}

bool btoep_close(btoep_dataset* dataset) {
  if (!btoep_index_flush(dataset))
    return false;

  // TODO: Return values
  fd_close(dataset, dataset->data_fd);
  fd_close(dataset, dataset->index_fd);
  btoep_unlock(dataset);
  return true;
}

void btoep_last_error(btoep_dataset* dataset, btoep_last_error_info* info) {
  memcpy(info, &dataset->last_error, sizeof(btoep_last_error_info));
}

const char* btoep_strerror(int error_code) {
  switch (error_code) {
  case B_ERR_INPUT_OUTPUT:         return "System input/output error";
  case B_ERR_DATASET_LOCKED:       return "Dataset locked by another process";
  case B_ERR_SIZE_TOO_SMALL:       return "Size too small to contain data";
  case B_ERR_INVALID_INDEX_FORMAT: return "Invalid index format";
  case B_ERR_DATA_CONFLICT:        return "Data conflicts with existing data";
  case B_ERR_READ_OUT_OF_BOUNDS:   return "Read out of bounds";
  case B_ERR_INVALID_ARGUMENT:     return "Invalid argument";
  case B_ERR_DEAD_INDEX_ITERATOR:  return "Index iterator is too old";
  default:                         return NULL;
  }
}

const char* btoep_strerror_name(int error_code) {
  switch (error_code) {
  case B_ERR_INPUT_OUTPUT:         return "ERR_INPUT_OUTPUT";
  case B_ERR_DATASET_LOCKED:       return "ERR_DATASET_LOCKED";
  case B_ERR_SIZE_TOO_SMALL:       return "ERR_SIZE_TOO_SMALL";
  case B_ERR_INVALID_INDEX_FORMAT: return "ERR_INVALID_INDEX_FORMAT";
  case B_ERR_DATA_CONFLICT:        return "ERR_DATA_CONFLICT";
  case B_ERR_READ_OUT_OF_BOUNDS:   return "ERR_READ_OUT_OF_BOUNDS";
  case B_ERR_INVALID_ARGUMENT:     return "ERR_INVALID_ARGUMENT";
  case B_ERR_DEAD_INDEX_ITERATOR:  return "ERR_DEAD_INDEX_ITERATOR";
  default:                         return NULL;
  }
}

bool btoep_data_add_range(btoep_dataset* dataset, btoep_range range, const void* data, int conflict_mode) {
  return btoep_data_write(dataset, range, data, range.length, conflict_mode) &&
         btoep_index_add(dataset, range);
}

bool btoep_data_write(btoep_dataset* dataset, btoep_range range, const void* data, size_t data_size, int conflict_mode) {
  if (dataset->read_only)
    return set_error(dataset, B_ERR_DATASET_READ_ONLY);

  btoep_index_iterator iterator;
  if (!btoep_index_iterator_start(dataset, &iterator))
    return false;

  if (data_size < range.length)
    range.length = data_size;

  btoep_range entry;

  if (!fd_seek(dataset, dataset->data_fd, range.offset, SEEK_SET, NULL))
    return false;

  const uint8_t* remaining_data = data;
  while (range.length != 0) {
    // Try to find an index entry that covers at least some area after the start
    // of the remaining data.
    while (!btoep_index_iterator_is_eof(&iterator)) {
      if (!btoep_index_iterator_peek(&iterator, &entry))
        return false;
      if (btoep_range_intersect(&entry, range))
        break;
      if (!btoep_index_iterator_skip(&iterator))
        return false;
    }

    // If no entry exists, we can write the rest of the data.
    // If an entry exists, we can write up to the entry.
    uint64_t safe_length = range.length;
    if (!btoep_index_iterator_is_eof(&iterator))
      safe_length = entry.offset - range.offset;

    if (!fd_write(dataset, dataset->data_fd, remaining_data, safe_length))
      return false;

    range = btoep_range_remove_left(range, safe_length);
    remaining_data += safe_length;

    if (!btoep_index_iterator_is_eof(&iterator)) {
      // This is existing data.
      if (conflict_mode == BTOEP_CONFLICT_KEEP_OLD) {
        // Simply ignore the data and skip ahead.
        // TODO: Fail if the data does not exist because the file is too short?
        if (!fd_seek(dataset, dataset->data_fd, entry.length, SEEK_CUR, NULL))
          return false;
      } else if (conflict_mode == BTOEP_CONFLICT_ERROR) {
        // Ensure the data is the same.
        uint8_t buf[8 * 1024];
        size_t n_read = entry.length; // TODO: Loop etc.
        if (!fd_read(dataset, dataset->data_fd, buf, &n_read))
          return false;
        if (memcmp(buf, remaining_data, n_read) != 0)
          return set_error(dataset, B_ERR_DATA_CONFLICT);
      } else {
        assert(conflict_mode == BTOEP_CONFLICT_OVERWRITE);
        if (!fd_write(dataset, dataset->data_fd, remaining_data, entry.length))
          return false;
      }
      range = btoep_range_remove_left(range, entry.length);
      remaining_data += entry.length;
    } else {
      assert(range.length == 0);
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
    return set_error(dataset, B_ERR_READ_OUT_OF_BOUNDS);

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
    range = btoep_range_remove_left(range, n_read);
    data = ((uint8_t*) data) + n_read;
  }

  return true;
}

bool btoep_data_read(btoep_dataset* dataset, uint64_t offset, void* data, size_t* length) {
  uint64_t size;
  if (!btoep_data_get_size(dataset, &size))
    return false;
  if (offset > size)
    return set_error(dataset, B_ERR_READ_OUT_OF_BOUNDS);
  if (!fd_seek(dataset, dataset->data_fd, offset, SEEK_SET, NULL))
    return false;
  return fd_read(dataset, dataset->data_fd, data, length);
}

bool btoep_data_get_size(btoep_dataset* dataset, uint64_t* size) {
  return fd_seek(dataset, dataset->data_fd, 0, SEEK_END, size);
}

bool btoep_data_set_size(btoep_dataset* dataset, uint64_t size, bool allow_destructive) {
  if (dataset->read_only)
    return set_error(dataset, B_ERR_DATASET_READ_ONLY);

  btoep_range relevant_range = btoep_max_range_from(size);

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
      return set_error(dataset, B_ERR_SIZE_TOO_SMALL);
  }

  return fd_truncate(dataset, dataset->data_fd, size);
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

bool btoep_index_iterator_start(btoep_dataset* dataset, btoep_index_iterator* iterator) {
  iterator->index_offset = 0;
  iterator->data_offset = 0;
  iterator->dataset = dataset;
  iterator->index_rev = dataset->index_rev;
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
      return set_error(dataset, B_ERR_INVALID_INDEX_FORMAT);
    byte = dataset->index_cache[(*where)++ - dataset->index_cache_range.offset];
    *result |= (uint64_t) (byte & 0x7f) << exponent;
    if (byte & 0x80) {
      exponent += 7;
      if (exponent >= 56)
        return set_error(dataset, B_ERR_INVALID_INDEX_FORMAT);
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

static bool btoep_index_read(btoep_index_iterator* iterator, uint64_t* next_offset, btoep_range* range) {
  if (iterator->index_rev != iterator->dataset->index_rev)
    return set_error(iterator->dataset, B_ERR_DEAD_INDEX_ITERATOR);

  btoep_range cache_range = { iterator->index_offset, 1024 };
  if (!btoep_index_fill_cache(iterator->dataset, cache_range))
    return false;

  uint64_t missing_bytes, existing_bytes;
  *next_offset = iterator->index_offset;
  if (!btoep_index_uleb128(iterator->dataset, next_offset, &missing_bytes))
    return false;
  if (!btoep_index_uleb128(iterator->dataset, next_offset, &existing_bytes))
    return false;

  int is_first = iterator->data_offset == 0;
  if (is_first) {
    range->offset = missing_bytes;
  } else {
    range->offset = iterator->data_offset + missing_bytes + 1;
  }

  range->length = existing_bytes + 1;

  return 1;
}

bool btoep_index_iterator_next(btoep_index_iterator* iterator, btoep_range* range) {
  uint64_t offset_in_index;
  bool ret = btoep_index_read(iterator, &offset_in_index, range);
  if (ret) {
    iterator->index_offset = offset_in_index;
    iterator->data_offset = range->offset + range->length;
  }
  return ret;
}

bool btoep_index_iterator_peek(btoep_index_iterator* iterator, btoep_range* range) {
  uint64_t offset_in_index;
  return btoep_index_read(iterator, &offset_in_index, range);
}

bool btoep_index_iterator_skip(btoep_index_iterator* iterator) {
  btoep_range range;
  return btoep_index_iterator_next(iterator, &range);
}

bool btoep_index_iterator_is_eof(btoep_index_iterator* iterator) {
  assert(iterator->index_offset <= iterator->dataset->total_index_size);
  return iterator->index_offset == iterator->dataset->total_index_size;
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

  // Prevent existing iterators from being used.
  dataset->index_rev++;

  return true;
}

// TODO: Avoid writing the same entry if a duplicate entry is added (just to avoid dirtying the cache)
bool btoep_index_add(btoep_dataset* dataset, btoep_range range) {
  if (dataset->read_only)
    return set_error(dataset, B_ERR_DATASET_READ_ONLY);

  btoep_index_iterator iterator;
  if (!btoep_index_iterator_start(dataset, &iterator))
    return false;

  btoep_range entry;

  index_editor editor;
  uint8_t editor_buffer[40];
  editor_init(dataset, &editor, editor_buffer);

  // First, skip all entries to the left of the new range.
  while (!btoep_index_iterator_is_eof(&iterator)) {
    if (!btoep_index_iterator_peek(&iterator, &entry))
      return false;
    if (entry.offset + entry.length >= range.offset)
      break;
    if (!btoep_index_iterator_skip(&iterator))
      return false;
  }

  editor_set_start(&editor, iterator.index_offset, iterator.data_offset);

  // Next, find all entries that we can merge.
  while (!btoep_index_iterator_is_eof(&iterator)) {
    if (!btoep_index_iterator_peek(&iterator, &entry))
      return false;
    if (!btoep_range_union(&range, entry))
      break;
    if (!btoep_index_iterator_skip(&iterator))
      return false;
  }

  // Create the new entry outside of the index.
  editor_write_range(&editor, &range);

  // If we are not at the end of the index yet, we will also need to modify the next entry.
  if (!btoep_index_iterator_is_eof(&iterator)) {
    if (!btoep_index_iterator_next(&iterator, &entry))
      return false;
    editor_write_range(&editor, &entry);
  }

  editor_set_end(&editor, iterator.index_offset);
  return editor_commit(&editor);
}

bool btoep_index_remove(btoep_dataset* dataset, btoep_range range) {
  if (dataset->read_only)
    return set_error(dataset, B_ERR_DATASET_READ_ONLY);

  btoep_index_iterator iterator;
  if (!btoep_index_iterator_start(dataset, &iterator))
    return false;

  btoep_range entry;

  index_editor editor;
  uint8_t editor_buffer[40];
  editor_init(dataset, &editor, editor_buffer);

  // First, skip all entries to the left of the range that we need to delete.
  while (!btoep_index_iterator_is_eof(&iterator)) {
    if (!btoep_index_iterator_peek(&iterator, &entry))
      return false;
    if (entry.offset + entry.length >= range.offset)
      break;
    if (!btoep_index_iterator_skip(&iterator))
      return false;
  }

  editor_set_start(&editor, iterator.index_offset, iterator.data_offset);

  while (!btoep_index_iterator_is_eof(&iterator)) {
    if (!btoep_index_iterator_peek(&iterator, &entry))
      return false;
    if (!btoep_range_overlaps(entry, range))
      break;
    if (!btoep_index_iterator_skip(&iterator))
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
  if (!btoep_index_iterator_is_eof(&iterator)) {
    if (!btoep_index_iterator_next(&iterator, &entry))
      return false;
    editor_write_range(&editor, &entry);
  }

  editor_set_end(&editor, iterator.index_offset);
  return editor_commit(&editor);
}

bool btoep_index_find_offset(btoep_dataset* dataset, uint64_t start, int mode,
                             bool* exists, uint64_t* offset) {
  btoep_index_iterator iterator;
  if (!btoep_index_iterator_start(dataset, &iterator))
    return false;

  btoep_range range;
  while (!btoep_index_iterator_is_eof(&iterator)) {
    if (!btoep_index_iterator_next(&iterator, &range))
      return false;
    if (range.offset > start) {
      *offset = (mode == BTOEP_FIND_DATA) ? range.offset : start;
      *exists = true;
      return true;
    } else if (btoep_range_contains(range, start)) {
      *offset = (mode == BTOEP_FIND_DATA) ? start : range.offset + range.length;
      *exists = true;
      return true;
    }
  }

  if ((*exists = (mode == BTOEP_FIND_NO_DATA)))
    *offset = start;

  return true;
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

  btoep_index_iterator iterator;
  if (!btoep_index_iterator_start(dataset, &iterator))
    return false;

  while (!btoep_index_iterator_is_eof(&iterator)) {
    btoep_range entry;
    if (!btoep_index_iterator_next(&iterator, &entry))
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
  btoep_index_iterator iterator;
  if (!btoep_index_iterator_start(dataset, &iterator))
    return false;

  while (!btoep_index_iterator_is_eof(&iterator)) {
    btoep_range entry;
    if (!btoep_index_iterator_next(&iterator, &entry))
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
    if (!fd_truncate(dataset, dataset->index_fd, dataset->total_index_size))
      return false;
    dataset->total_index_size_on_disk = dataset->total_index_size;
  }

  if (!fd_write(dataset, dataset->index_fd,
                dataset->index_cache + dataset->index_cache_dirty_range.offset,
                dataset->index_cache_dirty_range.length))
    return false;

  dataset->index_cache_is_dirty = false;

  return true;
}
