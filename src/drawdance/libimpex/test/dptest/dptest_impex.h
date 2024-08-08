// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPTEST_DPTEST_IMPEX_H
#define DPTEST_DPTEST_IMPEX_H
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
