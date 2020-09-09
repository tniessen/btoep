#include "test.h"

#include <btoep/range.h>

static void test_max_range_from(void) {
  btoep_range a = btoep_max_range_from(1000);
  assert(a.offset == 1000 && a.length == 18446744073709550615llu);
}

static void test_union(void) {
  btoep_range a, b;

  // Overlapping (right side)
  a = btoep_mkrange(50, 100);
  b = btoep_mkrange(75, 200);
  assert(btoep_range_union(&a, b));
  assert(a.offset == 50 && a.length == 225);

  // a is superset of b
  assert(btoep_range_union(&a, b));
  assert(a.offset == 50 && a.length == 225);

  // b is superset of a
  b = btoep_mkrange(40, 240);
  assert(btoep_range_union(&a, b));
  assert(a.offset == 40 && a.length == 240);

  // Adjacent (right side)
  b = btoep_mkrange(280, 25);
  assert(btoep_range_union(&a, b));
  assert(a.offset == 40 && a.length == 265);

  // Adjacent (left side)
  b = btoep_mkrange(25, 15);
  assert(btoep_range_union(&a, b));
  assert(a.offset == 25 && a.length == 280);

  // Overlapping (left side)
  b = btoep_mkrange(5, 100);
  assert(btoep_range_union(&a, b));
  assert(a.offset == 5 && a.length == 300);

  // Test empty ranges (should always succeed)
  for (uint64_t i = 0; i < 400; i++) {
    b = btoep_mkrange(i, 0);
    assert(btoep_range_union(&a, b));
    assert(a.offset == 5 && a.length == 300);
    assert(btoep_range_union(&b, a));
    assert(b.offset == 5 && b.length == 300);
  }

  // Other ranges should fail
  b = btoep_mkrange(0, 4);
  assert(!btoep_range_union(&a, b));
  b = btoep_mkrange(306, 10);
  assert(!btoep_range_union(&a, b));

  // Both empty should succeed, and return some empty range
  a = btoep_mkrange(5, 0);
  b = btoep_mkrange(10, 0);
  assert(btoep_range_union(&a, b));
  assert(a.length == 0);
}

static void test_outer(void) {
  btoep_range a, b;

  // Overlapping (right side)
  a = btoep_mkrange(50, 100);
  b = btoep_mkrange(75, 200);
  a = btoep_range_outer(a, b);
  assert(a.offset == 50 && a.length == 225);

  // a is superset of b
  a = btoep_range_outer(a, b);
  assert(a.offset == 50 && a.length == 225);

  // b is superset of a
  b = btoep_mkrange(40, 240);
  a = btoep_range_outer(a, b);
  assert(a.offset == 40 && a.length == 240);

  // Adjacent (right side)
  b = btoep_mkrange(280, 25);
  a = btoep_range_outer(a, b);
  assert(a.offset == 40 && a.length == 265);

  // Adjacent (left side)
  b = btoep_mkrange(25, 15);
  a = btoep_range_outer(a, b);
  assert(a.offset == 25 && a.length == 280);

  // Overlapping (left side)
  b = btoep_mkrange(5, 100);
  a = btoep_range_outer(a, b);
  assert(a.offset == 5 && a.length == 300);

  // Test empty ranges (should always succeed)
  for (uint64_t i = 0; i < 400; i++) {
    b = btoep_mkrange(i, 0);
    a = btoep_range_outer(a, b);
    assert(a.offset == 5 && a.length == 300);
    b = btoep_range_outer(b, a);
    assert(b.offset == 5 && b.length == 300);
  }

  // Other ranges should produce larger ranges
  b = btoep_mkrange(0, 4);
  a = btoep_range_outer(a, b);
  assert(a.offset == 0 && a.length == 305);
  b = btoep_mkrange(306, 10);
  a = btoep_range_outer(a, b);
  assert(a.offset == 0 && a.length == 316);

  // Both empty should succeed, and return some empty range
  a = btoep_mkrange(5, 0);
  b = btoep_mkrange(10, 0);
  a = btoep_range_outer(a, b);
  assert(a.length == 0);
}

static void test_intersect(void) {
  btoep_range a, b;

  a = btoep_mkrange(10, 20);
  b = btoep_mkrange(30, 0);
  assert(!btoep_range_intersect(&a, b));

  b = btoep_mkrange(15, 0);
  assert(!btoep_range_intersect(&a, b));

  b = btoep_mkrange(15, 1);
  assert(btoep_range_intersect(&a, b));
  assert(a.offset == 15);
  assert(a.length == 1);

  a = btoep_mkrange(10, 20);
  b = btoep_mkrange(30, 10);
  assert(!btoep_range_intersect(&a, b));

  a.offset = 20;
  assert(btoep_range_intersect(&a, b));
  assert(a.offset == 30);
  assert(a.length == 10);

  b = btoep_mkrange(20, 0);
  assert(!btoep_range_intersect(&a, b));

  a = btoep_mkrange(30, 0);
  b = btoep_mkrange(10, 90);
  assert(!btoep_range_intersect(&b, a));
  assert(!btoep_range_intersect(&a, b));
}

static void test_overlaps(void) {
  btoep_range a, b;

  a = b = btoep_mkrange(10, 20);
  assert(btoep_range_overlaps(a, b));

  b = btoep_mkrange(29, 20);
  assert(btoep_range_overlaps(a, b));
  assert(btoep_range_overlaps(b, a));

  b = btoep_mkrange(30, 20);
  assert(!btoep_range_overlaps(a, b));
  assert(!btoep_range_overlaps(b, a));

  b = btoep_mkrange(10, 0);
  assert(!btoep_range_overlaps(a, b));
  assert(!btoep_range_overlaps(b, a));
}

static void test_contains(void) {
  btoep_range range;

  range = btoep_mkrange(10, 5);
  assert(!btoep_range_contains(range, 0));
  assert(!btoep_range_contains(range, 9));
  assert(btoep_range_contains(range, 10));
  assert(btoep_range_contains(range, 14));
  assert(!btoep_range_contains(range, 15));

  range = btoep_mkrange(10, 0);
  assert(!btoep_range_contains(range, 0));
  assert(!btoep_range_contains(range, 9));
  assert(!btoep_range_contains(range, 10));
  assert(!btoep_range_contains(range, 14));
  assert(!btoep_range_contains(range, 15));
}

static void test_is_subset(void) {
  btoep_range super, sub;

  sub = super = btoep_mkrange(10, 90);
  assert(btoep_range_is_subset(super, sub));

  sub = btoep_mkrange(20, 70);
  assert(btoep_range_is_subset(super, sub));

  sub = btoep_mkrange(20, 81);
  assert(!btoep_range_is_subset(super, sub));

  sub = btoep_mkrange(9, 5);
  assert(!btoep_range_is_subset(super, sub));

  sub = btoep_mkrange(9, 0);
  assert(btoep_range_is_subset(super, sub));

  sub = btoep_mkrange(1000000, 0);
  assert(btoep_range_is_subset(super, sub));
}

static void test_remove(void) {
  btoep_range left;
  btoep_range right;

  btoep_range remove = { 50, 25 };

  // Original range is to the left: remove nothing
  left = btoep_mkrange(10, 40);
  btoep_range_remove(&left, &right, remove);
  assert(left.offset == 10 && left.length == 40);
  assert(right.length == 0);

  // Original range is to the right: remove nothing
  left = btoep_mkrange(75, 40);
  btoep_range_remove(&left, &right, remove);
  assert(left.offset == 75 && left.length == 40);
  assert(right.length == 0);

  // Ranges equal: remove everything
  left = remove;
  btoep_range_remove(&left, &right, remove);
  assert(left.length == 0);
  assert(right.length == 0);

  // Original range is subset: remove everything
  left = btoep_mkrange(60, 5);
  btoep_range_remove(&left, &right, remove);
  assert(left.length == 0);
  assert(right.length == 0);

  // Original range overlaps on the left side
  left = btoep_mkrange(10, 50);
  btoep_range_remove(&left, &right, remove);
  assert(left.offset == 10 && left.length == 40);
  assert(right.length == 0);

  // Original range overlaps on the right side
  left = btoep_mkrange(70, 10);
  btoep_range_remove(&left, &right, remove);
  assert(left.length == 0);
  assert(right.offset == 75 && right.length == 5);

  // Original range is superset (and remove != empty): split into left and right
  left = btoep_mkrange(10, 90);
  btoep_range_remove(&left, &right, remove);
  assert(left.offset == 10 && left.length == 40);
  assert(right.offset == 75);
  assert(right.length == 25);

  // Empty range: remove nothing
  left = btoep_mkrange(10, 20);
  remove = btoep_mkrange(15, 0);
  btoep_range_remove(&left, &right, remove);
  assert(left.offset == 10 && left.length == 20);
  assert(right.offset == 30 && right.length == 0);
}

static void test_remove_left(void) {
  btoep_range a;

  a = btoep_range_remove_left(btoep_mkrange(20, 10), 0);
  assert(a.offset == 20 && a.length == 10);

  a = btoep_range_remove_left(btoep_mkrange(20, 10), 5);
  assert(a.offset == 25 && a.length == 5);

  a = btoep_range_remove_left(btoep_mkrange(20, 10), 10);
  assert(a.offset == 30 && a.length == 0);
}

static void test_range(void) {
  test_max_range_from();
  test_union();
  test_outer();
  test_intersect();
  test_overlaps();
  test_contains();
  test_is_subset();
  test_remove();
  test_remove_left();
}

TEST_MAIN(test_range)
