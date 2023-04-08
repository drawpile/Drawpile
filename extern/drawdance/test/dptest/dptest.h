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
#ifndef DPTEST_DPTEST_H
#define DPTEST_DPTEST_H
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <stdio.h>


typedef struct DP_Test DP_Test;

typedef struct DP_TestRegistry {
    int used;
    int capacity;
    DP_Test *tests;
    void *user;
} DP_TestRegistry;

typedef struct DP_TestContext {
    DP_Test *test;
    const char *lb;
    const char *rb;
    FILE *out;
    FILE *err;
    long long total;
    long long passed;
    void *user;
} DP_TestContext;

typedef void (*DP_TestRegisterFn)(DP_TestRegistry *);
typedef void (*DP_TestFn)(DP_TestContext *);

struct DP_Test {
    char *name;
    DP_TestFn test_fn;
    void *user;
};


int DP_test_main(int argc, char **argv, DP_TestRegisterFn register_fn,
                 void *user);

void DP_test_register(DP_TestRegistry *R, const char *name, DP_TestFn test_fn,
                      void *user);


void DP_test_note(DP_TestContext *T, const char *fmt, ...) DP_FORMAT(2, 3);

void DP_test_diag(DP_TestContext *T, const char *fmt, ...) DP_FORMAT(2, 3);


bool DP_test_vok(DP_TestContext *T, const char *file, int line, bool value,
                 const char *fmt, va_list ap);

bool DP_test_ok(DP_TestContext *T, const char *file, int line, bool value,
                const char *fmt, ...) DP_FORMAT(5, 6);

bool DP_test_pass(DP_TestContext *T, const char *file, int line,
                  const char *fmt, ...) DP_FORMAT(4, 5);

bool DP_test_fail(DP_TestContext *T, const char *file, int line,
                  const char *fmt, ...) DP_FORMAT(4, 5);

bool DP_test_int_eq_ok(DP_TestContext *T, const char *file, int line,
                       const char *sa, const char *sb, long long a, long long b,
                       const char *fmt, ...) DP_FORMAT(8, 9);

bool DP_test_uint_eq_ok(DP_TestContext *T, const char *file, int line,
                        const char *sa, const char *sb, unsigned long long a,
                        unsigned long long b, const char *fmt, ...)
    DP_FORMAT(8, 9);

bool DP_test_str_eq_ok(DP_TestContext *T, const char *file, int line,
                       const char *sa, const char *sb, const char *a,
                       const char *b, const char *fmt, ...) DP_FORMAT(8, 9);

bool DP_test_str_len_eq_ok(DP_TestContext *T, const char *file, int line,
                           const char *sa, const char *salen, const char *sb,
                           const char *sblen, const char *a, size_t alen,
                           const char *b, size_t blen, const char *fmt, ...)
    DP_FORMAT(12, 13);

bool DP_test_ptr_eq_ok(DP_TestContext *T, const char *file, int line,
                       const char *sa, const char *sb, const void *a,
                       const void *b, const char *fmt, ...) DP_FORMAT(8, 9);

bool DP_test_not_null_ok(DP_TestContext *T, const char *file, int line,
                         const char *sa, const void *a, const char *fmt, ...)
    DP_FORMAT(6, 7);

bool DP_test_null_ok(DP_TestContext *T, const char *file, int line,
                     const char *sa, const void *a, const char *fmt, ...)
    DP_FORMAT(6, 7);

bool DP_test_file_eq_ok(DP_TestContext *T, const char *file, int line,
                        const char *sa, const char *sb, const char *a,
                        const char *b, const char *fmt, ...) DP_FORMAT(8, 9);


#define REGISTER_PARAMS DP_TestRegistry *R
#define TEST_PARAMS     DP_TestContext *T
#define REGISTER_ARGS   R
#define TEST_ARGS       T

// For sticking TEST_PARAMS into a struct, e.g. to have them in callbacks.
#define TEST_FIELD_DECL DP_TestContext *T
#define TEST_FIELD_INIT T
#define TEST_FIELD_PACK(X) \
    do {                   \
        (X).T = T;         \
    } while (0)
#define TEST_UNPACK(X) \
    TEST_FIELD_DECL;   \
    do {               \
        T = (X).T;     \
    } while (0)

#define REGISTER_TEST_USER(NAME, USER) \
    DP_test_register(REGISTER_ARGS, #NAME, NAME, USER)

#define REGISTER_TEST(NAME) REGISTER_TEST_USER(NAME, NULL)

#define FATAL(EXPR)                                                           \
    do {                                                                      \
        if (!(EXPR)) {                                                        \
            DP_test_diag(                                                     \
                TEST_ARGS, "Fatal failure in test %s (%s line %d), aborting", \
                T->test->name, &__FILE__[DP_PROJECT_DIR_LENGTH], __LINE__);   \
            DP_TRAP();                                                        \
        }                                                                     \
    } while (0)

#define NOTE(...) DP_test_note(TEST_ARGS, __VA_ARGS__)
#define DIAG(...) DP_test_diag(TEST_ARGS, __VA_ARGS__)

#define DIAG_VALUE(TITLE, P, SX, ...) \
    DIAG("%s (%s):\n%s" P "%s", TITLE, SX, T->lb, __VA_ARGS__, T->rb);

#define DIAG_ACTUAL_EXPECTED(P, SA, SB, A, B) \
    do {                                      \
        DIAG_VALUE("actual", P, SA, A);       \
        DIAG_VALUE("expected", P, SB, B);     \
    } while (0)

#define OK(VALUE, ...) \
    DP_test_ok(TEST_ARGS, __FILE__, __LINE__, VALUE, __VA_ARGS__)

#define NOK(VALUE, ...) OK(!(VALUE), __VA_ARGS__)

#define PASS(...) DP_test_pass(TEST_ARGS, __FILE__, __LINE__, __VA_ARGS__)
#define FAIL(...) DP_test_fail(TEST_ARGS, __FILE__, __LINE__, __VA_ARGS__)

#define INT_EQ_OK(A, B, ...)                                           \
    DP_test_int_eq_ok(TEST_ARGS, __FILE__, __LINE__, #A, #B, (A), (B), \
                      __VA_ARGS__)

#define UINT_EQ_OK(A, B, ...)                                           \
    DP_test_uint_eq_ok(TEST_ARGS, __FILE__, __LINE__, #A, #B, (A), (B), \
                       __VA_ARGS__)

#define STR_EQ_OK(A, B, ...)                                           \
    DP_test_str_eq_ok(TEST_ARGS, __FILE__, __LINE__, #A, #B, (A), (B), \
                      __VA_ARGS__)

#define STR_LEN_EQ_OK(A, ALEN, B, BLEN, ...)                                   \
    DP_test_str_len_eq_ok(TEST_ARGS, __FILE__, __LINE__, #A, #ALEN, #B, #BLEN, \
                          (A), (ALEN), (B), (BLEN), __VA_ARGS__)

#define PTR_EQ_OK(A, B, ...)                                           \
    DP_test_ptr_eq_ok(TEST_ARGS, __FILE__, __LINE__, #A, #B, (A), (B), \
                      __VA_ARGS__)

#define NOT_NULL_OK(A, ...) \
    DP_test_not_null_ok(TEST_ARGS, __FILE__, __LINE__, #A, (A), __VA_ARGS__)

#define NULL_OK(A, ...) \
    DP_test_null_ok(TEST_ARGS, __FILE__, __LINE__, #A, (A), __VA_ARGS__)

#define FILE_EQ_OK(A, B, ...)                                           \
    DP_test_file_eq_ok(TEST_ARGS, __FILE__, __LINE__, #A, #B, (A), (B), \
                       __VA_ARGS__)


#endif
