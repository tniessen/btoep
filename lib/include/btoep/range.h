#ifndef __BTOEP__RANGE_H__
#define __BTOEP__RANGE_H__

#include <stdbool.h>
#include <stdint.h>

/*
 * A range (offset, length) is the set of 64-bit integers i such that
 * i >= offset and i < offset + length. (This does not match the usual
 * mathematical definition of a range, which consists of start and end.)
 *
 * If the length is zero, the range is empty.
 *
 * The only case in which the below structure does not represent a proper range
 * is when offset + length >= 2**64.
 */
typedef struct {
  uint64_t offset;
  uint64_t length;
} btoep_range;

static inline btoep_range btoep_mkrange(uint64_t offset, uint64_t length) {
  // TODO: Assert that offset + length does not overflow
  btoep_range range = { offset, length };
  return range;
}

static inline btoep_range btoep_max_range_from(uint64_t offset) {
  return btoep_mkrange(offset, ((uint64_t) -1) - offset);
}

/*
 * For ranges A and B, calculate the union of A and B. This fails if A and B
 * are neither adjacent nor overlapping.
 *
 * If either range is empty, the other range is returned.
 */
bool btoep_range_union(btoep_range* out, btoep_range in);

/*
 * For ranges A and B, calculate the smallest range that contains all elements
 * from A and all elements from B.
 *
 * If either range is empty, the other range is returned.
 */
btoep_range btoep_range_outer(btoep_range a, btoep_range b);

/*
 * For ranges A and B, calculate the intersection of A and B, that is, the
 * largest range that contains only elements that are in both A and B.
 *
 * If the result would be empty, e.g., because either range was empty, this
 * function returns false, and the result is unmodified.
 */
bool btoep_range_intersect(btoep_range* out, btoep_range in);

/*
 * Checks whether ranges A and B overlap, meaning that either range contains
 * at least one element of the other range.
 *
 * If either range is empty, this returns false.
 */
bool btoep_range_overlaps(btoep_range a, btoep_range b);

/*
 * Checks whether the given range contains the given offset.
 */
bool btoep_range_contains(btoep_range range, uint64_t offset);

/*
 * Checks whether range A is a superset of range B.
 *
 * If B is empty, this always returns true, regardless of the offset of B.
 */
bool btoep_range_is_subset(btoep_range super, btoep_range sub);

/*
 * For a range L and a range X, this calculates L minus X. This can produce two
 * distinct ranges.
 */
void btoep_range_remove(btoep_range* left_in, btoep_range* right, btoep_range remove);

/*
 * Removes the left n values from a given range. If the length of the range is
 * less than n, the behavior is undefined.
 */
btoep_range btoep_range_remove_left(btoep_range in, uint64_t n);

#endif  // __BTOEP__RANGE_H__
