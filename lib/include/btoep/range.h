#ifndef __BTOEP__RANGE_H__
#define __BTOEP__RANGE_H__

#include <stdbool.h>
#include <stdint.h>

// TODO: Consider switching to pass-by-value whenever possible.

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

/*
 * For ranges A and B, calculate the union of A and B. This fails if A and B
 * are neither adjacent nor overlapping.
 *
 * If either range is empty, the other range is returned.
 */
bool btoep_range_union(btoep_range* out, const btoep_range* in);

/*
 * For ranges A and B, calculate the smallest range that contains all elements
 * from A and all elements from B.
 *
 * If either range is empty, the other range is returned.
 */
void btoep_range_outer(btoep_range* out, const btoep_range* in);

/*
 * For ranges A and B, calculate the intersection of A and B, that is, the
 * largest range that contains only elements that are in both A and B.
 *
 * If the result would be empty, e.g., because either range was empty, this
 * function returns false, and the result is unmodified.
 */
bool btoep_range_intersect(btoep_range* out, const btoep_range* in);

/*
 * Checks whether ranges A and B overlap, meaning that either range contains
 * at least one element of the other range.
 *
 * If either range is empty, this returns false.
 */
bool btoep_range_overlaps(const btoep_range* a, const btoep_range* b);

/*
 * Checks whether the given range contains the given offset.
 */
bool btoep_range_contains(const btoep_range* range, uint64_t offset);

/*
 * Checks whether range A is a superset of range B.
 *
 * If B is empty, this always returns true, regardless of the offset of B.
 */
bool btoep_range_is_subset(const btoep_range* super, const btoep_range* sub);

/*
 * For a range L and a range X, this calculates L minus X. This can produce two
 * distinct ranges.
 */
void btoep_range_remove(btoep_range* left_in, btoep_range* right, const btoep_range* remove);

#endif  // __BTOEP__RANGE_H__
