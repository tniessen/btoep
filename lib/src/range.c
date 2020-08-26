#include "../include/btoep/range.h"

#define swap(a, b) do { (a) = (a) ^ (b); (b) = (a) ^ (b); a = (a) ^ (b); } while (0)

// TODO: Simplify this
// TODO: Make sure it treats empty ranges correctly
static bool range_extend(uint64_t* offset, uint64_t* length, uint64_t right, uint64_t r_length, bool must_overlap) {
  uint64_t left = *offset;
  uint64_t l_length = *length;

  if (left == right) {
    *length = l_length > r_length ? l_length : r_length;
    return true;
  }

  if (l_length == 0 || (r_length != 0 && left > right)) {
    swap(left, right);
    swap(l_length, r_length);
  }

  if (r_length == 0) {
    *offset = left;
    *length = l_length;
    return true;
  }

  if (must_overlap && left + l_length < right)
    return false;  // No overlap.

  *offset = left;
  *length = (right + r_length > left + l_length ? right + r_length : left + l_length) - left;
  return true;
}

bool btoep_range_union(btoep_range* out, btoep_range in) {
  return range_extend(&out->offset, &out->length, in.offset, in.length, true);
}

btoep_range btoep_range_outer(btoep_range out, btoep_range in) {
  range_extend(&out.offset, &out.length, in.offset, in.length, false);
  return out;
}

bool btoep_range_intersect(btoep_range* out, btoep_range in) {
  uint64_t small_offset = out->offset,
           small_length = out->length,
           large_offset = in.offset,
           large_length = in.length,
           small_end    = small_offset + small_length,
           large_end    = large_offset + large_length;

  // This might slip through the other checks.
  if (small_length == 0)
    return false;

  // Exclude offsets that are too small.
  if (small_offset < large_offset) {
    uint64_t difference = large_offset - small_offset;
    if (difference >= small_length)
      return false;  // No overlap
    small_length -= difference;
    small_offset = large_offset;
  }

  // Exclude offsets that are too large.
  if (small_end > large_end) {
    uint64_t difference = small_end - large_end;
    if (difference >= small_length)
      return false;  // No overlap
    small_length -= difference;
  }

  out->offset = small_offset;
  out->length = small_length;
  return true;
}

bool btoep_range_overlaps(btoep_range a, btoep_range b) {
  return a.length != 0 && b.length != 0 && (
           btoep_range_contains(a, b.offset) ||
           btoep_range_contains(a, b.offset + b.length - 1) ||
           btoep_range_contains(b, a.offset) ||
           btoep_range_contains(b, a.offset + a.length - 1)
         );
}

bool btoep_range_contains(btoep_range range, uint64_t offset) {
  return range.offset <= offset && offset < range.offset + range.length;
}

bool btoep_range_is_subset(btoep_range super, btoep_range sub) {
  return sub.length == 0 || (
           btoep_range_contains(super, sub.offset) &&
           btoep_range_contains(super, sub.offset + sub.length - 1)
         );
}

void btoep_range_remove(btoep_range* left_in, btoep_range* right, btoep_range remove) {
  if (btoep_range_intersect(&remove, *left_in)) {
    uint64_t old_length = left_in->length;
    left_in->length = remove.offset - left_in->offset;
    right->offset = remove.offset + remove.length;
    right->length = old_length - left_in->length - remove.length;
  } else {
    right->offset = left_in->offset + left_in->length;
    right->length = 0;
  }
}
