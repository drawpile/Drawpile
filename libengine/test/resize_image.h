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
#define WIDTH  64
#define HEIGHT 32

typedef struct ResizeTestCase {
    const char *name;
    int x, y, dw, dh;
} ResizeTestCase;

ResizeTestCase resize_test_cases[] = {
    {"same", 0, 0, 0, 0},
    {"negative_x", -20, 0, 0, 0},
    {"positive_x", 20, 0, 0, 0},
    {"negative_y", 0, -10, 0, 0},
    {"positive_y", 0, 10, 0, 0},
    {"smaller_width", 0, 0, -16, 0},
    {"larger_width", 0, 0, 16, 0},
    {"smaller_height", 0, 0, 0, -8},
    {"larger_height", 0, 10, 0, 8},
    {"everything", -20, 10, 50, -12},
    {"out_of_bounds", -WIDTH, -HEIGHT, 0, 0},
};
