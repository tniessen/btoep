#include "test.h"

#include <btoep/range.h>

static void test_union(void) {
  btoep_range a, b;

  // Overlapping (right side)
  a.offset = 50;
  a.length = 100;
  b.offset = 75;
  b.length = 200;
  assert(btoep_range_union(&a, &b));
  assert(a.offset == 50 && a.length == 225);

  // a is superset of b
  assert(btoep_range_union(&a, &b));
  assert(a.offset == 50 && a.length == 225);

  // b is superset of a
  b.offset = 40;
  b.length = 240;
  assert(btoep_range_union(&a, &b));
  assert(a.offset == 40 && a.length == 240);

  // Adjacent (right side)
  b.offset = 280;
  b.length = 25;
  assert(btoep_range_union(&a, &b));
  assert(a.offset == 40 && a.length == 265);

  // Adjacent (left side)
  b.offset = 25;
  b.length = 15;
  assert(btoep_range_union(&a, &b));
  assert(a.offset == 25 && a.length == 280);

  // Overlapping (left side)
  b.offset = 5;
  b.length = 100;
  assert(btoep_range_union(&a, &b));
  assert(a.offset == 5 && a.length == 300);

  // Test empty ranges (should always succeed)
  for (uint64_t i = 0; i < 400; i++) {
    b.offset = i;
    b.length = 0;
    assert(btoep_range_union(&a, &b));
    assert(a.offset == 5 && a.length == 300);
    assert(btoep_range_union(&b, &a));
    assert(b.offset == 5 && b.length == 300);
  }

  // Other ranges should fail
  b.offset = 0;
  b.length = 4;
  assert(!btoep_range_union(&a, &b));
  b.offset = 306;
  b.length = 10;
  assert(!btoep_range_union(&a, &b));

  // Both empty should succeed, and return some empty range
  a.offset = 5;
  a.length = 0;
  b.offset = 10;
  b.length = 0;
  assert(btoep_range_union(&a, &b));
  assert(a.length == 0);
}

static void test_outer(void) {
  btoep_range a, b;

  // Overlapping (right side)
  a.offset = 50;
  a.length = 100;
  b.offset = 75;
  b.length = 200;
  btoep_range_outer(&a, &b);
  assert(a.offset == 50 && a.length == 225);

  // a is superset of b
  btoep_range_outer(&a, &b);
  assert(a.offset == 50 && a.length == 225);

  // b is superset of a
  b.offset = 40;
  b.length = 240;
  btoep_range_outer(&a, &b);
  assert(a.offset == 40 && a.length == 240);

  // Adjacent (right side)
  b.offset = 280;
  b.length = 25;
  btoep_range_outer(&a, &b);
  assert(a.offset == 40 && a.length == 265);

  // Adjacent (left side)
  b.offset = 25;
  b.length = 15;
  btoep_range_outer(&a, &b);
  assert(a.offset == 25 && a.length == 280);

  // Overlapping (left side)
  b.offset = 5;
  b.length = 100;
  btoep_range_outer(&a, &b);
  assert(a.offset == 5 && a.length == 300);

  // Test empty ranges (should always succeed)
  for (uint64_t i = 0; i < 400; i++) {
    b.offset = i;
    b.length = 0;
    btoep_range_outer(&a, &b);
    assert(a.offset == 5 && a.length == 300);
    btoep_range_outer(&b, &a);
    assert(b.offset == 5 && b.length == 300);
  }

  // Other ranges should produce larger ranges
  b.offset = 0;
  b.length = 4;
  btoep_range_outer(&a, &b);
  assert(a.offset == 0 && a.length == 305);
  b.offset = 306;
  b.length = 10;
  btoep_range_outer(&a, &b);
  assert(a.offset == 0 && a.length == 316);

  // Both empty should succeed, and return some empty range
  a.offset = 5;
  a.length = 0;
  b.offset = 10;
  b.length = 0;
  btoep_range_outer(&a, &b);
  assert(a.length == 0);
}

static void test_intersect(void) {
  btoep_range a, b;

  a.offset = 10;
  a.length = 20;
  b.offset = 30;
  b.length = 0;
  assert(!btoep_range_intersect(&a, &b));

  b.offset = 15;
  b.length = 0;
  assert(!btoep_range_intersect(&a, &b));

  b.offset = 15;
  b.length = 1;
  assert(btoep_range_intersect(&a, &b));
  assert(a.offset == 15);
  assert(a.length == 1);

  a.offset = 10;
  a.length = 20;
  b.offset = 30;
  b.length = 10;
  assert(!btoep_range_intersect(&a, &b));

  a.offset = 20;
  assert(btoep_range_intersect(&a, &b));
  assert(a.offset == 30);
  assert(a.length == 10);

  b.offset = 20;
  b.length = 0;
  assert(!btoep_range_intersect(&a, &b));

  a.offset = 30;
  a.length = 0;
  b.offset = 10;
  b.length = 90;
  assert(!btoep_range_intersect(&b, &a));
  assert(!btoep_range_intersect(&a, &b));
}

static void test_overlaps(void) {
  btoep_range a, b;

  a.offset = 10;
  a.length = 20;
  b = a;
  assert(btoep_range_overlaps(&a, &b));

  b.offset = 29;
  assert(btoep_range_overlaps(&a, &b));
  assert(btoep_range_overlaps(&b, &a));

  b.offset = 30;
  assert(!btoep_range_overlaps(&a, &b));
  assert(!btoep_range_overlaps(&b, &a));

  b.offset = 10;
  b.length = 0;
  assert(!btoep_range_overlaps(&a, &b));
  assert(!btoep_range_overlaps(&b, &a));
}

static void test_contains(void) {
  btoep_range range;

  range.offset = 10;
  range.length = 5;
  assert(!btoep_range_contains(&range, 0));
  assert(!btoep_range_contains(&range, 9));
  assert(btoep_range_contains(&range, 10));
  assert(btoep_range_contains(&range, 14));
  assert(!btoep_range_contains(&range, 15));

  range.length = 0;
  assert(!btoep_range_contains(&range, 0));
  assert(!btoep_range_contains(&range, 9));
  assert(!btoep_range_contains(&range, 10));
  assert(!btoep_range_contains(&range, 14));
  assert(!btoep_range_contains(&range, 15));
}

static void test_is_subset(void) {
  btoep_range super, sub;

  super.offset = 10;
  super.length = 90;
  sub = super;
  assert(btoep_range_is_subset(&super, &sub));

  sub.offset = 20;
  sub.length = 70;
  assert(btoep_range_is_subset(&super, &sub));

  sub.length = 81;
  assert(!btoep_range_is_subset(&super, &sub));

  sub.offset = 9;
  sub.length = 5;
  assert(!btoep_range_is_subset(&super, &sub));

  sub.length = 0;
  assert(btoep_range_is_subset(&super, &sub));

  sub.offset = 1000000;
  assert(btoep_range_is_subset(&super, &sub));
}

static void test_remove(void) {
  btoep_range left;
  btoep_range right;

  btoep_range remove = { 50, 25 };

  // Original range is to the left: remove nothing
  left.offset = 10;
  left.length = 40;
  btoep_range_remove(&left, &right, &remove);
  assert(left.offset == 10);
  assert(left.length == 40);
  assert(right.length == 0);

  // Original range is to the right: remove nothing
  left.offset = 75;
  left.length = 40;
  btoep_range_remove(&left, &right, &remove);
  assert(left.offset == 75);
  assert(left.length == 40);
  assert(right.length == 0);

  // Ranges equal: remove everything
  left = remove;
  btoep_range_remove(&left, &right, &remove);
  assert(left.length == 0);
  assert(right.length == 0);

  // Original range is subset: remove everything
  left.offset = 60;
  left.length = 5;
  btoep_range_remove(&left, &right, &remove);
  assert(left.length == 0);
  assert(right.length == 0);

  // Original range overlaps on the left side
  left.offset = 10;
  left.length = 50;
  btoep_range_remove(&left, &right, &remove);
  assert(left.offset == 10);
  assert(left.length == 40);
  assert(right.length == 0);

  // Original range overlaps on the right side
  left.offset = 70;
  left.length = 10;
  btoep_range_remove(&left, &right, &remove);
  assert(left.length == 0);
  assert(right.offset == 75);
  assert(right.length == 5);

  // Original range is superset (and remove != empty): split into left and right
  left.offset = 10;
  left.length = 90;
  btoep_range_remove(&left, &right, &remove);
  assert(left.offset == 10);
  assert(left.length == 40);
  assert(right.offset == 75);
  assert(right.length == 25);

  // Empty range: remove nothing
  left.offset = 10;
  left.length = 20;
  remove.offset = 15;
  remove.length = 0;
  btoep_range_remove(&left, &right, &remove);
  assert(left.offset == 10);
  assert(left.length == 20);
  assert(right.offset == 30);
  assert(right.length == 0);
}

static void test_range(void) {
  test_union();
  test_outer();
  test_intersect();
  test_overlaps();
  test_contains();
  test_is_subset();
  test_remove();
}

TEST_MAIN(test_range)
