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
#ifndef DPTEST_DPTEST_ENGINE_H
#define DPTEST_DPTEST_ENGINE_H
#include <dptest.h>

typedef struct DP_Image DP_Image;


bool DP_test_image_eq_ok(DP_TestContext *T, const char *file, int line,
                         const char *sa, const char *sb, DP_Image *a,
                         DP_Image *b, const char *fmt, ...) DP_FORMAT(8, 9);

bool DP_test_image_file_eq_ok(DP_TestContext *T, const char *file, int line,
                              const char *sa, const char *sb, const char *a,
                              const char *b, const char *fmt, ...)
    DP_FORMAT(8, 9);


#define IMAGE_EQ_OK(A, B, ...)                                           \
    DP_test_image_eq_ok(TEST_ARGS, __FILE__, __LINE__, #A, #B, (A), (B), \
                        __VA_ARGS__)

#define IMAGE_FILE_EQ_OK(A, B, ...)                                           \
    DP_test_image_file_eq_ok(TEST_ARGS, __FILE__, __LINE__, #A, #B, (A), (B), \
                             __VA_ARGS__)


#endif
