/*
 * Copyright (c) 2022 askmeaboutloom
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <dpcommon/common.h>
#include <dpcommon/geom.h>
#include <dpcommon_test.h>


static void assert_rect(DP_Rect rect, int x, int y, int width, int height)
{
    assert_true(DP_rect_valid(rect));
    char *got = DP_format("x1=%d, x2=%d, y1=%d, y2=%d, width=%d, height=%d",
                          rect.x1, rect.x2, rect.y1, rect.y2,
                          DP_rect_width(rect), DP_rect_height(rect));
    char *want = DP_format("x1=%d, x2=%d, y1=%d, y2=%d, width=%d, height=%d", x,
                           x + width - 1, y, y + height - 1, width, height);
    assert_string_equal(got, want);
    DP_free(want);
    DP_free(got);
}


static void make_rect(DP_UNUSED void **state)
{
    DP_Rect rect = DP_rect_make(0, 3, 1, 2);
    assert_rect(rect, 0, 3, 1, 2);
}

static void make_empty_rect(DP_UNUSED void **state)
{
    DP_Rect empty = DP_rect_make(0, 0, 0, 0);
    assert_false(DP_rect_valid(empty));
}


static void union_separate_rects(DP_UNUSED void **state)
{
    assert_rect(
        DP_rect_union(DP_rect_make(0, 1, 10, 11), DP_rect_make(20, 21, 10, 11)),
        0, 1, 30, 31);
}

static void union_intersecting_rects(DP_UNUSED void **state)
{
    assert_rect(
        DP_rect_union(DP_rect_make(10, 15, 20, 1), DP_rect_make(12, 5, 1, 50)),
        10, 5, 20, 50);
}

static void union_small_rect_in_larger_rect(DP_UNUSED void **state)
{
    assert_rect(
        DP_rect_union(DP_rect_make(0, 50, 100, 200), DP_rect_make(1, 53, 2, 4)),
        0, 50, 100, 200);
}


static void intersect_same_rect(DP_UNUSED void **state)
{
    DP_Rect rect = DP_rect_make(0, 10, 5, 50);
    assert_true(DP_rect_intersects(rect, rect));
}

static void intersect_separate_rects(DP_UNUSED void **state)
{
    assert_false(DP_rect_intersects(DP_rect_make(0, 1, 10, 11),
                                    DP_rect_make(20, 21, 10, 11)));
}

static void intersect_intersecting_rects(DP_UNUSED void **state)
{
    assert_true(DP_rect_intersects(DP_rect_make(10, 15, 20, 1),
                                   DP_rect_make(12, 5, 1, 50)));
}

static void intersect_small_rect_in_larger_rect(DP_UNUSED void **state)
{
    assert_true(DP_rect_intersects(DP_rect_make(0, 50, 100, 200),
                                   DP_rect_make(1, 53, 2, 4)));
}

static void intersect_adjacent_rects_x(DP_UNUSED void **state)
{
    assert_false(DP_rect_intersects(DP_rect_make(0, 50, 100, 60),
                                    DP_rect_make(100, 50, 150, 60)));
}

static void intersect_adjacent_rects_y(DP_UNUSED void **state)
{
    assert_false(DP_rect_intersects(DP_rect_make(50, 0, 60, 100),
                                    DP_rect_make(50, 100, 60, 150)));
}

static void intersect_barely_intersecting_rects_x(DP_UNUSED void **state)
{
    assert_true(DP_rect_intersects(DP_rect_make(0, 50, 100, 60),
                                   DP_rect_make(99, 50, 150, 60)));
}

static void intersect_barely_intersecting_rects_y(DP_UNUSED void **state)
{
    assert_true(DP_rect_intersects(DP_rect_make(50, 1, 60, 100),
                                   DP_rect_make(50, 100, 60, 150)));
}


int main(void)
{
    const struct CMUnitTest tests[] = {
        dp_unit_test(make_rect),
        dp_unit_test(make_empty_rect),
        dp_unit_test(union_separate_rects),
        dp_unit_test(union_intersecting_rects),
        dp_unit_test(union_small_rect_in_larger_rect),
        dp_unit_test(intersect_same_rect),
        dp_unit_test(intersect_separate_rects),
        dp_unit_test(intersect_intersecting_rects),
        dp_unit_test(intersect_small_rect_in_larger_rect),
        dp_unit_test(intersect_adjacent_rects_x),
        dp_unit_test(intersect_adjacent_rects_y),
        dp_unit_test(intersect_barely_intersecting_rects_x),
        dp_unit_test(intersect_barely_intersecting_rects_y),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
