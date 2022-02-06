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
#ifndef DPCOMMON_TEST_H
#define DPCOMMON_TEST_H
#include <dpcommon/common.h>
#include <setjmp.h> // IWYU pragma: keep
#include <stdarg.h> // IWYU pragma: keep
#include <stdio.h>  // IWYU pragma: keep
#include <cmocka.h> // IWYU pragma: export
// IWYU pragma: no_include "cmocka.h"

typedef struct DP_Input DP_Input;
typedef struct DP_Output DP_Output;
typedef struct DP_Queue DP_Queue;


#define dp_unit_test_prestate(TEST, STATE) \
    (struct CMUnitTest)                    \
        cmocka_unit_test_prestate_setup_teardown(TEST, setup, teardown, STATE)

#define dp_unit_test(TEST) dp_unit_test_prestate(TEST, NULL)


int setup(void **state);

int teardown(void **state);


void *initial_state(void **state);


void destructor_push(void **state, void *value, void (*destroy)(void *));

void destructor_remove(void **state, void *value);

void destructor_run(void **state, void *value);


char *push_format(void **state, const char *fmt, ...) DP_FORMAT(2, 3);

void push_input(void **state, DP_Input *value);

void push_output(void **state, DP_Output *value);

void push_queue(void **state, DP_Queue *value);


#define assert_files_equal(a, b) _assert_files_equal(a, b, __FILE__, __LINE__)

void _assert_files_equal(const char *a, const char *b, const char *file,
                         int line);


#endif
