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
#include <dpcommon/geom.h>
#include <dptest.h>


static void rect_ok(TEST_PARAMS, DP_Rect rect, int x, int y, int width,
                    int height, const char *title)
{
    OK(DP_rect_valid(rect), "%s valid", title);
    char *got = DP_format("x1=%d, x2=%d, y1=%d, y2=%d, width=%d, height=%d",
                          rect.x1, rect.x2, rect.y1, rect.y2,
                          DP_rect_width(rect), DP_rect_height(rect));
    char *want = DP_format("x1=%d, x2=%d, y1=%d, y2=%d, width=%d, height=%d", x,
                           x + width - 1, y, y + height - 1, width, height);
    STR_EQ_OK(got, want, "%s correct values", title);
    DP_free(want);
    DP_free(got);
}


static void rect_make(TEST_PARAMS)
{
    DP_Rect rect = DP_rect_make(0, 3, 1, 2);
    rect_ok(TEST_ARGS, rect, 0, 3, 1, 2, "normal rect");

    DP_Rect empty = DP_rect_make(0, 0, 0, 0);
    NOK(DP_rect_valid(empty), "empty rect is not valid");

    DP_Rect fits = DP_rect_make(1, 1, INT_MAX, INT_MAX);
    OK(DP_rect_valid(fits), "Rect with INT_MAX dimensions valid");

    DP_Rect overflow = DP_rect_make(9999, 2, INT_MAX, INT_MAX);
    NOK(DP_rect_valid(overflow), "Rect beyond INT_MAX dimensions not valid");
}


static void rect_unions(TEST_PARAMS)
{
    rect_ok(
        TEST_ARGS,
        DP_rect_union(DP_rect_make(0, 1, 10, 11), DP_rect_make(20, 21, 10, 11)),
        0, 1, 30, 31, "union of separate rects");

    rect_ok(
        TEST_ARGS,
        DP_rect_union(DP_rect_make(10, 15, 20, 1), DP_rect_make(12, 5, 1, 50)),
        10, 5, 20, 50, "union of overlapping rects");

    rect_ok(
        TEST_ARGS,
        DP_rect_union(DP_rect_make(0, 50, 100, 200), DP_rect_make(1, 53, 2, 4)),
        0, 50, 100, 200, "union of smaller in larger rect");
}


static void rect_intersections(TEST_PARAMS)
{
    DP_Rect rect = DP_rect_make(0, 10, 5, 50);
    OK(DP_rect_intersects(rect, rect), "same rect intersects");

    NOK(DP_rect_intersects(DP_rect_make(0, 1, 10, 11),
                           DP_rect_make(20, 21, 10, 11)),
        "separate rects don't intersect");

    OK(DP_rect_intersects(DP_rect_make(10, 15, 20, 1),
                          DP_rect_make(12, 5, 1, 50)),
       "overlapping rects intersect");

    OK(DP_rect_intersects(DP_rect_make(0, 50, 100, 200),
                          DP_rect_make(1, 53, 2, 4)),
       "larger in smaller rect intersects");

    NOK(DP_rect_intersects(DP_rect_make(0, 50, 100, 60),
                           DP_rect_make(100, 50, 150, 60)),
        "horizontally directly adjacent rects don't intersect");

    NOK(DP_rect_intersects(DP_rect_make(50, 0, 60, 100),
                           DP_rect_make(50, 100, 60, 150)),
        "vertically directly adjacent rects don't intersect");

    OK(DP_rect_intersects(DP_rect_make(0, 50, 100, 60),
                          DP_rect_make(99, 50, 150, 60)),
       "horizontally barely overlapping rects intersect");

    OK(DP_rect_intersects(DP_rect_make(50, 1, 60, 100),
                          DP_rect_make(50, 100, 60, 150)),
       "vertically barely overlapping rects intersect");
}


static void register_tests(REGISTER_PARAMS)
{
    REGISTER_TEST(rect_make);
    REGISTER_TEST(rect_unions);
    REGISTER_TEST(rect_intersections);
}

int main(int argc, char **argv)
{
    DP_test_main(argc, argv, register_tests, NULL);
}
