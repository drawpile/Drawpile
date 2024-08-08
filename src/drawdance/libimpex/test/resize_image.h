// SPDX-License-Identifier: GPL-3.0-or-later
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
