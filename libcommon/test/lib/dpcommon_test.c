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
#include "dpcommon_test.h"
#include "dpcommon/queue.h"
#include <dpcommon/common.h>
#include <dpcommon/input.h>
#include <dpcommon/output.h>


typedef struct DP_TestDestructor DP_TestDestructor;

struct DP_TestDestructor {
    void *value;
    void (*destroy)(void *);
    DP_TestDestructor *next;
};

typedef struct DP_TestState {
    void *initial_state;
    DP_TestDestructor *destructors;
} DP_TestState;


int setup(void **state)
{
    DP_TestState *ts = DP_malloc(sizeof(*ts));
    (*ts) = (DP_TestState){*state, NULL};
    *state = ts;
    return 0;
}

static void run_destructors(DP_TestState *ts)
{
    DP_TestDestructor *destructor = ts->destructors;
    while (destructor) {
        destructor->destroy(destructor->value);
        DP_TestDestructor *next = destructor->next;
        DP_free(destructor);
        destructor = next;
    }
}

int teardown(void **state)
{
    DP_TestState *ts = *state;
    run_destructors(ts);
    DP_free(ts);
    return 0;
}


void *initial_state(void **state)
{
    DP_TestState *ts = *state;
    return ts->initial_state;
}


void destructor_push(void **state, void *value, void (*destroy)(void *))
{
    DP_TestState *ts = *state;
    DP_TestDestructor *destructor = DP_malloc(sizeof(*destructor));
    *destructor = (DP_TestDestructor){value, destroy, ts->destructors};
    ts->destructors = destructor;
}

static void destructor_remove_with(void **state, void *value, bool run)
{
    DP_TestState *ts = *state;
    DP_TestDestructor **pp = &ts->destructors;
    while (*pp) {
        if ((*pp)->value == value) {
            DP_TestDestructor *destructor = *pp;
            *pp = destructor->next;
            if (run) {
                destructor->destroy(destructor->value);
            }
            DP_free(destructor);
            return;
        }
        else {
            pp = &(*pp)->next;
        }
    }
    DP_warn("No destructor found for %p", value);
}

void destructor_remove(void **state, void *value)
{
    destructor_remove_with(state, value, false);
}

void destructor_run(void **state, void *value)
{
    destructor_remove_with(state, value, true);
}


char *push_format(void **state, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char *buf = DP_vformat(fmt, ap);
    destructor_push(state, buf, DP_free);
    va_end(ap);
    return buf;
}

static void destroy_input(void *value)
{
    DP_input_free(value);
}

void push_input(void **state, DP_Input *value)
{
    destructor_push(state, value, destroy_input);
}

static void destroy_output(void *value)
{
    DP_output_free(value);
}

void push_output(void **state, DP_Output *value)
{
    destructor_push(state, value, destroy_output);
}

static void destroy_queue(void *value)
{
    DP_queue_dispose(value);
    DP_free(value);
}

void push_queue(void **state, DP_Queue *value)
{
    destructor_push(state, value, destroy_queue);
}


void _assert_files_equal(const char *a, const char *b, const char *file,
                         int line)
{
    size_t length_a;
    void *content_a = DP_slurp(a, &length_a);
    if (!content_a) {
        print_error("Can't read '%s': %s", a, DP_error());
        _fail(file, line);
        DP_UNREACHABLE();
    }

    size_t length_b;
    char *content_b = DP_slurp(b, &length_b);
    if (!content_b) {
        DP_free(content_a);
        print_error("Can't read '%s': %s", b, DP_error());
        _fail(file, line);
        DP_UNREACHABLE();
    }

    bool equal =
        length_a == length_b && memcmp(content_a, content_b, length_a) == 0;
    DP_free(content_a);
    DP_free(content_b);

    char buf[1024];
    snprintf(buf, sizeof(buf), "assert_files_equal(\"%s\", \"%s\")", a, b);
    _assert_true(cast_to_largest_integral_type(equal), buf, file, line);
}
